# EMBERHOME — instructions for Claude Code

Read this file in full before changing anything. It is the standing spec for the project.

## What this is

A side-scrolling puzzle-platformer in Unreal Engine 5.8 (C++), for Android, iOS and Windows, with a
static marketing website. A small child in a scarf carries a lantern through a grey, rainy forest;
the lantern is the only warm colour in the world. Eleven levels; reach the lamp post to "bring the light home".

**Names.** The game is called **EMBERHOME** (renamed from HOLLOWLIGHT on 2026-09-29 because that name
was already taken on Google Play and the App Store). Everything players see says EMBERHOME; the app id is
`com.brainrotinteractive.emberhome`, publisher Brainrot Interactive Studios. The internal code name is
still `Hollowlight` (folder, .uproject, module, `HL` class prefix, save slot, packaged file names) —
invisible to players and deliberately not renamed.

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
- 400 world units of screen height, ground top y = 300 (y grows down). Gravity 1500, jump 560, run 215 (the brief said 240; slowed at the
  user's request, 2026-10-02),
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
| 2 | A Stray | hills; the dog joins; a lever, then a lever only the dog can reach |
| 3 | Teeth in the Grass | trap fields; push the crate over traps to spring them |
| 4 | Over the Ridge | slopes, a cliff ladder, a winch handle carried down to a drawbridge |
| 5 | Still Water | water too wide to jump: push a crate in and it floats |
| 6 | The Warehouse | a plate the dog holds, a crowbar up on the racking, a crate parked on a plate |
| 7 | Sidings | railway yard: slide under a wagon, the dog into the signal hut, a swing bridge |
| 8 | The Station | luggage on one plate, the dog on another, a footbridge, a crowbar gate, a ladder |
| 9 | The Mountain | rock terraces to climb by the ledges, a ladder, scree slopes you slide down, a chasm to jump mid-slide |
| 10 | The Storm | everything, with lightning and heavy rain |
| 11 | Homecoming | calmer; the rain thins, dawn warms the sky, a village lamp post |

Puzzle levels carry a `Solution` script (go here, ACT, wait for the gate, climb...) that the autopilot
follows; that script is the proof the level can be solved. `run.ps1 -base` (no script) must *fail* on a
puzzle level - if the heuristic alone gets through, the puzzle does not block. Checkpoints are clean
cuts: nothing behind one may be needed to finish from it.

**Every level must be finishable by the autopilot without dying, from the start and from every
checkpoint.** After any level or physics change run the harness (seconds) and then the automation tests:

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1            # all levels + all checkpoints
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 -Level 7 -Trace
```
The harness also has `-base` (heuristic only, no look-ahead) and `-fine <t0> <t1>` (per-step trace).
Reach at full run: a full jump rises ~102 units and carries ~160 horizontally (~198 at a sprint); a crate top is 56 above
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
`hl.Title`, `hl.UnlockAll`, `hl.ResetProgress`, `hl.Autopilot 0|1`, `hl.HideUI 0|1`, `hl.Debug.Pose <n>`.

`DisableEnginePluginsByDefault` is on in the .uproject: engine plugins are opt-in. Add one only if the
game needs it at runtime, and mark platform-specific ones `Optional` so the project still loads on a
machine without that platform installed.

## Platforms

- **Windows**: packages and runs (Shipping build verified with the capture script).
- **Android**: configured (`com.brainrotinteractive.emberhome`, landscape, arm64, Vulkan + ES3.1, min SDK 26,
  target 35, icons in `Build/Android/res`). Needs the Android target platform in the Epic launcher and
  Android Studio/SDK/NDK — see `SETUP.md`. Not yet built on this machine.
- **iOS**: configured (`com.brainrotinteractive.emberhome`, landscape, Metal, icon in `Build/IOS/Resources`).
  Needs the iOS target platform, a Mac with Xcode and an Apple Developer account — see `SETUP.md`.
- Touch controls appear automatically on phones/tablets (Settings: Auto / On / Off), respect the
  notch safe area, and use generous hit zones. The app pauses and saves when sent to the background.
- **Website**: `Website/` is a static site (no third-party requests). `.github/workflows/pages.yml`
  deploys it to GitHub Pages on push to main. `Website/privacy.html` is the store privacy-policy URL.

## EMBERHOME 2.0 (asked for 2026-10-02) — staged update

The user wants Vector-style movement, LIMBO-style thinking puzzles, a companion animal, and new settings.
Inspiration only: original levels and animation, nothing copied. Ship each stage to the closed-test track.

| Stage | Content | Status |
|---|---|---|
| 1 | Touch calibration ("touch the light"), visible traps | done (version code 3) |
| 2 | Movement: sprint momentum, slide/crouch, vault, ledge grab + pull-up, roll | done (version code 3) |
| 3 | Companion **dog** (user's choice), ladders, levers, plates, tools you carry and use, ACT button | done (version code 4, 2.0.0) |
| 4 | Sloped terrain, warehouse and railway-station settings, 5 new puzzle levels replacing the 5 most repetitive | done (version code 4, 2.0.0) |

Decisions: companion is a **dog** (sniffs out/barks at traps, holds pressure plates, squeezes through gaps to pull
levers, fetches tools; it should also hint at how a level works). Controls are **buttons and swipes together**
(pads: left/right, JUMP, SLIDE, ACT; swipe up = jump, swipe down = slide). ACT is E / F / gamepad Y.

Movement rules now in the core (`HLTypes.h`): sprint 215 -> 265 after ~0.35 s of unbroken running; "down" at
>= 130 speed is a slide (body 22 high, 1.15 s, no steering; lengthened and the run slowed after the user's
phone test), otherwise a crouch (crawl 90); obstacles up to 36
high are vaulted at a run; in the air, hands within 18 below a ledge top catch it and pull up (0.1 s hang +
0.3 s climb, "down" lets go), so ledges up to ~160 above the ground are climbable without a crate; landings
faster than 720 need a roll (moving or holding down) or cost a 0.22 s stumble. The autopilot slides when
standing height is blocked but 22 is clear, and goes back for a crate it left behind at wide water.

Touch calibration: first launch on a touch device (and Settings -> CALIBRATE TOUCH) shows two targets; the taps
give `reported = Scale * true + Offset` per axis, stored in the save and inverted for every touch in
`AHLPlayerController::GatherInput`. Corrections smaller than a fingertip's wobble are ignored. This exists
because the user's phone reported touches offset from where they landed and would not be connected for
measurement — do not remove it without a real device test.

Things to operate (`HLLevel.h`, `FSim::DoAct` / `UpdateMechanisms` / `UpdateDog`):
- **ACT** does the first that applies: use the carried tool on a socket in reach, pick up a tool, throw a lever,
  otherwise whistle. One tool is carried at a time. A whistle inside a `FDogTaskDef` range sends the dog to
  throw that lever by a way only it fits; anywhere else it toggles the dog between *stay here* and *follow*.
- **Gates** are doors (solid while shut, slide up) or, with `bBridge`, drawbridge decks (walkable once down).
  A lever or a tool latches one open for good; a **plate** holds it open only while the child, a crate or the
  dog stands on it.
- **Slopes** are thin `bStep` grounds (collision) under one `FSlopeDef` (drawing). **Ladders**: hold jump to
  climb, down to descend.
- **The dog** cannot be hurt and never springs a trap. It follows, points at open jaws ahead and barks, fits
  under a 16-high gap, and catches up with a poof if left behind. If the child dithers inside a `FHintDef`
  range it runs to the thing that matters and barks at it - a nudge, never the whole answer.
- **Scree** (`AddScree`, `FGroundDef::Slide`, added 2026-10-02 for the mountain level the user asked for): a
  slope too steep to stand on. The child is put into a slide and pushed downhill up to 320, no steering; a
  jump still works (that is how the chasm is crossed), and the slide carries on under low things at the
  foot. It cannot be walked up. Going down any slope the feet are snapped to the next step (child and dog)
  so nobody hops down a hillside.
- The dog is drawn from a small skeleton (`DrawDog`): shoulder and hip, two-bone legs solved to planted paws
  (hind legs with a hock), trot and gallop gaits, sit, point, crawl under low gaps, paws on the hillside line.
- Settings (`FTheme.Setting`): Forest, Warehouse, Railway, Station, Mountain change only the backdrop layers, the floor
  edge and how blocks are dressed (racking, wagons); still monochrome, still black silhouettes.

## Checklist

- [x] Core sim, eleven levels, autopilot; harness + automation tests green
- [x] Renderer, UI, audio, save, input (keyboard, gamepad, mouse, touch)
- [x] Windows Shipping package runs end-to-end
- [x] Icons, website, privacy policy, store listing copy (`STORE_LISTING.md`)
- [ ] Android: install platform + SDK (human), `package.ps1 -Platform Android`, test on a device
- [ ] iOS: Mac + Xcode + signing (human), build, test on a device
- [ ] Store accounts, listings and submission (human)
- [ ] Website: push to GitHub and enable Pages (needs the user's go-ahead), swap "coming soon" for store links
