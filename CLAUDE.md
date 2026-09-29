# HOLLOWLIGHT — instructions for Claude Code

Read this file in full before changing anything. It is the standing spec for the project.

## What this is

A side-scrolling puzzle-platformer in Unreal Engine 5.8 (C++), for Android, iOS and Windows, with a
static marketing website. A small child in a scarf carries a lantern through a grey, rainy forest;
the lantern is the only warm colour in the world. Ten levels; reach the lamp post to "bring the light home".

The original design brief was a single-file HTML5 canvas game. The user chose Unreal instead
(2026-09-28). The brief's art direction, physics numbers, level 1 layout and attract mode are kept
exactly; everything else (levels 2-10, menus, audio, save, mobile) was added on top.

## Architecture

```
Private/Core    engine-agnostic C++ (no Unreal headers): HLTypes, HLLevel(s), HLSim, HLAutopilot
Private/Game    Unreal wiring: AHLGameMode, AHLPlayerController (input, lifecycle, capture), AHLHUD,
                FHLGame (screens, fixed-step loop, camera, weather, progress), UHLSaveGame
Private/Render  FHLDraw (batched canvas triangles), FHLWorldRenderer (everything in the world)
Private/UI      FHLUI (menus, HUD, touch controls, procedural serif title)
Private/Audio   UHLAudioSynth (USynthComponent: all sound synthesised live)
Private/Tests   automation tests
```

- **Core must never include Unreal headers.** It is also compiled by `Tools/SimHarness` with plain MSVC,
  which is how levels and the autopilot are iterated in seconds instead of minutes.
- **No authored assets.** Nothing is imported or created in the editor: the game runs on the engine's
  `/Engine/Maps/Entry` map, world rendering is disabled, and `AHLHUD` paints every frame with canvas
  triangles. The only texture is the film-grain tile, created at runtime. The only engine asset used
  is the Roboto font. Keep it that way — it is what makes the game identical and cheap on every platform.
- The sim is deterministic and copyable; the autopilot clones it to look ahead. Anything that breaks
  copyability or determinism (pointers into other objects, wall-clock reads, randomness) breaks the autopilot.
- Every class/file gets a one-line comment pointing back to the CLAUDE.md section it belongs to, as the
  existing files do.

## Rules from the design brief (do not change without the user)

- Monochrome grey world; the play layer is pure black silhouettes. The lantern (#ffc46e) is the only warm colour.
  Warm light is drawn *behind* the play layer so silhouettes stay black (level 10's dawn and the village
  windows are the deliberate exceptions).
- Sky gradient #c4c5bf -> #5e5f5a. Tree layers at scroll speeds 0.18/0.38/0.62/0.85 in #a3a49e, #7b7c77,
  #4a4b47, #22231f; foreground at 1.35. Trees are seeded per cell so they never change. Fog bands, light
  shafts, ~170 rain streaks, lantern light (dark overlay with a hole + additive amber + flicker + embers),
  vignette, animated grain.
- 400 world units of screen height, ground top y = 300 (y grows down). Gravity 1500, jump 560, run 240,
  push 115, fixed 1/120 s substeps, coyote 0.1 s, jump buffer 0.14 s, early release = short hop.
  Collision: one axis at a time, block only when entering a solid from the side you move toward.
- Player collision box 18 x 44. Crate 56 x 56. Traps 40 wide. Log: rope 212, +/-0.95 rad, 2.6 s.
- Camera follows horizontally with smoothing, child ~36% from the left.
- Death: lantern goes out, fade to black, respawn at the last checkpoint; traps reset, used crates stay.
- Attract mode: the autopilot plays behind the title; level 1 must finish in about 20 s without dying.

## Levels (`Private/Core/HLLevels.cpp`)

| # | Name | Introduces |
|---|---|---|
| 1 | The Edge of the Wood | the brief's level exactly: pits, crate + ledge, jaw traps, one log |
| 2 | Stepping Stones | stones over a gully, a stump that needs the crate, a jumpable pond |
| 3 | Teeth in the Grass | trap fields; push the crate over traps to spring them |
| 4 | The Hanging Wood | many logs, two swinging against each other |
| 5 | Drifting Boughs | moving platforms over wide gaps, a lift to a high shelf |
| 6 | Rotten Branches | branches that give way 0.55 s after you land |
| 7 | Still Water | water too wide to jump: push a crate in and it floats |
| 8 | Into the Canopy | lifts into the canopy (vertical camera), rotten branches down |
| 9 | The Storm | everything, with lightning and heavy rain |
| 10 | Homecoming | calmer; the rain thins, dawn warms the sky, a village lamp post |

**Every level must be finishable by the autopilot without dying, from the start and from every
checkpoint.** After any level or physics change run the harness (seconds) and then the automation tests:

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1            # all levels + all checkpoints
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 -Level 7 -Trace
```
The harness also has `-base` (heuristic only, no look-ahead) and `-fine <t0> <t1>` (per-step trace).
Reach at full run: a full jump rises ~102 units and carries ~179 horizontally; a crate top is 56 above
the ground; water surface 339 makes a floating crate's top flush with ground at 300.

## Build, test, run (Windows, UE 5.8 launcher build)

Engine root `C:\Program Files\Epic Games\UE_5.8` (override with `UE_ROOT`).

```
# Regenerate project files (after adding/removing .cpp files)
"<Engine>\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<repo>\Hollowlight.uproject" -game -rocket
# Compile the editor target
"<Engine>\Engine\Build\BatchFiles\Build.bat" HollowlightEditor Win64 Development -project="<repo>\Hollowlight.uproject" -waitmutex
# All automation tests, with a pass/fail summary
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1 [-NoBuild]
# Visual check: the autopilot plays every level and screen, screenshots in Saved/Screenshots/WindowsEditor/
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1 [-Phone]
# Package (Packaged/<Platform>/)
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64|Android|IOS [-Release]
```

Command line: `-HLLevel=N` starts level N, `-HLCapture [-HLCaptureTag=x]` runs the capture script and
quits, `-HLForceTouch` shows the touch pads on desktop. Console: `hl.Play <level> [checkpoint]`,
`hl.Title`, `hl.UnlockAll`, `hl.ResetProgress`, `hl.Autopilot 0|1`, `hl.HideUI 0|1`.

`DisableEnginePluginsByDefault` is on in the .uproject: engine plugins are opt-in. Add one only if the
game needs it at runtime, and mark platform-specific ones `Optional` so the project still loads on a
machine without that platform installed.

## Platforms

- **Windows**: packages and runs (Shipping build verified with the capture script).
- **Android**: configured (`com.brainrotinteractive.hollowlight`, landscape, arm64, Vulkan + ES3.1, min SDK 26,
  target 35, icons in `Build/Android/res`). Needs the Android target platform in the Epic launcher and
  Android Studio/SDK/NDK — see `SETUP.md`. Not yet built on this machine.
- **iOS**: configured (`com.brainrotinteractive.hollowlight`, landscape, Metal, icon in `Build/IOS/Resources`).
  Needs the iOS target platform, a Mac with Xcode and an Apple Developer account — see `SETUP.md`.
- Touch controls appear automatically on phones/tablets (Settings: Auto / On / Off), respect the
  notch safe area, and use generous hit zones. The app pauses and saves when sent to the background.
- **Website**: `Website/` is a static site (no third-party requests). `.github/workflows/pages.yml`
  deploys it to GitHub Pages on push to main. `Website/privacy.html` is the store privacy-policy URL.

## Checklist

- [x] Core sim, ten levels, autopilot; harness + automation tests green
- [x] Renderer, UI, audio, save, input (keyboard, gamepad, mouse, touch)
- [x] Windows Shipping package runs end-to-end
- [x] Icons, website, privacy policy, store listing copy (`STORE_LISTING.md`)
- [ ] Android: install platform + SDK (human), `package.ps1 -Platform Android`, test on a device
- [ ] iOS: Mac + Xcode + signing (human), build, test on a device
- [ ] Store accounts, listings and submission (human)
- [ ] Website: push to GitHub and enable Pages (needs the user's go-ahead), swap "coming soon" for store links
