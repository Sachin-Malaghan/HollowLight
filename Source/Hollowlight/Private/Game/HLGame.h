// EMBERHOME: game flow - screens, the fixed-step loop, camera, weather, progress. (CLAUDE.md: Game flow)
// Plain C++ owned by AHLPlayerController; AHLHUD draws it.
#pragma once

#include "CoreMinimal.h"
#include "Core/HLAutopilot.h"
#include "Core/HLSim.h"

class UHLSaveGame;

enum class EHLScreen : uint8
{
	Title, LevelSelect, Settings, Credits, Playing, Paused, LevelComplete, Ending
};

enum class EHLAction : uint8
{
	None, Play, Levels, Settings, Credits, Back, SelectLevel, Resume, RestartCheckpoint, RestartLevel,
	QuitToTitle, NextLevel, Replay, ToggleMusic, ToggleSound, CycleTouch, ToggleGrain, ToggleFlashing,
	Pause, PlayAgain, Quit, PrivacyPolicy
};

enum class EHLUiSound : uint8 { Move, Select, Back };

struct FHLControls
{
	int Dir = 0;
	bool Jump = false;
};

struct FHLMenuInput
{
	bool Up = false, Down = false, Left = false, Right = false;
	bool Confirm = false, Back = false, Pause = false;
	bool bPointerValid = false;
	bool bPointerMoved = false;
	bool bClick = false;           // mouse click or tap released this frame
	FVector2D Pointer = FVector2D::ZeroVector;
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
	virtual void OnThunder(float Strength, float Delay) = 0;
	virtual void SetMix(float Rain, float Wind, bool bMusic, bool bSound, bool bPausedOrMenu, float Warmth) = 0;
};

class FHLGame
{
public:
	void Init(UHLSaveGame* InSave, IHLAudioSink* InAudio);
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
	const HL::FLevelDef& CurrentLevel() const { return *Sim.Level; }

	// State (read by the HUD)
	EHLScreen Screen = EHLScreen::Title;
	EHLScreen ReturnScreen = EHLScreen::Title;   // where Back from Settings goes
	int32 LevelIndex = 0;
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
	bool bNewBest = false;
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

	// Set by the QUIT button (desktop) or Back on the title screen (Android); the controller closes the app.
	bool bQuitRequested = false;
	static bool PlatformHasQuitButton() { return !(PLATFORM_ANDROID || PLATFORM_IOS); }

private:
	void StepWorld(double Dt, const FHLControls& Controls);
	void UpdateCamera(double Dt, bool bSnap);
	void HandleMenu(const FHLMenuInput& Menu);
	void Back();
	void CompleteLevel();

	IHLAudioSink* Audio = nullptr;
	double NextLightningCheck = 0;
	int32 LastButtonCount = 0;
};
