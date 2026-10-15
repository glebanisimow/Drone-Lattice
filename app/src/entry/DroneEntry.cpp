// Plugin / standalone entry of Drone Lattice (the Studio links the processor directly).
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DroneProcessor(); }
