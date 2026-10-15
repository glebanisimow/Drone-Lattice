// Widgets shared by the Drone Lattice and Ward editors.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

// Editor-facing description of a host parameter.
struct ParamInfo
{
    juce::String id, group, name;
    int groupOrder = 0, order = 0;
};

// Rotary knob bound to a host parameter, with name and value.
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name);
    void resized() override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Rows of knobs with a caption per row.
class KnobPage final : public juce::Component
{
public:
    struct Row { juce::String title; std::vector<ParamInfo> params; };
    KnobPage (juce::AudioProcessorValueTreeState&, std::vector<Row>);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    std::vector<Row> rows;
    std::vector<std::vector<std::unique_ptr<Knob>>> knobs;
};

// Params of one group, in their Faust [N] order.
std::vector<ParamInfo> paramsOfGroup (const std::vector<ParamInfo>&, const juce::String& group);
