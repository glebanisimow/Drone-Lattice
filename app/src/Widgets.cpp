#include "Widgets.h"
#include "Theme.h"

#include <algorithm>

using namespace theme;

//--------------------------------------------------------------------- Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
{
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    slider.setColour (juce::Slider::textBoxTextColourId, dim);
    addAndMakeVisible (slider);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.5f)));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    slider.setBounds (b);
}

//------------------------------------------------------------------ KnobPage
KnobPage::KnobPage (juce::AudioProcessorValueTreeState& s, std::vector<Row> r) : rows (std::move (r))
{
    for (auto& row : rows)
    {
        auto& v = knobs.emplace_back();
        for (auto& p : row.params)
        {
            v.push_back (std::make_unique<Knob> (s, p.id, p.name));
            addAndMakeVisible (*v.back());
        }
    }
}

void KnobPage::resized()
{
    const int kw = 92, kh = 92, cap = 20;
    int y = 12;
    for (auto& row : knobs)
    {
        int x = 14;
        y += cap;
        for (auto& k : row)
        {
            if (x + kw > getWidth() - 8)
            {
                x = 14;
                y += kh + 4;
            }
            k->setBounds (x, y, kw, kh);
            x += kw + 4;
        }
        y += kh + 8;
    }
}

void KnobPage::paint (juce::Graphics& g)
{
    g.fillAll (panel);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (knobs[i].empty()) continue;
        const auto first = knobs[i].front()->getBounds();
        g.setColour (dim);
        g.drawText (rows[i].title.toUpperCase(), 16, first.getY() - 20, 400, 16, juce::Justification::centredLeft);
    }
}

std::vector<ParamInfo> paramsOfGroup (const std::vector<ParamInfo>& all, const juce::String& g)
{
    std::vector<ParamInfo> v;
    for (auto& p : all)
        if (p.group == g) v.push_back (p);
    std::stable_sort (v.begin(), v.end(), [] (const ParamInfo& a, const ParamInfo& b) { return a.order < b.order; });
    return v;
}
