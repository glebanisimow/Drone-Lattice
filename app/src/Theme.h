#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour bg      { 0xff0b0d10 };
inline const juce::Colour panel   { 0xff14171c };
inline const juce::Colour raised  { 0xff1c2027 };
inline const juce::Colour text    { 0xffd8d4cc };
inline const juce::Colour dim     { 0xff7d8590 };
inline const juce::Colour nodeOff { 0xff2a2f37 };
inline const juce::Colour amber   { 0xffe8a33d };
inline const juce::Colour teal    { 0xff4fb3bf };
inline const juce::Colour violet  { 0xffb07ce8 };
inline const juce::Colour green   { 0xff8bc34a };

inline juce::Colour modColour (int c)
{
    switch (c)
    {
        case 0:  return amber;
        case 1:  return teal;
        case 2:  return violet;
        default: return green;
    }
}

class Look final : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, amber);
        setColour (juce::Slider::trackColourId, amber.withAlpha (0.6f));
        setColour (juce::Slider::backgroundColourId, raised);
        setColour (juce::TextButton::buttonColourId, raised);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, amber);
        setColour (juce::ComboBox::backgroundColourId, raised);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, raised);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.3f));
        setColour (juce::TabbedButtonBar::tabTextColourId, dim);
        setColour (juce::TabbedButtonBar::frontTextColourId, text);
        setColour (juce::TabbedComponent::backgroundColourId, panel);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = start + pos * (end - start);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - 3, r - 3, 0.0f, start, end, true);
        g.setColour (raised.brighter (0.15f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 3, r - 3, 0.0f, start, ang, true);
        g.setColour (amber);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (raised);
        g.fillEllipse (c.x - r + 8, c.y - r + 8, 2 * r - 16, 2 * r - 16);
        const auto tip = c.getPointOnCircumference (r - 11, ang);
        g.setColour (text);
        g.drawLine ({ c.getPointOnCircumference (r * 0.2f, ang), tip }, 2.0f);
    }

    void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool) override
    {
        const auto area = b.getActiveArea().toFloat();
        const bool front = b.isFrontTab();
        g.setColour (front ? panel : bg);
        g.fillRect (area);
        if (front)
        {
            g.setColour (amber);
            g.fillRect (area.withHeight (2.0f));
        }
        g.setColour (front ? text : (over ? text.withAlpha (0.8f) : dim));
        g.setFont (juce::FontOptions (13.5f, front ? juce::Font::bold : juce::Font::plain));
        g.drawText (b.getButtonText(), area, juce::Justification::centred);
    }
};
} // namespace theme
