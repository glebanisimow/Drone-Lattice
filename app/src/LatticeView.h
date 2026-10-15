// Interactive JI lattice: the visual form is the interface.
//   nodes glow with the engines' node energy, labels show JI ratio + Hz,
//   edges light up where res_mesh neighbours share a mode (coupling paths),
//   drag = focus, wheel = radius, double-click a node = recentre the lattice.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace lattice
{
struct Ratio
{
    unsigned long long p = 1, q = 1;
    bool exact = true;    // false: too large for a readable fraction
    double l2 = 0.0;      // log2 of the octave-reduced ratio, [0, 1)
};

Ratio ratio (int i, int j, int genX, int genY);

// res_mesh modes h_a of node a and h_b of node b at the same frequency (2 cent).
bool sharedMode (double la, double lb, double stretch, int modes, int& ha, int& hb);
} // namespace lattice

class LatticeView final : public juce::Component
{
public:
    explicit LatticeView (DroneProcessor&);

    void refresh();   // pull meters, ~30 Hz
    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    static constexpr int GW = 4, GH = 4, NN = GW * GH;

    DroneProcessor& proc;
    std::array<float, NN> energy {};
    float meshPeak = 1e-6f;
    int hover = -1;
    bool dragging = false;

    juce::Rectangle<float> grid() const;
    float cell() const { return grid().getWidth() / GW; }
    juce::Point<float> toPx (float fx, float fy) const;
    juce::Point<float> fromPx (juce::Point<float>) const;
    int nodeAt (juce::Point<float>) const;
    void setFocus (juce::Point<float> px);
    int centreI() const;
    int centreJ() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatticeView)
};
