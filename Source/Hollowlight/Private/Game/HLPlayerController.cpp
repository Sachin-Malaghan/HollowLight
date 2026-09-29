// HOLLOWLIGHT: input, lifecycle, console commands and the capture script. (CLAUDE.md: Game flow / Input)
#include "Game/HLPlayerController.h"

#include "Audio/HLAudioSynth.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/HLSaveGame.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UI/HLUI.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogHollowlight, Log, All);

namespace
{
	AHLPlayerController* FindController(UWorld* World)
	{
		return World ? Cast<AHLPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdPlay(TEXT("hl.Play"), TEXT("hl.Play <level 1-10> [checkpoint 0-n]  start a level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World))
			{
				const int32 Level = Args.Num() > 0 ? FCString::Atoi(*Args[0]) - 1 : 0;
				const int32 Cp = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1;
				PC->Game.StartLevel(Level, Cp);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTitle(TEXT("hl.Title"), TEXT("Return to the title screen"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Game.StartAttract(0); PC->Game.GoTo(EHLScreen::Title); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdUnlock(TEXT("hl.UnlockAll"), TEXT("Unlock every level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Save->UnlockedLevels = PC->Game.NumLevels(); PC->Game.SaveProgress(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(TEXT("hl.ResetProgress"), TEXT("Forget all progress (keeps settings)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World))
			{
				PC->Save->UnlockedLevels = 1;
				PC->Save->LastLevel = 0;
				PC->Save->bFinishedGame = false;
				for (float& T : PC->Save->BestTimes) { T = 0; }
				for (int32& D : PC->Save->BestDeaths) { D = 0; }
				PC->Game.SaveProgress();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutopilot(TEXT("hl.Autopilot"), TEXT("hl.Autopilot 0|1  let the autopilot play the current level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Game.bAutopilotInPlay = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0; PC->Game.Pilot.Reset(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHideUI(TEXT("hl.HideUI"), TEXT("hl.HideUI 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Game.bHideUI = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0; }
		}));

	struct FCaptureShot
	{
		int32 Level;        // -1 = no level change
		int32 Checkpoint;
		EHLScreen Screen;
		bool bHideUI;
		float Wait;
		const TCHAR* Name;  // nullptr = no screenshot for this step
	};

	// Level shots start at a checkpoint and let the autopilot run into something worth seeing.
	const FCaptureShot CaptureScript[] = {
		{ 0, -1, EHLScreen::Title, false, 7.0f, TEXT("01_title") },
		{ -1, 0, EHLScreen::LevelSelect, false, 1.2f, TEXT("02_levels") },
		{ 0, 1, EHLScreen::Playing, true, 4.2f, TEXT("10_level01_traps") },
		{ 0, 1, EHLScreen::Playing, true, 9.2f, TEXT("10_level01_crate") },
		{ 0, 4, EHLScreen::Playing, true, 2.4f, TEXT("10_level01_log") },
		{ 1, 0, EHLScreen::Playing, true, 3.4f, TEXT("11_level02") },
		{ 2, 1, EHLScreen::Playing, true, 2.6f, TEXT("12_level03") },
		{ 3, 2, EHLScreen::Playing, true, 2.8f, TEXT("13_level04") },
		{ 4, 0, EHLScreen::Playing, true, 3.6f, TEXT("14_level05") },
		{ 5, 0, EHLScreen::Playing, true, 3.2f, TEXT("15_level06") },
		{ 6, 0, EHLScreen::Playing, true, 4.4f, TEXT("16_level07") },
		{ 7, 1, EHLScreen::Playing, true, 2.5f, TEXT("17_level08") },
		{ 8, 3, EHLScreen::Playing, true, 3.0f, TEXT("18_level09") },
		{ 9, 6, EHLScreen::Playing, true, 3.5f, TEXT("19_level10") },
		{ 2, -1, EHLScreen::Playing, false, 1.6f, TEXT("20_hud_card") },
		{ 0, 2, EHLScreen::Playing, false, 1.5f, nullptr },
		{ -1, 0, EHLScreen::Paused, false, 0.6f, TEXT("21_paused") },
		{ 0, 6, EHLScreen::Playing, false, 9.0f, TEXT("22_complete") },
		{ 9, 7, EHLScreen::Playing, false, 13.0f, TEXT("23_ending") },
		{ -1, 0, EHLScreen::Settings, false, 0.8f, TEXT("24_settings") },
	};
}

AHLPlayerController::AHLPlayerController()
{
	Audio = CreateDefaultSubobject<UHLAudioSynth>(TEXT("Synth"));
	Audio->SetupAttachment(RootComponent);
	bShowMouseCursor = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AHLPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) { return; }

	ActivateTouchInterface(nullptr);   // remove any virtual joystick another path may have added

	Save = Cast<UHLSaveGame>(UGameplayStatics::LoadGameFromSlot(UHLSaveGame::SlotName, 0));
	if (!Save) { Save = Cast<UHLSaveGame>(UGameplayStatics::CreateSaveGameObject(UHLSaveGame::StaticClass())); }

	if (Audio) { Audio->Start(); }
	Game.Init(Save, Audio);

	// Nothing in the 3D world is drawn; the HUD paints everything.
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->bDisableWorldRendering = true; }

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &AHLPlayerController::HandleBackground);
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &AHLPlayerController::HandleBackground);

	const TCHAR* Cmd = FCommandLine::Get();
	bForceTouch = FParse::Param(Cmd, TEXT("HLForceTouch"));
	int32 StartLevelArg = 0;
	if (FParse::Value(Cmd, TEXT("HLLevel="), StartLevelArg)) { Game.StartLevel(StartLevelArg - 1); }
	if (FParse::Param(Cmd, TEXT("HLCapture")))
	{
		bCapture = true;
		Game.bNoSave = true;
		Game.bAutopilotInPlay = true;
		FParse::Value(Cmd, TEXT("HLCaptureTag="), CaptureTag);
		Save->UnlockedLevels = Game.NumLevels();
		for (int32 I = 0; I < Save->BestTimes.Num(); ++I) { Save->BestTimes[I] = I < 6 ? 20.f + I * 2.3f : 0.f; }
	}
}

void AHLPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	if (Save) { Game.SaveProgress(); }
	Super::EndPlay(Reason);
}

void AHLPlayerController::HandleBackground()
{
	Game.OnAppBackground();
}

bool AHLPlayerController::IsTouchDevice() const
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return false;
#endif
}

bool AHLPlayerController::ShouldShowTouch() const
{
	if (bForceTouch) { return true; }
	if (!Save) { return IsTouchDevice(); }
	switch (Save->TouchMode)
	{
	case EHLTouchMode::On: return true;
	case EHLTouchMode::Off: return false;
	default: return IsTouchDevice() || bTouchSeen;
	}
}

void AHLPlayerController::UpdateSafeArea(double W, double H)
{
	SafeAreaTimer -= 1.0;
	if (SafeAreaTimer > 0) { return; }
	SafeAreaTimer = 60;
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const double Min = H * 0.02;
	SafeArea = FVector4(FMath::Max((double)Metrics.TitleSafePaddingSize.X, Min), FMath::Max((double)Metrics.TitleSafePaddingSize.Y, Min),
	                    FMath::Max((double)Metrics.TitleSafePaddingSize.Z, Min), FMath::Max((double)Metrics.TitleSafePaddingSize.W, Min));
}

void AHLPlayerController::GatherInput(FHLControls& Controls, FHLMenuInput& Menu)
{
	auto Down = [&](const FKey& K) { return IsInputKeyDown(K); };
	auto Pressed = [&](const FKey& K) { return WasInputKeyJustPressed(K); };

	// Keyboard + gamepad
	const float StickX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float StickY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const bool bLeft = Down(EKeys::Left) || Down(EKeys::A) || Down(EKeys::Gamepad_DPad_Left) || StickX < -0.35f;
	const bool bRight = Down(EKeys::Right) || Down(EKeys::D) || Down(EKeys::Gamepad_DPad_Right) || StickX > 0.35f;
	const bool bJump = Down(EKeys::SpaceBar) || Down(EKeys::W) || Down(EKeys::Up) || Down(EKeys::Gamepad_FaceButton_Bottom);

	Menu.Up = Pressed(EKeys::Up) || Pressed(EKeys::W) || Pressed(EKeys::Gamepad_DPad_Up) || (StickY > 0.5f && StickPrevY <= 0.5f);
	Menu.Down = Pressed(EKeys::Down) || Pressed(EKeys::S) || Pressed(EKeys::Gamepad_DPad_Down) || (StickY < -0.5f && StickPrevY >= -0.5f);
	Menu.Left = Pressed(EKeys::Left) || Pressed(EKeys::A) || Pressed(EKeys::Gamepad_DPad_Left) || (StickX < -0.5f && StickPrevX >= -0.5f);
	Menu.Right = Pressed(EKeys::Right) || Pressed(EKeys::D) || Pressed(EKeys::Gamepad_DPad_Right) || (StickX > 0.5f && StickPrevX <= 0.5f);
	Menu.Confirm = Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar) || Pressed(EKeys::Gamepad_FaceButton_Bottom);
	Menu.Back = Pressed(EKeys::Escape) || Pressed(EKeys::BackSpace) || Pressed(EKeys::Gamepad_FaceButton_Right) || Pressed(EKeys::Android_Back);
	Menu.Pause = Pressed(EKeys::P) || Pressed(EKeys::Gamepad_Special_Right);
	StickPrevX = StickX;
	StickPrevY = StickY;

	const bool bAnyKey = bLeft || bRight || bJump || Menu.Up || Menu.Down || Menu.Confirm || Menu.Back;
	if (bAnyKey && !IsTouchDevice()) { bTouchSeen = false; }   // Auto mode hides the pads again on keyboard use

	// Mouse (desktop only; on phones the "mouse" just mirrors the first finger)
	if (!IsTouchDevice())
	{
		double MX = 0, MY = 0;
		if (GetMousePosition(MX, MY))
		{
			const FVector2D M(MX, MY);
			Menu.bPointerValid = true;
			Menu.Pointer = M;
			Menu.bPointerMoved = LastMouse.X >= 0 && FVector2D::DistSquared(M, LastMouse) > 1.0;
			LastMouse = M;
			if (WasInputKeyJustReleased(EKeys::LeftMouseButton)) { Menu.bClick = true; }
		}
	}

	// Touch
	int32 VX = 0, VY = 0;
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport())
	{
		FVector2D Size;
		VC->GetViewportSize(Size);
		VX = (int32)Size.X;
		VY = (int32)Size.Y;
	}
	const FHLTouchLayout Layout = FHLTouchLayout::Compute(VX, VY, SafeArea);
	const bool bControls = Game.IsGameplay() && ShouldShowTouch();
	bTouchLeft = bTouchRight = bTouchJump = false;
	bMenuTouchDown = false;
	for (int32 I = 0; I < MaxTouches; ++I)
	{
		double X = 0, Y = 0;
		bool bPressed = false;
		GetInputTouchState((ETouchIndex::Type)(ETouchIndex::Touch1 + I), X, Y, bPressed);
		const FVector2D P(X, Y);
		if (bPressed && !TouchDown[I])
		{
			bTouchSeen = true;
			TouchStart[I] = P;
			TouchIsControl[I] = bControls && (Layout.HitLeft(P) || Layout.HitRight(P) || Layout.HitJump(P));
			TouchStartedOnJump[I] = bControls && Layout.HitJump(P);
		}
		if (bPressed)
		{
			TouchLast[I] = P;
			if (TouchIsControl[I] && bControls)
			{
				if (TouchStartedOnJump[I]) { bTouchJump = true; }
				else if (Layout.HitLeft(P)) { bTouchLeft = true; }
				else if (Layout.HitRight(P)) { bTouchRight = true; }
			}
			else if (!TouchIsControl[I])
			{
				bMenuTouchDown = true;
				MenuTouch = P;
				Menu.bPointerValid = true;
				Menu.Pointer = P;
				Menu.bPointerMoved = true;
			}
		}
		else if (TouchDown[I])
		{
			// A tap (not a control press) clicks whatever is under it.
			if (!TouchIsControl[I] && FVector2D::Distance(TouchStart[I], TouchLast[I]) < VY * 0.06)
			{
				Menu.bPointerValid = true;
				Menu.Pointer = TouchLast[I];
				Menu.bClick = true;
			}
			TouchIsControl[I] = false;
			TouchStartedOnJump[I] = false;
		}
		TouchDown[I] = bPressed;
	}

	Controls.Dir = ((bRight || bTouchRight) ? 1 : 0) - ((bLeft || bTouchLeft) ? 1 : 0);
	Controls.Jump = bJump || bTouchJump;
}

void AHLPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!Save) { return; }

	FVector2D Size(1280, 720);
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->GetViewportSize(Size); }
	UpdateSafeArea(Size.X, Size.Y);

	FHLControls Controls;
	FHLMenuInput Menu;
	GatherInput(Controls, Menu);
	if (bCapture)
	{
		Menu = FHLMenuInput();
		TickCapture(DeltaTime);
	}
	Game.Tick(DeltaTime, Controls, Menu, Size.X, Size.Y);

	if (Game.bQuitRequested)
	{
		Game.bQuitRequested = false;
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void AHLPlayerController::TickCapture(float DeltaTime)
{
	constexpr int32 NumSteps = UE_ARRAY_COUNT(CaptureScript);
	CaptureClock += DeltaTime;
	if (CaptureStep >= NumSteps)
	{
		// Give the last screenshot a moment to be written, then quit.
		if (CaptureClock > 1.0 && CaptureStep == NumSteps)
		{
			++CaptureStep;
			ConsoleCommand(TEXT("quit"));
		}
		return;
	}
	if (CaptureStep >= 0 && CaptureClock < CaptureScript[CaptureStep].Wait) { return; }

	if (CaptureStep >= 0 && CaptureScript[CaptureStep].Name && !bCaptureShotTaken)
	{
		// Request the shot, then let this frame render before the next step changes anything.
		const FString Name = CaptureTag.IsEmpty() ? FString(CaptureScript[CaptureStep].Name) : CaptureTag + TEXT("_") + CaptureScript[CaptureStep].Name;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("Hollowlight_") + Name + TEXT(".png")));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		UE_LOG(LogHollowlight, Display, TEXT("HLCapture: %s (screen %d, level %d, x=%.0f)"), *Path, (int32)Game.Screen, Game.LevelIndex + 1, Game.Sim.P.X);
		bCaptureShotTaken = true;
		return;
	}
	if (bCaptureShotTaken && CaptureClock < CaptureScript[CaptureStep].Wait + 0.3) { return; }
	bCaptureShotTaken = false;

	++CaptureStep;
	CaptureClock = 0;
	if (CaptureStep >= NumSteps) { return; }

	const FCaptureShot& S = CaptureScript[CaptureStep];
	Game.bHideUI = S.bHideUI;
	if (S.Level >= 0)
	{
		if (S.Screen == EHLScreen::Title) { Game.StartAttract(S.Level); Game.GoTo(EHLScreen::Title); }
		else { Game.StartLevel(S.Level, S.Checkpoint); }
	}
	else
	{
		Game.GoTo(S.Screen);
		if (S.Screen == EHLScreen::LevelSelect) { Game.ReturnScreen = EHLScreen::Title; }
	}
}
