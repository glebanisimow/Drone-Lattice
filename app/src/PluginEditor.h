#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "LatticeView.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"

// 60 s history of the 4 mod bus channels.
class ModScope final : public juce::Component
{
public:
    explicit ModScope (DroneProcessor&);
    void push (const std::array<float, 4>& v);
    void paint (juce::Graphics&) override;

private:
    DroneProcessor& proc;
    std::vector<std::array<float, 4>> hist;
    int head = 0, count = 0;
};

class DroneEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DroneEditor (DroneProcessor&);
    ~DroneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void savePreset();
    void loadPreset();
    std::vector<ParamInfo> group (const juce::String& g) const;

    DroneProcessor& proc;
    theme::Look look;

    juce::Label title, cpuLabel, swarmLabel, meshLabel;
    juce::ComboBox presets;
    juce::TextButton saveBtn { "Save" }, loadBtn { "Load" }, seedBtn { "New seed" };
    juce::ToggleButton modToggle { "Mod bus" };
    juce::Slider blend { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modAtt;
    std::unique_ptr<Knob> level;

    LatticeView lattice;
    ModScope scope;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DroneEditor)
};
