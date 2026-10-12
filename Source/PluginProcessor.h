#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/*  Drummer Viz — аудио-ЭФФЕКТ. Звук проходит насквозь (с задержкой kLatencyMs, её компенсирует Ableton),
    а плагин даёт барабанщику две вещи:
      1) положение и темп транспорта Ableton — по ним картинка играет ноты из загруженного MIDI-файла;
      2) 5 полос уровня звука каждые 5 мс — по ним веб-страница пытается «услышать» удары (экспериментально).  */
class DrummerVizProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kLatencyMs = 100;   // насколько звук задерживается: это даёт руке время замахнуться
    static constexpr int kBands = 5;         // <=120 Гц, 120-400, 400-2500, 2500-8000, >=8000 Гц
    static constexpr int kFifoFrames = 2048;

    DrummerVizProcessor();
    ~DrummerVizProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drummer Viz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // транспорт хоста (пишет аудиопоток, читает интерфейс)
    std::atomic<double> hostPpq { 0.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };

    // кадры полос; возвращает число добавленных кадров
    int popBandFrames (std::vector<float>& out);

    // сохраняется вместе с проектом Ableton
    juce::MemoryBlock midiFile;
    int mode = 0;                    // 0 = ноты из MIDI-файла по транспорту, 1 = слушать звук
    double offsetQuarters = 0.0;     // сдвиг клипа относительно начала проекта, в четвертях

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> delay { 1 << 18 };
    std::array<juce::dsp::IIR::Filter<float>, 8> flt;
    std::array<double, kBands> acc {};
    int frameLen = 220, frameCount = 0;
    int latencySamples = 4410;

    juce::AbstractFifo fifo { kFifoFrames };
    std::array<float, (size_t) kFifoFrames * kBands> bandBuf {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrummerVizProcessor)
};
