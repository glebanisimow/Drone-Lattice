#include "LatticeView.h"
#include "Theme.h"

#include <cmath>
#include <numeric>

namespace lattice
{
static constexpr unsigned long long LIM = 1000000000000000ULL;   // overflow guard

Ratio ratio (int i, int j, int genX, int genY)
{
    Ratio r;
    const double e = i * std::log2 ((double) genX) + j * std::log2 ((double) genY);
    r.l2 = e - std::floor (e);

    bool ok = true;
    auto mul = [&ok] (unsigned long long& a, unsigned long long g, int k)
    {
        for (int t = 0; t < k && ok; ++t)
        {
            if (a > LIM / g) ok = false;
            else a *= g;
        }
    };
    mul (i > 0 ? r.p : r.q, (unsigned long long) genX, std::abs (i));
    mul (j > 0 ? r.p : r.q, (unsigned long long) genY, std::abs (j));

    // octave-reduce to [1, 2)
    while (ok && r.p >= 2 * r.q)
    {
        if (r.p % 2 == 0) r.p /= 2;
        else if (r.q > LIM / 2) ok = false;
        else r.q *= 2;
    }
    while (ok && r.p < r.q)
    {
        if (r.q % 2 == 0) r.q /= 2;
        else if (r.p > LIM / 2) ok = false;
        else r.p *= 2;
    }
    if (ok)
    {
        const auto g = std::gcd (r.p, r.q);
        r.p /= g;
        r.q /= g;
    }
    r.exact = ok && r.p < 100000 && r.q < 100000;
    return r;
}

bool sharedMode (double la, double lb, double stretch, int modes, int& ha, int& hb)
{
    for (int a = 1; a <= modes; ++a)
        for (int b = 1; b <= modes; ++b)
        {
            const double d = la - lb + (1.0 + stretch) * (std::log2 ((double) a) - std::log2 ((double) b));
            if (std::abs (d) * 1200.0 < 2.0)
            {
                ha = a;
                hb = b;
                return true;
            }
        }
    return false;
}
} // namespace lattice

LatticeView::LatticeView (DroneProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
}

int LatticeView::centreI() const { return (GW - 1) / 2; }
int LatticeView::centreJ() const { return (GH - 1) / 2; }

juce::Rectangle<float> LatticeView::grid() const
{
    auto b = getLocalBounds().toFloat().reduced (28.0f, 0.0f).withTrimmedTop (40.0f).withTrimmedBottom (54.0f);
    const float side = juce::jmin (b.getWidth(), b.getHeight());
    return b.withSizeKeepingCentre (side, side);
}

juce::Point<float> LatticeView::toPx (float fx, float fy) const
{
    const auto g = grid();
    const float c = cell();
    return { g.getX() + (fx + 0.5f) * c, g.getBottom() - (fy + 0.5f) * c };
}

juce::Point<float> LatticeView::fromPx (juce::Point<float> p) const
{
    const auto g = grid();
    const float c = cell();
    return { (p.x - g.getX()) / c - 0.5f, (g.getBottom() - p.y) / c - 0.5f };
}

int LatticeView::nodeAt (juce::Point<float> p) const
{
    const float r = cell() * 0.3f;
    for (int n = 0; n < NN; ++n)
        if (toPx ((float) (n % GW), (float) (n / GW)).getDistanceFrom (p) < r)
            return n;
    return -1;
}

void LatticeView::refresh()
{
    const float blend = proc.param ("app.blend");
    const float ga = std::cos (blend * juce::MathConstants<float>::halfPi);
    const float gb = std::sin (blend * juce::MathConstants<float>::halfPi);

    float maxA = 1e-6f, maxB = 1e-6f;
    std::array<float, NN> a {}, b {};
    for (int n = 0; n < NN; ++n)
    {
        a[(size_t) n] = proc.nodeMeter (DroneProcessor::SWARM, n);
        b[(size_t) n] = proc.nodeMeter (DroneProcessor::MESH, n);
        maxA = juce::jmax (maxA, a[(size_t) n]);
        maxB = juce::jmax (maxB, b[(size_t) n]);
    }
    meshPeak = juce::jmax (maxB, meshPeak * 0.985f);   // slow release: loudness changes stay visible

    for (int n = 0; n < NN; ++n)
    {
        const float t = juce::jlimit (0.0f, 1.0f, ga * a[(size_t) n] / maxA + gb * std::sqrt (b[(size_t) n] / meshPeak));
        energy[(size_t) n] += 0.25f * (t - energy[(size_t) n]);
    }
}

void LatticeView::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const int gx = juce::roundToInt (proc.param ("tuning.genX"));
    const int gy = juce::roundToInt (proc.param ("tuning.genY"));
    const int ox = juce::roundToInt (proc.param ("tuning.offX"));
    const int oy = juce::roundToInt (proc.param ("tuning.offY"));
    const float root = proc.param ("tuning.root");
    const float blend = proc.param ("app.blend");
    const float gb = std::sin (blend * juce::MathConstants<float>::halfPi);
    const bool modOn = proc.param ("app.modOn") > 0.5f;
    const float c = cell();

    // node data
    struct Node { lattice::Ratio r; int i, j; juce::Point<float> px; };
    std::array<Node, NN> nodes;
    for (int n = 0; n < NN; ++n)
    {
        const int x = n % GW, y = n / GW;
        const int i = x - centreI() + ox, j = y - centreJ() + oy;
        nodes[(size_t) n] = { lattice::ratio (i, j, gx, gy), i, j, toPx ((float) x, (float) y) };
    }

    // title
    g.setColour (text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("LATTICE", bounds.withHeight (34.0f).reduced (16.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String (root, 2) + " Hz   x" + juce::String (gx) + " / y" + juce::String (gy)
                    + "   offset " + juce::String (ox) + ", " + juce::String (oy),
                bounds.withHeight (34.0f).reduced (16.0f, 0.0f), juce::Justification::centredRight);

    // focus field (with mod bus displacement)
    const float fx = proc.param ("lattice.focusX"), fy = proc.param ("lattice.focusY");
    const float rad = proc.param ("lattice.radius");
    const float m1 = modOn ? proc.modValue (0) : 0.0f, m2 = modOn ? proc.modValue (1) : 0.0f, m3 = modOn ? proc.modValue (2) : 0.0f;
    const float efx = fx + proc.param ("mod.toFocusX") * m1;
    const float efy = fy + proc.param ("mod.toFocusY") * m2;
    const float erad = rad * std::exp2 (proc.param ("mod.toRadius") * m3);
    const auto ef = toPx (efx, efy);
    {
        const float R = juce::jmax (8.0f, erad * c * 2.2f);
        juce::ColourGradient grad (amber.withAlpha (0.22f), ef.x, ef.y, amber.withAlpha (0.0f), ef.x + R, ef.y, true);
        g.setGradientFill (grad);
        g.fillEllipse (ef.x - R, ef.y - R, 2 * R, 2 * R);
    }

    // edges: faint grid, bright where res_mesh neighbours share a mode
    const float stretch = proc.param ("mesh.stretch");
    for (int n = 0; n < NN; ++n)
    {
        const int x = n % GW, y = n / GW;
        for (int k = 0; k < 2; ++k)
        {
            const int m = k == 0 ? (x + 1 < GW ? n + 1 : -1) : (y + 1 < GH ? n + GW : -1);
            if (m < 0) continue;
            const auto& A = nodes[(size_t) n];
            const auto& B = nodes[(size_t) m];
            int ha = 0, hb = 0;
            const bool shared = lattice::sharedMode (A.r.l2, B.r.l2, stretch, DroneProcessor::MESH_MODES, ha, hb);
            const auto p0 = A.px, p1 = B.px;
            if (shared && gb > 0.01f)
            {
                const float e = juce::jmin (energy[(size_t) n], energy[(size_t) m]);
                g.setColour (teal.withAlpha (gb * (0.25f + 0.6f * e)));
                g.drawLine ({ p0, p1 }, 1.5f + 3.0f * e);
                g.setColour (teal.withAlpha (gb * 0.8f));
                g.setFont (juce::FontOptions (10.5f));
                const auto mid = (p0 + p1) * 0.5f;
                const auto lbl = "h" + juce::String (ha) + ":h" + juce::String (hb);
                if (k == 0) g.drawText (lbl, juce::Rectangle<float> (60, 14).withCentre (mid.translated (0, -11)), juce::Justification::centred);
                else        g.drawText (lbl, juce::Rectangle<float> (60, 14).withCentre (mid.translated (26, 0)), juce::Justification::centred);
            }
            else
            {
                g.setColour (text.withAlpha (0.07f));
                g.drawLine ({ p0, p1 }, 1.0f);
            }
        }
    }

    // nodes
    for (int n = 0; n < NN; ++n)
    {
        const auto& N = nodes[(size_t) n];
        const float e = energy[(size_t) n];
        const auto p = N.px;
        const float glow = c * (0.18f + 0.26f * e);
        g.setColour (amber.withAlpha (0.10f + 0.30f * e));
        g.fillEllipse (p.x - glow, p.y - glow, 2 * glow, 2 * glow);
        const float core = c * 0.12f;
        g.setColour (nodeOff.interpolatedWith (amber, juce::jlimit (0.0f, 1.0f, 0.15f + 0.85f * e)));
        g.fillEllipse (p.x - core, p.y - core, 2 * core, 2 * core);
        if (N.i == 0 && N.j == 0)
        {
            g.setColour (text.withAlpha (0.8f));
            g.drawEllipse (p.x - core - 4, p.y - core - 4, 2 * core + 8, 2 * core + 8, 1.2f);
        }
        if (n == hover)
        {
            g.setColour (text);
            g.drawEllipse (p.x - core - 7, p.y - core - 7, 2 * core + 14, 2 * core + 14, 1.0f);
        }

        const auto label = N.r.exact ? juce::String ((juce::int64) N.r.p) + "/" + juce::String ((juce::int64) N.r.q)
                                     : juce::String (juce::roundToInt (N.r.l2 * 1200.0)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));
        g.setColour (text.withAlpha (0.55f + 0.45f * e));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> (c, 18).withCentre (p.translated (0, core + 16)), juce::Justification::centred);
        g.setColour (dim);
        g.setFont (juce::FontOptions (11.0f));
        const double hz = root * std::exp2 (N.r.l2);
        g.drawText (juce::String (hz, hz < 100 ? 1 : 0) + " Hz", juce::Rectangle<float> (c, 14).withCentre (p.translated (0, core + 31)),
                    juce::Justification::centred);
    }

    // focus handle (parameter) and effective focus (with mod)
    const auto bf = toPx (fx, fy);
    g.setColour (text.withAlpha (0.35f));
    g.drawLine ({ bf, ef }, 1.0f);
    {
        const float R = rad * c;
        juce::Path ring;
        ring.addEllipse (bf.x - R, bf.y - R, 2 * R, 2 * R);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 5.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, ring, dashes, 2);
        g.setColour (text.withAlpha (0.25f));
        g.fillPath (dashed);
    }
    g.setColour (text);
    g.drawEllipse (bf.x - 9, bf.y - 9, 18, 18, 1.6f);
    g.setColour (amber);
    g.fillEllipse (ef.x - 4, ef.y - 4, 8, 8);

    // axes
    const auto gr = grid();
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97 ")) + juce::String (gx) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92")),
                juce::Rectangle<float> (gr.getX(), gr.getBottom() + 2, gr.getWidth(), 16), juce::Justification::centred);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, gr.getX() - 14, gr.getCentreY()));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97 ")) + juce::String (gy) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92")),
                juce::Rectangle<float> (gr.getX() - 14 - 100, gr.getCentreY() - 8, 200, 16), juce::Justification::centred);
    g.restoreState();

    // footer: hover info or hint
    auto foot = bounds.withTrimmedTop (bounds.getHeight() - 34).reduced (16, 0);
    g.setFont (juce::FontOptions (12.5f));
    if (hover >= 0)
    {
        const auto& N = nodes[(size_t) hover];
        const double hz = root * std::exp2 (N.r.l2);
        const double hd = std::abs (N.i) * std::log2 ((double) gx) + std::abs (N.j) * std::log2 ((double) gy);
        g.setColour (text);
        g.drawText ("node " + juce::String (N.i) + ", " + juce::String (N.j) + "    "
                        + (N.r.exact ? juce::String ((juce::int64) N.r.p) + "/" + juce::String ((juce::int64) N.r.q) : juce::String ("-"))
                        + "    " + juce::String (hz, 2) + " Hz    " + juce::String (N.r.l2 * 1200.0, 1) + " cent    hdist "
                        + juce::String (hd, 2) + "    energy " + juce::String (juce::roundToInt (energy[(size_t) hover] * 100)) + "%",
                    foot, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (dim);
        g.drawText ("drag: focus    wheel: radius    double-click a node: make it the centre", foot,
                    juce::Justification::centredLeft);
    }
}

void LatticeView::setFocus (juce::Point<float> px)
{
    const auto f = fromPx (px);
    if (auto* p = proc.apvts.getParameter ("lattice.focusX"))
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.0f, (float) (GW - 1), f.x)));
    if (auto* p = proc.apvts.getParameter ("lattice.focusY"))
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.0f, (float) (GH - 1), f.y)));
}

void LatticeView::mouseDown (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() > 1 || ! e.mods.isLeftButtonDown()) return;
    dragging = true;
    for (auto* id : { "lattice.focusX", "lattice.focusY" })
        if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
    setFocus (e.position);
    repaint();
}

void LatticeView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || ! e.mods.isLeftButtonDown()) return;
    setFocus (e.position);
    repaint();
}

void LatticeView::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    for (auto* id : { "lattice.focusX", "lattice.focusY" })
        if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
}

void LatticeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n < 0) return;
    const int dx = n % GW - centreI(), dy = n / GW - centreJ();
    proc.setParam ("tuning.offX", juce::jlimit (-6.0f, 6.0f, proc.param ("tuning.offX") + (float) dx));
    proc.setParam ("tuning.offY", juce::jlimit (-6.0f, 6.0f, proc.param ("tuning.offY") + (float) dy));
    proc.setParam ("lattice.focusX", (float) centreI());
    proc.setParam ("lattice.focusY", (float) centreJ());
    repaint();
}

void LatticeView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float r = proc.param ("lattice.radius") * std::exp2 (w.deltaY * 1.5f);
    proc.setParam ("lattice.radius", juce::jlimit (0.2f, 4.0f, r));
    repaint();
}

void LatticeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = nodeAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void LatticeView::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}
