# Dynamic Occlusion Test

SKSE plugin for Skyrim SE/AE (built and tested against runtime 1.7.104) that hides
geometry above the player when it sits between an ARPG-style camera and the
character: roofs, ceilings, upper floors, tree canopies.

Instead of a clip plane, it hides individual meshes, so floors and walls stay intact.

## How it works (v0)

Every `Sweep.Interval` seconds:

1. Build a corridor from the camera to `Sweep.HeadHeight` above the player's feet,
   padded by `Sweep.CorridorRadius`.
2. Walk every loaded reference whose 3D reaches into the corridor, down to its
   individual meshes.
3. Hide a mesh when its bound center is at least `Hide.MinHeight` above the
   player's feet, its radius is at most `Hide.MaxShapeRadius`, and it touches the corridor.
4. Restore it `Hide.Linger` seconds after it stops qualifying. Everything is
   restored on cell change and save load.

`Background.BlackFog` is an experiment: it sets the interior's fog colors to black
while you're inside, to test whether that is what shows through removed roofs.

## Build

Requires xmake 3+ and Visual Studio 2022 Build Tools (C++ workload).

```
git clone --recurse-submodules https://github.com/nannernick/Dynamic-Occlusion-Test
cd Dynamic-Occlusion-Test
xmake f -m releasedbg -y
xmake
```

Output: `build/windows/x64/releasedbg/DynamicOcclusion.dll`.
Install it and `dist/SKSE/Plugins/DynamicOcclusion.ini` into `Data/SKSE/Plugins/`.

## Requirements

- SKSE 2.3.1+ (for 1.7.104)
- Address Library for SKSE Plugins v13+
