# ARPG Cutaway

An SKSE plugin that lets an ARPG-style camera (high, angled down at the player) see
into Skyrim interiors. Roofs, ceilings and the tops of walls between the camera and
your character are cut away. Floors, furniture and characters stay.

Built for and tested on **Skyrim 1.7.104**. This is a work in progress.

## How it works

There are two modes, set with `[General] Mode` in the INI.

### Mode 1: horizontal cut (default)

Everything above a cut plane is clipped away by the GPU, like the cutaway view in
Diablo-style games. Walls end at a clean line, and roofs and ceilings disappear.

- **Static per area:** the cut height is anchored when you enter a cell and only
  moves on a sustained change in floor level (another storey), gliding there. Stairs
  and bumps don't move it.
- **Far walls stay:** the plane sits at `Height` above the floor at your position
  and rises (`Slope`) going away from the camera, so the walls on the far side of
  the room render full height. The tilt follows the camera's direction. The slope is
  clamped automatically so the plane never dips below the floor anywhere on screen.
- **Per-area overrides:** `[Cells]` sets a different height, or `off`, for
  specific interiors by editor ID.

It hooks the renderer's per-mesh draw call and swaps in a projection matrix whose
near clip plane is tilted to lie flat at the cut height (an *oblique near plane*,
Lengyel 2005). No shaders are touched.

With `AllGeometry = 1`, every main-camera mesh is drawn with that projection, so
every mesh writes consistent depth. With `AllGeometry = 0`, only meshes that cross
the plane get it. That's cheaper, but in rooms where the floor and walls are one
mesh, everything else fails the depth test and vanishes.

### Mode 0: hide whole meshes

Each mesh in a corridor between the camera and the player is classified by height,
size and which side of the player it's on, then hidden with the engine's cull flag.
This is the original approach. It works per mesh, so it can't split a wall, and the
bounding-sphere guesses cause flicker and misclassified walls. It's kept for comparison.

### Background

With the roof gone you'd see the interior fog color (teal) and the sky dome behind it.
`BlackFog` sets the interior fog to black while you're inside, and `HideSky` hides
the sky dome. Both are restored when you leave.

## Settings

All settings live in `Data/SKSE/Plugins/ARPGCutaway.ini`. Restart the game after editing.

| Setting | Default | What it does |
|---|---|---|
| `General.Enabled` | 1 | Master switch |
| `General.InteriorOnly` | 1 | Only run in interior cells |
| `General.Mode` | 1 | 1 = horizontal cut, 0 = hide whole meshes |
| `Cut.Height` | 220 | Cut height above the floor at your position (a human is ~128) |
| `Cut.Radius` | 2500 | Selective cut only: how far from you meshes get cut |
| `Cut.AllGeometry` | 1 | Draw every mesh with the cut projection (consistent depth) |
| `Cut.Slope` | 1.0 | How fast the cut rises behind you (1 = 45°, 0 = flat); clamped to keep the floor |
| `Cut.ReanchorDistance` | 150 | Height change needed before the cut moves (e.g. another floor) |
| `Cut.ReanchorDelay` | 1.0 | Seconds on the ground at the new height before it moves (jumps never count) |
| `Cut.ReanchorSpeed` | 400 | How fast the cut glides to its new height (units/second) |
| `[Cells] <EditorID>` | — | Per-interior cut height, or `off` |
| `Background.BlackFog` | 1 | Black interior fog while inside |
| `Background.HideSky` | 1 | Hide the sky dome while inside |
| `Debug.Log` | 1 | Write diagnostics to the log |

Mode 0 settings (`[Sweep]`, `[Hide]`) are documented in the INI itself.

## Known issues and open questions

- **Effects drawn outside the hooked path**, such as fire, smoke, candle flames and
  spell effects, may still be hidden or show through walls in Mode 1.
- **Screen-space effects** like ambient occlusion read depth assuming the normal
  projection, so they may shade wrong near the cut.
- **Distant surfaces** may z-fight, since the tilted projection loses depth precision far away.
- **Round holes in some floors** in Mode 0. The cause hasn't been confirmed yet;
  probably a ceiling-hung effect mesh that also draws at floor level.
- **Interiors only.** Exteriors are off by default.

## Build

Requires xmake 3+ and Visual Studio 2022 Build Tools (C++ workload).

```
git clone --recurse-submodules https://github.com/nannernick/ARPG-Cutaway
cd ARPG-Cutaway
xmake f -m releasedbg -y
xmake
```

The plugin is written to `build/windows/x64/releasedbg/ARPGCutaway.dll`.

## Install

Copy `ARPGCutaway.dll` and `dist/SKSE/Plugins/ARPGCutaway.ini` into
`Data/SKSE/Plugins/`. Launch with `skse64_loader.exe`. The log is written to
`Documents/My Games/Skyrim Special Edition/SKSE/ARPGCutaway.log`.

### Requirements

- SKSE 2.3.1+ (for 1.7.104)
- Address Library for SKSE Plugins v13+
- Uses [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) (alandtse fork, `ng` branch) as a submodule

## Credits

The idea of using a projection-matrix clip plane comes from machinegod's
*Skyrim Camera Disocclusion*. This plugin is a separate implementation and contains
none of its code.
