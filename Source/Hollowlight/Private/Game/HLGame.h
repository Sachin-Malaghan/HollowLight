// EMBERHOME: game flow - screens, the fixed-step loop, camera, weather, progress. (CLAUDE.md: Game flow)
// Plain C++ owned by AHLPlayerController; AHLHUD draws it.
#pragma once

#include "CoreMinimal.h"
#include "Core/HLAutopilot.h"
#include "Core/HLSim.h"

class UHLSaveGame;

enum class EHLScreen : uint8
{
	Title, LevelSelect, Settings, Credits, Playing, Paused, LevelComplete, Ending, Calibrate
};

enum class EHLAction : uint8
{
	None, Play, Levels, Settings, Credits, Back, SelectLevel, Resume, RestartCheckpoint, RestartLevel,
	QuitToTitle, NextLevel, Replay, ToggleMusic, ToggleSound, CycleTouch, ToggleGrain, ToggleFlashing,
	Pause, PlayAgain, Quit, PrivacyPolicy, Calibrate, SkipCalibrate, ToggleTimer, PageLevels
};

enum class EHLUiSound : uint8 { Move, Select, Back };

// Sounds of things that are simply going on near the child (a blade turning, a fire burning, a wolf's
// feet), as opposed to things that happen once (those are sim events). FHLGame decides when; the synth how.
enum class EHLCue : uint8
{
	SawWhirr, FireCrackle, FireRoar, PlankStep, BridgeCreak, WolfPaws, WolfGrowl, DogPaws, DogPant, GhostMoan,
	FloodRush, Ratchet, Scree, Count
};

struct FHLControls
{
	int Dir = 0;
	bool Jump = false;
	bool Down = false;   // slide / crouch
	bool Act = false;    // use / pick up / whistle for the dog
};

// A speck of dust, grit or ash. Purely for show: lives in the game, not the simulation.
struct FHLParticle
{
	double X = 0, Y = 0, VX = 0, VY = 0;
	double Life = 0, MaxLife = 1;
	double Size = 1;
	double Gravity = 0;
	float Grey = 0.4f, Alpha = 0.3f;
	bool bGrow = true;    // dust swells as it thins; debris shrinks
	bool bWarm = false;   // ember sparks: drawn in the lantern's colour, additively
};

struct FHLMenuInput
{
	bool Up = false, Down = false, Left = false, Right = false;
	bool Confirm = false, Back = false, Pause = false;
	bool bPointerValid = false;
	bool bPointerMoved = false;
	bool bClick = false;           // mouse click or tap released this frame
	FVector2D Pointer = FVector2D::ZeroVector;
	FVector2D RawPointer = FVector2D::ZeroVector;   // before touch calibration is applied
};

struct FHLButton
{
	FBox2D Box;
	EHLAction Action = EHLAction::None;
	int32 Param = 0;
	bool bEnabled = true;
};

class IHLAudioSink
{
public:
	virtual ~IHLAudioSink() = default;
	virtual void OnSimEvent(const HL::FEvent& Event, float Pan, float Distance) = 0;
	virtual void OnUiSound(EHLUiSound Sound) = 0;
	virtual void OnCue(EHLCue Cue, float Near, float Pan) = 0;   // Near: 1 = right here, 0 = out of earshot
	virtual void OnThunder(float Strength, float Delay) = 0;
	virtual void SetMix(float Rain, float Wind, bool bMusic, bool bSound, bool bPausedOrMenu, float Warmth) = 0;
};

class FHLGame
{
public:
	void Init(UHLSaveGame* InSave, IHLAudioSink* InAudio, bool bTouchDevice);
	void Tick(float DeltaSeconds, const FHLControls& Controls, const FHLMenuInput& Menu, double ScreenW, double ScreenH);

	// Flow
	void StartLevel(int32 Index, int32 Checkpoint = -1);
	void StartAttract(int32 Index);
	void GoTo(EHLScreen NewScreen);
	void Activate(const FHLButton& Button);
	void OnAppBackground();
	void SaveProgress();

	bool IsGameplay() const { return Screen == EHLScreen::Playing; }
	bool IsWorldPaused() const { return Screen == EHLScreen::Paused; }
	bool ShowsWorld() const { return true; }
	int32 NumLevels() const;
	int32 NumCleared() const;                       // levels finished at least once
	int32 ReachedCheckpoints(int32 Level) const;    // checkpoints of that level reached so far
	int32 EmbersFound(int32 Level = -1) const;      // embers found in that level (-1 = in all of them)
	int32 EmbersTotal() const;
	double TotalBestTime() const;                   // sum of the best times, 0 until every level is cleared
	double BodyStretch() const;                     // squash (< 0) and stretch (> 0) of the child, for the renderer
	TArray<FHLParticle> Particles;
	const HL::FLevelDef& CurrentLevel() const { return *Sim.Level; }

	// State (read by the HUD)
	EHLScreen Screen = EHLScreen::Title;
	EHLScreen ReturnScreen = EHLScreen::Title;   // where Back from Settings goes
	int32 LevelIndex = 0;
	int32 LevelPage = 0;         // which chapter the level select is showing
	static constexpr int32 kChapterSize = 11;
	int32 NumChapters() const { return (NumLevels() + kChapterSize - 1) / kChapterSize; }
	bool bAttract = true;
	HL::FSim Sim;
	HL::FAutopilot Pilot;
	double Accumulator = 0;
	double PrevX = 0, PrevY = 0;
	double RenderX = 0, RenderY = 0;
	double CamX = 0, CamY = 0;
	double Scale = 1, ViewW = 711;
	double RealTime = 0;
	double ScreenTime = 0;
	double LevelTime = 0;       // real time since the level (or attract run) started
	double Lightning = 0;
	double Shake = 0;
	double MenuFade = 1;         // 1 = menu fully shown
	int32 CalibrateStep = 0;                        // which target is showing
	FVector2D CalibrateRaw[2];                      // where the taps were reported (screen fractions)
	static FVector2D CalibrateTarget(int32 Step) { return Step == 0 ? FVector2D(0.25, 0.35) : FVector2D(0.75, 0.65); }
	bool bNewBest = false;
	FString NoteText;            // the script line shown at the top right (FLevelDef::Notes, or the ghost warning)
	double NoteAlpha = 0;
	float ResultTime = 0;
	int32 ResultDeaths = 0;

	TArray<FHLButton> Buttons;   // rebuilt every frame by the UI
	int32 Focus = 0;
	bool bPointerActive = false;  // last menu input came from mouse/touch (hide keyboard focus ring)

	UHLSaveGame* Save = nullptr;

	// Validation / capture (-HLCapture): the autopilot plays real levels, nothing is saved.
	bool bAutopilotInPlay = false;
	bool bNoSave = false;
	bool bHideUI = false;
	// Trailer (-HLTrailer): a caption under each clip, then the closing card.
	bool bTrailer = false;
	FString TrailerCaption;
	double TrailerClipTime = 0, TrailerClipLen = 1;
	double TrailerCard = 0;      // 0..1 as the closing card comes up
	int32 DebugPose = -1;      // >= 0: draw the child in this EPose (hl.Debug.Pose, capture script)

	// Set by the QUIT button (desktop) or Back on the title screen (Android); the controller closes the app.
	bool bQuitRequested = false;
	static bool PlatformHasQuitButton() { return !(PLATFORM_ANDROID || PLATFORM_IOS); }

private:
	void StepWorld(double Dt, const FHLControls& Controls);
	void UpdateCamera(double Dt, bool bSnap);
	void HandleMenu(const FHLMenuInput& Menu);
	void HandleCalibration(const FHLMenuInput& Menu);
	double PixelW = 1, PixelH = 1;   // viewport size in pixels
	void Back();
	void CompleteLevel();
	FString WantedNote() const;
	void Burst(double X, double Y, int32 Count, double Speed, double Up, double Size, double Life, float Grey, float Alpha, double Gravity, bool bGrow, bool bWarm = false);
	void OnEventJuice(const HL::FEvent& E);
	void TickAmbient(double Dt);
	void Cue(EHLCue C, double X, double Range, double Every);
	double CueTimer[(int32)EHLCue::Count] = {};
	double LandStamp = -10, LandStrength = 0, JumpStamp = -10, PushDust = 0;

	IHLAudioSink* Audio = nullptr;
	double NextLightningCheck = 0;
	int32 LastButtonCount = 0;
};
