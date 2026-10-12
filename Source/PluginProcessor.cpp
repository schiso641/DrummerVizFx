#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

DrummerVizProcessor::DrummerVizProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool DrummerVizProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void DrummerVizProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    latencySamples = (int) std::lround (sampleRate * kLatencyMs / 1000.0);
    setLatencySamples (latencySamples);                       // Ableton компенсирует эту задержку

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    delay.prepare (spec);
    delay.setDelay ((float) latencySamples);

    using C = juce::dsp::IIR::Coefficients<float>;
    const float sr = (float) sampleRate;
    flt[0].coefficients = C::makeLowPass  (sampleRate, 120.0f);      // полоса 0
    flt[1].coefficients = C::makeHighPass (sampleRate, 120.0f);      // полоса 1: ВЧ 120 + НЧ 400
    flt[2].coefficients = C::makeLowPass  (sampleRate, 400.0f);
    flt[3].coefficients = C::makeHighPass (sampleRate, 400.0f);      // полоса 2: 400..2500
    flt[4].coefficients = C::makeLowPass  (sampleRate, 2500.0f);
    flt[5].coefficients = C::makeHighPass (sampleRate, 2500.0f);     // полоса 3: 2500..8000
    flt[6].coefficients = C::makeLowPass  (sampleRate, juce::jmin (8000.0f, 0.45f * sr));
    flt[7].coefficients = C::makeHighPass (sampleRate, juce::jmin (8000.0f, 0.45f * sr));   // полоса 4
    for (auto& f : flt) f.reset();

    acc.fill (0.0);
    frameCount = 0;
    frameLen = juce::jmax (1, (int) std::lround (sampleRate * 0.005));   // кадр 5 мс
    fifo.reset();
}

void DrummerVizProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // 1) транспорт хоста
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
            if (auto b = pos->getBpm()) hostBpm = *b;
            hostPlaying = pos->getIsPlaying();
        }

    // 2) полосы уровня (звук ещё не задержан → анализ «видит» удар раньше, чем он будет слышен)
    const int n = buffer.getNumSamples(), chs = buffer.getNumChannels();
    const float* l = buffer.getReadPointer (0);
    const float* r = chs > 1 ? buffer.getReadPointer (1) : l;
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (l[i] + r[i]);
        const float y[kBands] = {
            flt[0].processSample (m),
            flt[2].processSample (flt[1].processSample (m)),
            flt[4].processSample (flt[3].processSample (m)),
            flt[6].processSample (flt[5].processSample (m)),
            flt[7].processSample (m)
        };
        for (int b = 0; b < kBands; ++b) acc[(size_t) b] += (double) y[b] * (double) y[b];

        if (++frameCount >= frameLen)
        {
            int s1, z1, s2, z2;
            fifo.prepareToWrite (1, s1, z1, s2, z2);
            if (z1 > 0)
            {
                for (int b = 0; b < kBands; ++b)
                    bandBuf[(size_t) s1 * kBands + (size_t) b] = (float) std::sqrt (acc[(size_t) b] / frameLen);
                fifo.finishedWrite (1);
            }
            acc.fill (0.0);
            frameCount = 0;
        }
    }

    // 3) звук идёт дальше без изменений, только с задержкой
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    delay.process (ctx);
}

int DrummerVizProcessor::popBandFrames (std::vector<float>& out)
{
    int frames = 0;
    for (;;)
    {
        int s1, z1, s2, z2;
        fifo.prepareToRead (1, s1, z1, s2, z2);
        if (z1 <= 0) break;
        for (int b = 0; b < kBands; ++b) out.push_back (bandBuf[(size_t) s1 * kBands + (size_t) b]);
        fifo.finishedRead (1);
        ++frames;
    }
    return frames;
}

juce::AudioProcessorEditor* DrummerVizProcessor::createEditor()
{
    return new DrummerVizEditor (*this);
}

void DrummerVizProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream out (destData, false);
    out.writeInt (0x44565A32);                    // "DVZ2"
    out.writeInt (mode);
    out.writeDouble (offsetQuarters);
    out.writeInt ((int) midiFile.getSize());
    if (midiFile.getSize() > 0) out.write (midiFile.getData(), midiFile.getSize());
}

void DrummerVizProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in (data, (size_t) sizeInBytes, false);
    if (in.readInt() != 0x44565A32) return;
    mode = in.readInt();
    offsetQuarters = in.readDouble();
    const int n = in.readInt();
    midiFile.reset();
    if (n > 0)
    {
        midiFile.setSize ((size_t) n);
        in.read (midiFile.getData(), n);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrummerVizProcessor();
}
