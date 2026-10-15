#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "FaustBridge.h"
#include "MidiMono.h"
#include "Widgets.h"

#include <array>
#include <atomic>

// One Faust engine and its bindings to host parameters.
struct EngineSlot
{
    juce::String key;                    // "swarm", "mesh", "bus"
    std::unique_ptr<dsp> dsp;
    ParamCollector ui;
    std::vector<juce::String> ids;       // host parameter id per ui.params entry
    std::vector<std::pair<std::atomic<float>*, float*>> bind;
    std::array<float*, 16> meters {};    // viz/nNN bargraph zones
};

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // others reset to default
};

class DroneProcessor final : public juce::AudioProcessor
{
public:
    enum { SWARM = 0, MESH = 1, BUS = 2 };
    static constexpr int NN = 16, NMOD = 4, CHUNK = 256;
    static constexpr int MESH_MODES = 4;   // res_mesh NP / NN

    DroneProcessor();
    ~DroneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drone Lattice"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- editor access (message thread) ---
    float nodeMeter (int engine, int n) const;
    float modValue (int c) const { return lastMod[(size_t) c].load (std::memory_order_relaxed); }
    int popScope (std::array<float, NMOD>* dst, int maxFrames);
    float cpuLoad() const { return cpu.load (std::memory_order_relaxed); }
    float scopeRate() const { return SCOPE_HZ; }
    const std::vector<ParamInfo>& paramInfos() const { return infos; }
    static const std::vector<FactoryPreset>& factoryPresets();
    void newSeeds();
    float param (const juce::String& id) const;
    void setParam (const juce::String& id, float value);

    std::vector<std::unique_ptr<EngineSlot>> engines;   // before apvts: the layout reads them
    std::vector<ParamInfo> infos;
    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr float SCOPE_HZ = 30.0f;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* blendParam = nullptr;
    std::atomic<float>* modOnParam = nullptr;
    juce::SmoothedValue<float> blendSm;
    MidiMono mono;
    std::vector<float*> rootZones, glideZones;   // tuning/root, tuning/glide of both engines

    std::array<std::vector<float>, NMOD> modBuf;
    std::vector<float> zeros;
    std::array<std::vector<float>, 2> bufA, bufB;
    std::vector<float> gainA, gainB;

    std::array<std::atomic<float>, NMOD> lastMod {};
    juce::AbstractFifo scopeFifo { 4096 };
    std::vector<std::array<float, NMOD>> scopeData = std::vector<std::array<float, NMOD>> (4096);
    int scopeEvery = 1600, scopeCount = 0;

    std::atomic<float> cpu { 0.0f };
    double sr = 48000.0;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DroneProcessor)
};
