// Faust <-> host glue: collects the widgets of a generated Faust class
// (sliders, entries, bargraphs) with their group, order and metadata.
#pragma once

#include <faust/dsp/dsp.h>
#include <faust/gui/UI.h>
#include <faust/gui/meta.h>

#include <cctype>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct FaustParam
{
    std::string group, name, unit;   // group = innermost box ("tuning")
    float init = 0, min = 0, max = 1, step = 0;
    bool log = false;
    int order = 0, groupOrder = 0;   // from the [N] label metadata
    float* zone = nullptr;
};

struct FaustMeter
{
    std::string name;
    float* zone = nullptr;
    float min = 0, max = 1;
};

class ParamCollector final : public UI
{
public:
    std::vector<FaustParam> params;
    std::vector<FaustMeter> meters;

    void openTabBox (const char* l) override        { open (l); }
    void openHorizontalBox (const char* l) override { open (l); }
    void openVerticalBox (const char* l) override   { open (l); }
    void closeBox() override                        { if (! stack.empty()) stack.pop_back(); }

    void addButton (const char* l, float* z) override      { add (l, z, 0, 0, 1, 1); }
    void addCheckButton (const char* l, float* z) override { add (l, z, 0, 0, 1, 1); }
    void addVerticalSlider (const char* l, float* z, float i, float lo, float hi, float s) override   { add (l, z, i, lo, hi, s); }
    void addHorizontalSlider (const char* l, float* z, float i, float lo, float hi, float s) override { add (l, z, i, lo, hi, s); }
    void addNumEntry (const char* l, float* z, float i, float lo, float hi, float s) override         { add (l, z, i, lo, hi, s); }

    void addHorizontalBargraph (const char* l, float* z, float lo, float hi) override { meters.push_back ({ l, z, lo, hi }); }
    void addVerticalBargraph (const char* l, float* z, float lo, float hi) override   { meters.push_back ({ l, z, lo, hi }); }
    void addSoundfile (const char*, const char*, Soundfile**) override {}

    void declare (float* z, const char* key, const char* val) override
    {
        if (z == nullptr)
        {
            if (isOrder (key))
                pendingBoxOrder = std::atoi (key);
            return;
        }
        auto& m = meta[z];
        const std::string k (key);
        if (isOrder (key))     m.order = std::atoi (key);
        else if (k == "scale") m.log = std::string (val) == "log";
        else if (k == "unit")  m.unit = val;
    }

private:
    struct Meta { int order = 0; bool log = false; std::string unit; };
    struct Box  { std::string label; int order = 0; };

    std::vector<Box> stack;
    std::map<float*, Meta> meta;
    int pendingBoxOrder = 0;

    static bool isOrder (const char* k)
    {
        if (*k == 0) return false;
        for (; *k; ++k)
            if (! std::isdigit ((unsigned char) *k)) return false;
        return true;
    }

    void open (const char* l)
    {
        stack.push_back ({ l, pendingBoxOrder });
        pendingBoxOrder = 0;
    }

    void add (const char* l, float* z, float init, float lo, float hi, float step)
    {
        FaustParam p;
        if (stack.size() > 1)
        {
            p.group = stack.back().label;
            p.groupOrder = stack.back().order;
        }
        p.name = l;
        p.zone = z;
        p.init = init; p.min = lo; p.max = hi; p.step = step;
        const auto& m = meta[z];
        p.order = m.order; p.log = m.log; p.unit = m.unit;
        params.push_back (p);
    }
};

// Factories, one translation unit per generated class.
std::unique_ptr<dsp> createLatticeSwarm();
std::unique_ptr<dsp> createResMesh();
std::unique_ptr<dsp> createModBus();
