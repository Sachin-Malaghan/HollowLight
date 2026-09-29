# Game playbook — how HOLLOWLIGHT was built, from idea to Play Store

A reusable recipe for building the next game the same way. Written from the real build of HOLLOWLIGHT
(`C:\SACHIN\Hollowlight`, https://github.com/Sachin-Malaghan/HollowLight). Read it in full before
starting a new game; follow the order; reuse the tools and fixes rather than rediscovering them.

---

## 0. The machine (already set up — just verify)

| Tool | Where / version |
|---|---|
| Unreal Engine 5.8 (Launcher build) with **Android** target platform | `C:\Program Files\Epic Games\UE_5.8` |
| Visual Studio 2022, "Game development with C++" | builds via UE's `Build.bat` |
| Android Studio + SDK (platform android-36, build-tools 36.0.0, NDK 27.2.12479018, cmdline-tools) | `%LOCALAPPDATA%\Android\Sdk` (installed by `Engine\Extras\Android\SetupAndroid.bat`) |
| JDK 21 (Microsoft) — Gradle can't run on Android Studio's Java 25 | `C:\Program Files\Microsoft\jdk-21.0.12.101-hotspot`, wired via `%USERPROFILE%\.gradle\gradle.properties` → `org.gradle.java.home=...` |
| Node, Python-free; PowerShell 5.1 + Git Bash | |
| Avast antivirus (HTTPS scanning) | breaks Java downloads — handled in `package.ps1` (see §8) |
| GitHub account `Sachin-Malaghan`, Play Console developer account (Brainrot Interactive Studios) | |

iOS needs a Mac + Xcode + Apple Developer account — not available yet. Configure iOS settings anyway.

---

## 1. Decide the shape of the game first

Ask/settle these before writing code (they change everything):
- **Engine & targets.** Unreal (C++) for Android + iOS + Windows. Unreal can't export to the web, so the
  website is a landing page, not a playable build.
- **Art direction that suits procedural drawing.** HOLLOWLIGHT is 2D silhouettes, gradients and glows, so it
  needed zero imported assets. If the new game needs 3D meshes/textures, budget for an asset pipeline
  (Blender scripts / Python import) instead of the canvas approach below.
- **New folder, new git repo** per game (e.g. `C:\SACHIN\<GameName>`), never inside another project.
- **Publisher identity:** app id `com.brainrotinteractive.<game>`, company "Brainrot Interactive Studios".
  The app id is **permanent** after the first Play upload — set it on day one.

---

## 2. Architecture that worked

```
Source/<Game>/Private/Core     engine-agnostic C++ (no Unreal headers): types, levels, deterministic sim, autopilot
Source/<Game>/Private/Game     Unreal wiring: GameMode (no pawn), PlayerController (input, lifecycle, capture), HUD, game flow, SaveGame
Source/<Game>/Private/Render   batched canvas triangles (FHLDraw) + world renderer
Source/<Game>/Private/UI       menus, HUD overlay, touch pads, procedural title glyphs
Source/<Game>/Private/Audio    USynthComponent subclass: all sound synthesised live
Source/<Game>/Private/Tests    automation tests
Tools/SimHarness               builds Core with plain MSVC (cl.exe) — runs in seconds
Tools/Validation               run_tests.ps1, capture.ps1 (screenshots of every screen)
Tools/Build                    package.ps1, make_icons.ps1, store_graphics.ps1, website_shots.ps1, create_upload_key.ps1
Website/                       static site + privacy policy; .github/workflows/pages.yml deploys it
CLAUDE.md, SETUP.md, STORE_LISTING.md, README.md
```

Key ideas:
1. **Engine-agnostic core.** Gameplay/physics/levels in plain C++ (`std::vector`, `double`), deterministic,
   fixed timestep (1/120 s), **copyable by value**. Consequences: a standalone harness compiles it with
   `cl` in seconds, and an automation test runs the same code inside Unreal.
2. **An autopilot that proves every level is beatable.** A simple heuristic policy + receding-horizon
   look-ahead (clone the sim, simulate ~1.6 s for a few candidate moves, reject ones that die). It doubles
   as the attract mode behind the title and as the level-validation test (from the start *and* from every
   checkpoint). Iterating levels = edit `Levels.cpp` → run harness → read trace.
3. **No authored assets (for a 2D/stylised game).** Game runs on `/Engine/Maps/Entry`,
   `GameViewportClient->bDisableWorldRendering = true`, and `AHUD::DrawHUD` paints everything with
   `FCanvasTriangleItem` batches (vertex colours, translucent/additive blend). Identical on all platforms,
   cheap on phones, no editor work, everything reviewable as code. Fonts: engine Roboto via `FSlateFontInfo`
   + letter spacing; a serif title drawn as geometry.
4. **Procedural audio** in a `USynthComponent` (`OnGenerateAudio`): voices with envelope + tone sweep +
   band-passed noise, rain bed, drone + sparse pentatonic notes, simple Schroeder reverb; game thread
   pushes commands through a lock-free `TQueue`, mix targets through `std::atomic`.
5. **Game flow in plain C++** (`FHLGame`): screens enum, button list rebuilt by the UI every frame and
   hit-tested next frame (mouse, touch tap, keyboard/gamepad focus), save via `USaveGame`, pause + save on
   `ApplicationWillEnterBackgroundDelegate`/`WillDeactivate`.
6. **Input by polling** in `PlayerTick` (`IsInputKeyDown`, `WasInputKeyJustPressed`,
   `GetInputTouchState` for 10 fingers). Touch pads: generous hit zones, first-touch decides control vs UI,
   a tap (small move) on release clicks UI. Safe-area insets from `FDisplayMetrics::TitleSafePaddingSize`.
7. **Platform exits:** QUIT button on desktop only; Android Back on the title quits; iOS has none (Apple rule).

---

## 3. Build order (do phases in this order; verify each before the next)

1. Core types + sim + first level exactly as specified → harness passes.
2. Autopilot → first level finishes without dying (and in the expected time).
3. Remaining levels, one mechanic introduced per level, then mixed → harness: every level + every checkpoint.
4. Unreal project files (uproject, targets, Build.cs, configs) → editor target compiles.
5. Renderer → capture screenshots and **look at them**; fix visuals.
6. UI/menus, save, audio, input (keyboard, gamepad, mouse, touch).
7. Automation tests (levels, checkpoints, physics numbers) → `run_tests.ps1` green.
8. Windows Shipping package → run it end to end with the capture script (proves cooking/fonts work).
9. Trim plugins (§6), icons, website, privacy policy, store copy.
10. Android APK → install on a real phone → fix touch/aspect issues (§8) → release bundle.

Always verify by running things: harness, tests, screenshots, packaged build. Read screenshots after every
visual change — several real bugs were only visible in them.

---

## 4. Commands (Windows, UE 5.8 launcher build)

```
# project files (after adding/removing .cpp files)
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<repo>\<Game>.uproject" -game -rocket
# compile editor target (close the editor first: Live Coding blocks outside builds)
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" <Game>Editor Win64 Development -project="<repo>\<Game>.uproject" -waitmutex
# tests headless (log: Saved\Logs\<Game>.log)
"...\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\<Game>.uproject" -nullrhi -unattended -nosplash -nopause -ExecCmds="Automation RunTests <Game>; Quit" -TestExit="Automation Test Queue Empty"
# play without packaging
"...\Engine\Binaries\Win64\UnrealEditor.exe" "<repo>\<Game>.uproject" -game -windowed -ResX=1600 -ResY=900
# package
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Win64|Android [-Config Development|Shipping] [-Release]
```
Copy `Tools/` from HOLLOWLIGHT and rename — the scripts are generic apart from the project name.

---

## 5. Validation tooling worth copying

- **SimHarness** (`Tools/SimHarness/run.ps1`): `-Level N`, `-Trace`, `-base` (heuristic only),
  `-fine t0 t1` (per-step state dump). Found every autopilot/level bug in minutes.
- **Capture script** (`-HLCapture` command-line flag in the PlayerController): a table of
  {level, checkpoint, screen, hideUI, wait, name}; the autopilot plays, screenshots are requested with
  `FScreenshotRequest::RequestScreenshot(path, true, false)`, then quit. **Wait one frame after requesting a
  shot before changing state**, or you capture the next screen at 0% fade-in. Tags: `-HLCaptureTag=phone`
  with `-HLForceTouch` and a 1560x720 window for phone-shaped shots.
- Console commands (`FAutoConsoleCommandWithWorldAndArgs`): play level N, unlock all, reset progress,
  autopilot on/off, hide UI.
- Website/store images are generated from captures (`website_shots.ps1`, `store_graphics.ps1`).

---

## 6. Unreal gotchas we hit (and the fixes)

| Symptom | Cause | Fix |
|---|---|---|
| `FEvent` ambiguous symbol | Unreal has `FEvent` (threading) | qualify your own type (`HL::FEvent`) or avoid the name |
| Member function name clashes with a constant (`Dim`) | shadowing is an error in UE | rename |
| Build fails "Unable to build while Live Coding is active" | editor open | close editor, rebuild |
| Package 373 MB | 174 default engine plugins | `"DisableEnginePluginsByDefault": true` in .uproject, enable only what's needed (EnhancedInput, XInputDevice, device-profile selectors; mark Android/iOS ones `"Optional": true`) → 170 MB Win, 63 MB Android AAB |
| Floating virtual joysticks on Android swallow taps | engine-generated `Config/DefaultInput.ini` sets `DefaultTouchInterface=/Engine/MobileResources/HUD/DefaultVirtualJoysticks...` (InputSettings live in **DefaultInput.ini**, not DefaultEngine.ini) | set `DefaultTouchInterface=None` there **and** override `virtual void CreateTouchInterface() override {}` in the PlayerController |
| Taps land right of the finger; black strip on one side | UE default `MaxAspectRatio=2.1` letterboxes 20:9 phones but maps touches against the full width | `[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings] MaxAspectRatio=3.0` and `bUseDisplayCutout=True` |
| APK launches with "Failed to open descriptor file ...uproject" | BuildCookRun without `-package` (staged data never goes into the APK) | always pass `-package` |
| Jump height test off by ~2% | semi-implicit Euler at 1/120 s peaks `v·dt/2` below `v²/2g` | test against the discrete formula |
| Additive glow turns black silhouettes brown | glow drawn over the play layer | draw big warm light *behind* the play layer; only small cores on top |
| Visible rim on glows | linear falloff | several rings with `(1-r)^2` falloff |

Config essentials: `GameDefaultMap=/Engine/Maps/Entry`, `GlobalDefaultGameMode=/Script/<Game>.<Mode>`,
`+MapsToCook=(FilePath="/Engine/Maps/Entry")`, `+DirectoriesToAlwaysCook=(Path="/Engine/EngineFonts")`,
`r.MobileContentScaleFactor=0` (native resolution), Android: arm64, Vulkan+ES3.1, MinSDK 26, TargetSDK 36,
`bEnableBundle=True`, `bEnableUniversalAPK=True`, landscape.

---

## 7. Android build: every problem on this PC and its fix

1. **Epic Launcher lacked the Android platform** → Launcher → UE 5.8 → Options → tick Android.
2. **SetupAndroid.bat needs cmdline-tools** → Android Studio → SDK Manager → SDK Tools → Command-line Tools.
3. **"PKIX path building failed"** during Gradle download → Avast Web Shield re-signs HTTPS.
   `package.ps1` copies the JDK's `cacerts`, imports the scanner roots from the Windows store
   (`Cert:\LocalMachine\Root`, subject matching Avast/AVG/Kaspersky/ESET/Bitdefender/Zscaler) with keytool,
   and sets `JAVA_TOOL_OPTIONS=-Djavax.net.ssl.trustStore=<copy>`. (`trustStoreType=Windows-ROOT` does NOT
   work with Android Studio's JBR.) Note keytool prints success on stderr — don't let `ErrorActionPreference=Stop` kill it.
4. **"Unsupported class file major version 69"** → Gradle 8 on Java 25. UE ignores JAVA_HOME changes here
   (it hard-codes Android Studio's `jbr`); the fix that works is Gradle's own
   `%USERPROFILE%\.gradle\gradle.properties`: `org.gradle.java.home=C:/Program Files/Microsoft/jdk-21...`.
5. After changing the **package name**, delete `Intermediate\Android` and `Build\Android\src` (stale
   generated Java under the old package breaks the build: "package R does not exist").

Outputs: `Packaged\Android\<Game>-arm64.apk` (Development) or `<Game>-Android-Shipping.aab` +
`_universal.apk` (Shipping). Install: copy the APK to the phone and tap it, or `adb install`.
Uninstall the old app first when the package name changes.

---

## 8. Mobile UX checklist (learned on a real phone)

- No engine virtual joystick; draw your own pads, large hit zones, show pressed state.
- Fill the whole screen (MaxAspectRatio 3.0, display cutout on) and keep UI inside safe-area insets.
- Menu targets ≥ ~11% of screen height; a touch ring under the finger helps (and diagnoses offsets).
- Pause + save on background; Android Back = back/pause, quits on the title.
- Test on the phone early — touch mapping bugs never show on desktop.

---

## 9. Release: Google Play

Prepared in HOLLOWLIGHT (copy the same files):
- `Tools/Build/create_upload_key.ps1` — the **owner** runs it and types the password; writes
  `Build/Android/<game>-upload.keystore` and `Config/Android/AndroidEngine.ini` (both git-ignored). Back both up.
- `package.ps1 -Platform Android -Release` → signed `.aab` (refuses to run without the key).
- `Tools/Build/store_graphics.ps1` → feature graphic 1024x500, screenshots within Play's 2:1 limit, 512 icon.
- `STORE_LISTING.md` → short/full description, keywords, and answers for Data safety (no data), ads (none),
  content rating, target audience.
- Privacy policy page on the website (required).
Play rules to remember: $25 one-time account; ID + phone verification; **new personal accounts must run a
closed test with ≥12 testers for 14 consecutive days** before production; target a recent API level
(36 in 2026); raise `StoreVersion` for every upload; the app id can never change.

Website: static `Website/` + `.github/workflows/pages.yml`; enable GitHub → Settings → Pages → Source:
GitHub Actions. No third-party scripts/fonts/analytics so the privacy policy can say "no data collected".

---

## 10. Working agreement with the user (Sachin)

- Plain step-by-step instructions with exact clicks and copy-paste commands; the user is not an Unreal expert.
- Things only the user can do: Epic Launcher installs, account sign-ups/payments/ID checks, typing
  passwords, Play Console publishing clicks. Prepare everything else in advance.
- Ask before installing software or publishing anything; commit and push to the game's GitHub repo when asked.
- Never put secrets (keystore passwords, API keys) in git.
- Commit messages end with the Co-Authored-By line the session specifies.

---

## Starter prompt for the next game (paste into a new chat)

> Read `C:\SACHIN\Hollowlight\PLAYBOOK.md` and `C:\SACHIN\Hollowlight\CLAUDE.md` first. Build a new game the
> same way: Unreal Engine 5.8 C++, Android + iOS + Windows, new folder `C:\SACHIN\<GameName>` with its own git
> repo, app id `com.brainrotinteractive.<gamename>`, publisher Brainrot Interactive Studios. Reuse the
> architecture, tools (copy `Tools/` and adapt), configs and every fix in the playbook. Here is the game: …
