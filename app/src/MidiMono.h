// Mono MIDI input shared by the synths: last-note priority with a held-note
// stack, and a gain envelope per sample.
//   Off   : MIDI ignored, gain 1 (the drone plays on its own)
//   Gate  : sounds while a key is held (attack / release)
//   Latch : the first note starts the drone, later notes only change pitch;
//           all-notes-off (CC 123 / 120) releases it
// Parameters "midi.mode", "midi.attack", "midi.release", "midi.glide" are
// added by addParams(); every synth uses the same ids.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>

class MidiMono
{
public:
    enum Mode { Off = 0, Gate = 1, Latch = 2 };

    static void addParams (juce::AudioProcessorValueTreeState::ParameterLayout& L)
    {
        const auto logR = [] (float lo, float hi)
        {
            return juce::NormalisableRange<float> (lo, hi,
                [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
        };
        const auto secs = juce::AudioParameterFloatAttributes().withLabel ("s").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, v < 1.0f ? 3 : 2) + " s"; });
        L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "midi.mode", 1 }, "MIDI mode",
                                                             juce::StringArray { "Off", "Gate", "Latch" }, 0));
        L.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "midi.attack", 1 }, "MIDI attack",
                                                            logR (0.005f, 10.0f), 0.3f, secs));
        L.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "midi.release", 1 }, "MIDI release",
                                                            logR (0.01f, 20.0f), 2.0f, secs));
        L.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "midi.glide", 1 }, "MIDI glide",
                                                            juce::NormalisableRange<float> (0.0f, 5.0f), 0.15f, secs));
    }

    void bind (juce::AudioProcessorValueTreeState& s)
    {
        pMode = s.getRawParameterValue ("midi.mode");
        pAttack = s.getRawParameterValue ("midi.attack");
        pRelease = s.getRawParameterValue ("midi.release");
        pGlide = s.getRawParameterValue ("midi.glide");
    }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        gain.assign ((size_t) juce::jmax (maxBlock, 4096), 1.0f);
        reset();
    }

    void reset()
    {
        held = 0;
        note = -1;
        latched = false;
        env = 0.0f;
    }

    int mode() const { return pMode != nullptr ? (int) pMode->load() : Off; }
    float glide() const { return pGlide != nullptr ? pGlide->load() : 0.0f; }
    // MIDI note that sets the pitch, -1 before the first note or when Off
    int currentNote() const { return mode() == Off ? -1 : note; }
    float gainAt (int i) const { return gain[(size_t) juce::jmin (i, (int) gain.size() - 1)]; }

    // Runs the envelope over n samples; gains()[i] is valid for i < n.
    void process (const juce::MidiBuffer& midi, int n)
    {
        const int m = mode();
        n = juce::jmin (n, (int) gain.size());
        if (m == Off)
        {
            std::fill (gain.begin(), gain.begin() + n, 1.0f);
            env = 1.0f;
            latched = false;
            return;
        }
        const float ka = coeff (pAttack->load()), kr = coeff (pRelease->load());
        int i = 0;
        auto run = [&] (int upTo)
        {
            const bool on = m == Latch ? latched : held > 0;
            const float target = on ? peak : 0.0f;
            const float k = target > env ? ka : kr;
            for (; i < upTo; ++i)
            {
                env += (target - env) * k;
                gain[(size_t) i] = env;
            }
        };
        for (const auto meta : midi)
        {
            run (juce::jlimit (0, n, meta.samplePosition));
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                push (msg.getNoteNumber());
                note = msg.getNoteNumber();
                peak = 0.4f + 0.6f * msg.getFloatVelocity();
                latched = true;
            }
            else if (msg.isNoteOff())
            {
                pop (msg.getNoteNumber());
                if (held > 0) note = stack[(size_t) held - 1];   // legato back to the previous key
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                held = 0;
                latched = false;
            }
        }
        run (n);
    }

    static float noteHz (int n) { return 440.0f * std::pow (2.0f, (float) (n - 69) / 12.0f); }

private:
    float coeff (float seconds) const
    {
        // reaches ~95 % of the step in `seconds`
        return 1.0f - std::exp (-3.0f / (juce::jmax (0.001f, seconds) * (float) sr));
    }

    void push (int n)
    {
        pop (n);
        if (held < (int) stack.size()) stack[(size_t) held++] = n;
    }

    void pop (int n)
    {
        for (int i = 0; i < held; ++i)
            if (stack[(size_t) i] == n)
            {
                for (int j = i; j + 1 < held; ++j) stack[(size_t) j] = stack[(size_t) j + 1];
                --held;
                return;
            }
    }

    std::atomic<float> *pMode = nullptr, *pAttack = nullptr, *pRelease = nullptr, *pGlide = nullptr;
    double sr = 48000;
    std::vector<float> gain;
    std::array<int, 16> stack {};
    int held = 0, note = -1;
    bool latched = false;
    float env = 0.0f, peak = 1.0f;
};
