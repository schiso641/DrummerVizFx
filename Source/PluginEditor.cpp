#include "PluginEditor.h"
#include <BinaryData.h>
#include <cstring>
#include <vector>

DrummerVizEditor::DrummerVizEditor (DrummerVizProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    auto options = juce::WebBrowserComponent::Options {};
   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                         .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                  .getChildFile ("DrummerViz")));
   #endif
    options = options.withResourceProvider ([this] (const juce::String& url) { return getResource (url); });

    web = std::make_unique<Web> (options);
    web->onLoaded = [this] { pageLoaded = true; pushState(); };
    addAndMakeVisible (*web);

    addAndMakeVisible (openBtn);
    openBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose the MIDI clip that Superior Drummer plays", juce::File {}, "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                  {
                                      juce::MemoryBlock mb;
                                      if (f.loadFileAsData (mb)) { proc.midiFile = mb; pushMidi(); }
                                  }
                              });
    };

    addAndMakeVisible (modeBox);
    modeBox.addItem ("Notes from MIDI file (exact, synced to Ableton)", 1);
    modeBox.addItem ("Listen to the audio (experimental)", 2);
    modeBox.setSelectedId (proc.mode + 1, juce::dontSendNotification);
    modeBox.onChange = [this] { proc.mode = modeBox.getSelectedId() - 1; pushMode(); };

    addAndMakeVisible (offsetLabel);
    addAndMakeVisible (offsetSlider);
    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    offsetSlider.setRange (-256.0, 256.0, 0.25);
    offsetSlider.setValue (proc.offsetQuarters, juce::dontSendNotification);
    offsetSlider.onValueChange = [this] { proc.offsetQuarters = offsetSlider.getValue(); pushOffset(); };

    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (420, 360, 1600, 1200);
    setSize (900, 560);
    startTimerHz (30);
}

DrummerVizEditor::~DrummerVizEditor()
{
    stopTimer();
}

void DrummerVizEditor::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (32).reduced (4, 3);
    openBtn.setBounds (bar.removeFromLeft (110));
    bar.removeFromLeft (6);
    modeBox.setBounds (bar.removeFromLeft (juce::jmin (300, bar.getWidth() / 2)));
    bar.removeFromLeft (8);
    offsetLabel.setBounds (bar.removeFromLeft (130));
    offsetSlider.setBounds (bar);
    web->setBounds (area);
}

std::optional<juce::WebBrowserComponent::Resource> DrummerVizEditor::getResource (const juce::String& url)
{
    if (url == "/" || url == "/index.html" || url.isEmpty())
    {
        std::vector<std::byte> bytes ((size_t) BinaryData::drummer_htmlSize);
        std::memcpy (bytes.data(), BinaryData::drummer_html, bytes.size());
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String ("text/html") };
    }
    return std::nullopt;
}

void DrummerVizEditor::js (const juce::String& code)
{
    if (pageLoaded && web != nullptr) web->evaluateJavascript (code);
}

void DrummerVizEditor::pushMidi()
{
    if (proc.midiFile.getSize() == 0) return;
    js ("window.__plug&&window.__plug.loadMidi('" + juce::Base64::toBase64 (proc.midiFile.getData(), proc.midiFile.getSize()) + "')");
}

void DrummerVizEditor::pushMode()   { js (juce::String ("window.__plug&&window.__plug.mode('") + (proc.mode == 1 ? "audio" : "midi") + "')"); }
void DrummerVizEditor::pushOffset() { js ("window.__plug&&window.__plug.offset(" + juce::String (proc.offsetQuarters, 3) + ")"); }

void DrummerVizEditor::pushState()
{
    js ("window.__plug&&window.__plug.init({latencyMs:" + juce::String (DrummerVizProcessor::kLatencyMs) + "})");
    pushMode();
    pushOffset();
    pushMidi();
}

void DrummerVizEditor::timerCallback()
{
    std::vector<float> frames;
    proc.popBandFrames (frames);              // всегда разбираем очередь, чтобы она не переполнялась
    if (! pageLoaded) return;

    js ("window.__plug&&window.__plug.tick(" + juce::String (proc.hostPpq.load(), 6) + "," + juce::String (proc.hostBpm.load(), 4)
        + "," + (proc.hostPlaying.load() ? "1" : "0") + ")");

    if (proc.mode == 1 && ! frames.empty())
    {
        juce::String s ("window.__plug&&window.__plug.bands([");
        for (size_t i = 0; i < frames.size(); ++i)
        {
            if (i > 0) s << ",";
            s << juce::String (frames[i], 5);
        }
        s << "])";
        js (s);
    }
}
