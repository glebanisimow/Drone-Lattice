#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <set>

namespace
{
std::unique_ptr<EngineSlot> makeSlot (const char* key, std::unique_ptr<dsp> d)
{
    auto s = std::make_unique<EngineSlot>();
    s->key = key;
    s->dsp = std::move (d);
    s->dsp->buildUserInterface (&s->ui);
    for (auto& m : s->ui.meters)
        if (m.name.size() == 3 && m.name[0] == 'n')
        {
            const int n = std::atoi (m.name.c_str() + 1);
            if (n >= 0 && n < 16) s->meters[(size_t) n] = m.zone;
        }
    return s;
}

std::vector<std::unique_ptr<EngineSlot>> makeEngines()
{
    std::vector<std::unique_ptr<EngineSlot>> v;
    v.push_back (makeSlot ("swarm", createLatticeSwarm()));
    v.push_back (makeSlot ("mesh", createResMesh()));
    v.push_back (makeSlot ("bus", createModBus()));
    return v;
}

// Contract groups shared by both engines map to one host parameter, so one
// knob drives both (same paths and ranges by contract). The mod bus has its
// own namespace.
juce::String paramId (const juce::String& engineKey, const FaustParam& p)
{
    const auto base = juce::String (p.group) + "." + juce::String (p.name);
    return engineKey == "bus" ? "modbus." + base : base;
}

juce::NormalisableRange<float> rangeFor (const FaustParam& p)
{
    const float step = p.step;
    if (p.log && p.min > 0.0f)
        return { p.min, p.max,
                 [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [step] (float s, float e, float v)
                 { return juce::jlimit (s, e, step > 0.0f ? std::round (v / step) * step : v); } };
    return { p.min, p.max, step };
}

int decimalsFor (float step)
{
    if (step >= 1.0f)   return 0;
    if (step >= 0.1f)   return 1;
    if (step >= 0.01f)  return 2;
    if (step >= 0.001f) return 3;
    return 4;
}

float softclip (float x)
{
    const float k = 0.95f, a = std::abs (x);
    if (a <= k) return x;
    return std::copysign (k + (1.0f - k) * std::tanh ((a - k) / (1.0f - k)), x);
}
} // namespace

DroneProcessor::DroneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      engines (makeEngines()),
      apvts (*this, nullptr, "DroneLattice", createLayout())
{
    for (auto& s : engines)
        for (size_t i = 0; i < s->ui.params.size(); ++i)
            s->bind.emplace_back (apvts.getRawParameterValue (s->ids[i]), s->ui.params[i].zone);

    blendParam = apvts.getRawParameterValue ("app.blend");
    modOnParam = apvts.getRawParameterValue ("app.modOn");
    for (auto& m : lastMod) m.store (0.0f);
    mono.bind (apvts);
    for (auto& s : engines)
        for (auto& p : s->ui.params)
            if (p.group == "tuning" && p.name == "root")  rootZones.push_back (p.zone);
            else if (p.group == "tuning" && p.name == "glide") glideZones.push_back (p.zone);
}

juce::AudioProcessorValueTreeState::ParameterLayout DroneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::set<juce::String> seen;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "app.blend", 1 }, "Swarm / Mesh", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return juce::String (juce::roundToInt ((1.0f - v) * 100)) + " / " + juce::String (juce::roundToInt (v * 100)); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "app.modOn", 1 }, "Mod bus on", true));
    MidiMono::addParams (layout);
    int order = 0;
    for (auto* id : { "midi.mode", "midi.attack", "midi.release", "midi.glide" })
        infos.push_back ({ id, "midi", juce::String (id).fromFirstOccurrenceOf (".", false, false), 0, order++ });

    for (auto& s : engines)
    {
        for (auto& p : s->ui.params)
        {
            const auto id = paramId (s->key, p);
            s->ids.push_back (id);
            if (! seen.insert (id).second)
                continue;   // shared contract parameter, already created

            const auto unit = juce::String (p.unit);
            const int dec = decimalsFor (p.step);
            auto attrs = juce::AudioParameterFloatAttributes()
                             .withLabel (unit)
                             .withStringFromValueFunction ([dec, unit] (float v, int)
                             { return juce::String (v, dec) + (unit.isEmpty() ? "" : " " + unit); });
            const auto display = (s->key == "bus" ? juce::String (p.group) + " " : juce::String()) + juce::String (p.name);
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, display, rangeFor (p), p.init, attrs));

            infos.push_back ({ id, s->key == "bus" ? "modbus." + juce::String (p.group) : juce::String (p.group),
                               juce::String (p.name), p.groupOrder, p.order });
        }
    }
    return layout;
}

void DroneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& s : engines)
        s->dsp->init ((int) sampleRate);

    for (auto& b : modBuf) b.assign (CHUNK, 0.0f);
    zeros.assign (CHUNK, 0.0f);
    for (auto& b : bufA) b.assign (CHUNK, 0.0f);
    for (auto& b : bufB) b.assign (CHUNK, 0.0f);
    gainA.assign (CHUNK, 0.0f);
    gainB.assign (CHUNK, 0.0f);

    mono.prepare (sampleRate, samplesPerBlock);
    blendSm.reset (sampleRate, 0.3);
    blendSm.setCurrentAndTargetValue (blendParam->load());
    scopeEvery = juce::jmax (1, (int) (sampleRate / SCOPE_HZ));
    scopeCount = 0;
}

bool DroneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DroneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();

    for (auto& s : engines)
        for (auto& [raw, zone] : s->bind)
            *zone = raw->load (std::memory_order_relaxed);

    const bool modOn = modOnParam->load() > 0.5f;
    blendSm.setTargetValue (blendParam->load());

    const int total = buffer.getNumSamples();
    // MIDI: the note sets the lattice root (folded into the root range), glide from midi.glide
    mono.process (midi, total);
    if (const int note = mono.currentNote(); note >= 0)
    {
        float hz = MidiMono::noteHz (note);
        while (hz > 880.0f) hz *= 0.5f;
        while (hz < 20.0f) hz *= 2.0f;
        for (auto* z : rootZones) *z = hz;
        for (auto* z : glideZones) *z = juce::jmax (0.05f, mono.glide());
    }
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int pos = 0; pos < total; pos += CHUNK)
    {
        const int n = juce::jmin (CHUNK, total - pos);

        float* mod[NMOD];
        for (int c = 0; c < NMOD; ++c) mod[c] = modBuf[(size_t) c].data();
        engines[BUS]->dsp->compute (n, nullptr, mod);

        float* modIn[NMOD];
        for (int c = 0; c < NMOD; ++c) modIn[c] = modOn ? mod[c] : zeros.data();

        // Equal-power blend; an engine whose gain stays at 0 is not computed.
        bool runA = false, runB = false;
        for (int i = 0; i < n; ++i)
        {
            const float b = blendSm.getNextValue();
            gainA[(size_t) i] = b > 0.9999f ? 0.0f : std::cos (b * juce::MathConstants<float>::halfPi);
            gainB[(size_t) i] = b < 0.0001f ? 0.0f : std::sin (b * juce::MathConstants<float>::halfPi);
            runA |= gainA[(size_t) i] > 0.0f;
            runB |= gainB[(size_t) i] > 0.0f;
        }

        float* a[2] = { bufA[0].data(), bufA[1].data() };
        float* b[2] = { bufB[0].data(), bufB[1].data() };
        if (runA) engines[SWARM]->dsp->compute (n, modIn, a);
        if (runB) engines[MESH]->dsp->compute (n, modIn, b);

        for (int i = 0; i < n; ++i)
        {
            const float env = mono.gainAt (pos + i);
            const float ga = runA ? env * gainA[(size_t) i] : 0.0f;
            const float gb = runB ? env * gainB[(size_t) i] : 0.0f;
            const float l = softclip (ga * a[0][i] + gb * b[0][i]);
            const float r = softclip (ga * a[1][i] + gb * b[1][i]);
            outL[pos + i] = l;
            if (outR != nullptr) outR[pos + i] = r;

            if (++scopeCount >= scopeEvery)
            {
                scopeCount = 0;
                int s1, n1, s2, n2;
                scopeFifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 + n2 > 0)
                {
                    auto& d = scopeData[(size_t) (n1 > 0 ? s1 : s2)];
                    for (int c = 0; c < NMOD; ++c) d[(size_t) c] = mod[c][i];
                }
                scopeFifo.finishedWrite (n1 + n2);
            }
        }
        for (int c = 0; c < NMOD; ++c)
            lastMod[(size_t) c].store (mod[c][n - 1], std::memory_order_relaxed);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, total);

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs / juce::jmax (1e-9, total / sr));
    cpu.store (0.95f * cpu.load() + 0.05f * load, std::memory_order_relaxed);
}

float DroneProcessor::nodeMeter (int engine, int n) const
{
    auto* z = engines[(size_t) engine]->meters[(size_t) n];
    return z != nullptr ? *z : 0.0f;
}

int DroneProcessor::popScope (std::array<float, NMOD>* dst, int maxFrames)
{
    int s1, n1, s2, n2;
    scopeFifo.prepareToRead (maxFrames, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) dst[i] = scopeData[(size_t) (s1 + i)];
    for (int i = 0; i < n2; ++i) dst[n1 + i] = scopeData[(size_t) (s2 + i)];
    scopeFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

float DroneProcessor::param (const juce::String& id) const
{
    auto* p = apvts.getRawParameterValue (id);
    return p != nullptr ? p->load() : 0.0f;
}

void DroneProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void DroneProcessor::newSeeds()
{
    auto& rng = juce::Random::getSystemRandom();
    for (auto* id : { "swarm.seed", "mesh.seed", "modbus.bus.seed" })
        setParam (id, (float) rng.nextInt (10000));
}

//------------------------------------------------------------------- presets
const std::vector<FactoryPreset>& DroneProcessor::factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Glass breath", { { "app.blend", 1.0f } } },
        { "Bowed lattice", { { "app.blend", 1.0f }, { "mesh.couple", 1.15f }, { "mesh.decay", 20.0f },
                             { "mesh.drive", 1.5f }, { "mod.toCouple", 0.3f } } },
        { "Swarm cloud", { { "app.blend", 0.0f } } },
        { "Both, slow tide", { { "app.blend", 0.6f }, { "modbus.bus.rate", 0.3f }, { "mod.toFocusX", 1.2f },
                               { "mod.toFocusY", 1.2f }, { "lattice.radius", 0.8f }, { "tuning.glide", 30.0f } } },
        { "Septimal dusk", { { "app.blend", 0.5f }, { "tuning.genY", 7.0f }, { "tuning.root", 41.2f },
                             { "lattice.purity", 0.6f }, { "swarm.tilt", 1.4f }, { "mesh.tilt", 1.4f } } },
        { "Bright stretch", { { "app.blend", 0.3f }, { "tuning.root", 110.0f }, { "swarm.stretch", 0.03f },
                              { "mesh.stretch", 0.03f }, { "swarm.tilt", 0.6f }, { "mesh.tilt", 0.6f } } },
    };
    return presets;
}

int DroneProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String DroneProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : "";
}

void DroneProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    currentPreset = index;
    const auto& pr = presets[(size_t) index];
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        float v = rp->getDefaultValue();
        for (auto& [id, value] : pr.values)
            if (rp->getParameterID() == id)
                v = rp->convertTo0to1 (value);
        // seeds are kept: a preset is a sound shape, the seed picks a variant
        if (rp->getParameterID().endsWith (".seed"))
            continue;
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (v);
        rp->endChangeGesture();
    }
}

void DroneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DroneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = state.getProperty ("preset", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* DroneProcessor::createEditor() { return new DroneEditor (*this); }
