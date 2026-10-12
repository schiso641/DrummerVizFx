#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <memory>
#include <optional>

class DrummerVizEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrummerVizEditor (DrummerVizProcessor&);
    ~DrummerVizEditor() override;

    void resized() override;

private:
    // WebView, который сообщает, что страница загрузилась
    struct Web : juce::WebBrowserComponent
    {
        using juce::WebBrowserComponent::WebBrowserComponent;
        std::function<void()> onLoaded;
        void pageFinishedLoading (const juce::String&) override { if (onLoaded) onLoaded(); }
    };

    void timerCallback() override;
    void js (const juce::String& code);
    void pushState();
    void pushMidi();
    void pushMode();
    void pushOffset();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    DrummerVizProcessor& proc;
    juce::TextButton openBtn { "Open MIDI..." };
    juce::ComboBox modeBox;
    juce::Label offsetLabel { {}, "Start offset (beats):" };
    juce::Slider offsetSlider;
    std::unique_ptr<Web> web;
    std::unique_ptr<juce::FileChooser> chooser;
    bool pageLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrummerVizEditor)
};
