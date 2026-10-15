# Drone Lattice

Experimental drone synth built on host-independent Faust engines. Visual form =
interface = sound. Standalone app + VST3 (Windows).

| Engine | File | Inputs | Outputs |
|---|---|---|---|
| JI lattice + additive swarm | `engines/lattice_swarm.dsp` | 4 (mod bus, -1..1) | 2 |
| Coupled resonator mesh on the lattice | `engines/res_mesh.dsp` | 4 (mod bus, -1..1) | 2 |
| Mod bus: seeded LFO / OU walk / Lorenz chaos, 1 s..10 min | `engines/mod_bus.dsp` | 0 | 4 (mod bus, -1..1) |

`lib/contract.lib` holds the shared contract (tuning lattice, mod bus, seeded
RNG, smoothing, output stage).

## Build (Windows)

Needs VS 2022 C++ Build Tools, CMake, and Faust + JUCE under
`%LOCALAPPDATA%\drone-tools`. Faust code is generated at build time from
`engines/*.dsp`.

```powershell
.\app\build.ps1              # Release build + dist\DroneLattice-Setup-<ver>.exe
.\app\build.ps1 -NoInstaller
```

Output: `Drone Lattice.exe` (Standalone) and `Drone Lattice.vst3`.

---
Split out of the `drone-lattice` monorepo; the Ward instrument and the Drone
Studio host live in their own repositories.
