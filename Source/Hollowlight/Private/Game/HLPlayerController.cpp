// EMBERHOME: input, lifecycle, console commands and the capture script. (CLAUDE.md: Game flow / Input)
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

	FAutoConsoleCommandWithWorldAndArgs CmdPlay(TEXT("hl.Play"), TEXT("hl.Play <level 1-21> [checkpoint 0-n]  start a level"),
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
				for (int32& C : PC->Save->ReachedCheckpoints) { C = 0; }
				for (int32& M : PC->Save->EmberMask) { M = 0; }
				PC->Save->LastCheckpoint = -1;
				PC->Game.SaveProgress();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutopilot(TEXT("hl.Autopilot"), TEXT("hl.Autopilot 0|1  let the autopilot play the current level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Game.bAutopilotInPlay = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0; PC->Game.Pilot.Reset(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPose(TEXT("hl.Debug.Pose"), TEXT("hl.Debug.Pose <-1..8>  draw the child in a fixed pose (-1 = off)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AHLPlayerController* PC = FindController(World)) { PC->Game.DebugPose = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1; }
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
		int32 Pose = -1;    // force a pose for this shot (visual check of poses the autopilot never strikes)
		bool bLinger = false;   // stand still until the ghost comes
	};

	// Level shots start at a checkpoint and let the autopilot run into something worth seeing.
	struct FTrailerClip
	{
		int32 Level;        // -2 = the closing card
		int32 Checkpoint;
		float Seconds;
		const TCHAR* Caption;
		bool bLinger;       // stand still so the ghost comes
	};

	// The best of both chapters in thirty seconds. Each clip starts at a cairn just before something happens.
	const FTrailerClip TrailerScript[] = {
		{ 0, 4, 2.6f, TEXT("A child. A lantern."), false },
		{ 1, 0, 2.6f, TEXT("A stray who stays."), false },
		{ 8, 5, 3.2f, TEXT("Mountains to climb, and to fall from."), false },
		{ 12, 2, 3.4f, TEXT("Bridges that will not hold."), false },
		{ 13, 2, 2.8f, TEXT("Machines that never stopped."), false },
		{ 15, 2, 3.6f, TEXT("Fire."), false },
		{ 5, 0, 2.6f, TEXT("Things that come for the light."), true },
		{ 16, 1, 4.2f, TEXT("And things that hunt."), false },
		{ -2, 0, 5.0f, TEXT(""), false },
	};

	const FCaptureShot CaptureScript[] = {
		{ 0, -1, EHLScreen::Title, false, 7.0f, TEXT("01_title") },
		{ -1, 0, EHLScreen::LevelSelect, false, 1.2f, TEXT("02_levels") },
		{ -1, 1, EHLScreen::LevelSelect, false, 1.2f, TEXT("03_levels_chapter2") },
		{ 0, 1, EHLScreen::Playing, true, 4.2f, TEXT("10_level01_traps") },
		{ 0, 1, EHLScreen::Playing, true, 9.2f, TEXT("10_level01_crate") },
		{ 0, 4, EHLScreen::Playing, true, 2.4f, TEXT("10_level01_log") },
		{ 1, 0, EHLScreen::Playing, true, 3.4f, TEXT("11_level02_slope") },
		{ 1, 2, EHLScreen::Playing, true, 4.6f, TEXT("11_level02_lever") },
		{ 1, 3, EHLScreen::Playing, true, 4.0f, TEXT("11_level02_dogdoor") },
		{ 2, 1, EHLScreen::Playing, true, 2.6f, TEXT("12_level03") },
		{ 3, 2, EHLScreen::Playing, true, 4.2f, TEXT("13_level04_ladder") },
		{ 3, 3, EHLScreen::Playing, true, 3.2f, TEXT("13_level04_handle") },
		{ 3, 3, EHLScreen::Playing, true, 9.5f, TEXT("13_level04_bridge") },
		{ 4, 0, EHLScreen::Playing, true, 3.6f, TEXT("14_level05") },
		{ 5, 0, EHLScreen::Playing, true, 4.0f, TEXT("15_level06_plate") },
		{ 5, 2, EHLScreen::Playing, true, 4.0f, TEXT("15_level06_racking") },
		{ 5, 3, EHLScreen::Playing, true, 3.0f, TEXT("15_level06_crate") },
		{ 6, 0, EHLScreen::Playing, true, 3.0f, TEXT("16_level07_wagon") },
		{ 6, 2, EHLScreen::Playing, true, 3.0f, TEXT("16_level07_hut") },
		{ 6, 3, EHLScreen::Playing, true, 6.0f, TEXT("16_level07_bridge") },
		{ 7, 1, EHLScreen::Playing, true, 3.0f, TEXT("17_level08_barriers") },
		{ 7, 2, EHLScreen::Playing, true, 3.5f, TEXT("17_level08_footbridge") },
		{ 7, 3, EHLScreen::Playing, true, 3.0f, TEXT("17_level08_gate") },
		{ 8, 0, EHLScreen::Playing, true, 3.4f, TEXT("18_level09_terraces") },
		{ 8, 1, EHLScreen::Playing, true, 4.4f, TEXT("18_level09_ladder") },
		{ 8, 2, EHLScreen::Playing, true, 3.6f, TEXT("18_level09_scree") },
		{ 8, 2, EHLScreen::Playing, true, 4.5f, TEXT("18_level09_trunk") },
		{ 8, 5, EHLScreen::Playing, true, 2.9f, TEXT("18_level09_chasm") },
		{ 9, 3, EHLScreen::Playing, true, 3.0f, TEXT("19_level10") },
		{ 10, 6, EHLScreen::Playing, true, 3.5f, TEXT("19_level11") },
		{ 11, 0, EHLScreen::Playing, true, 3.0f, TEXT("40_level12_rope") },
		{ 12, 0, EHLScreen::Playing, true, 4.2f, TEXT("41_level13_bridge") },
		{ 13, 0, EHLScreen::Playing, true, 2.4f, TEXT("42_level14_saw") },
		{ 13, 2, EHLScreen::Playing, true, 2.2f, TEXT("42_level14_hammers") },
		{ 14, 1, EHLScreen::Playing, true, 7.5f, TEXT("43_level15_flood") },
		{ 15, 0, EHLScreen::Playing, true, 2.6f, TEXT("44_level16_vents") },
		{ 15, 2, EHLScreen::Playing, true, 3.4f, TEXT("44_level16_catwalk") },
		{ 16, 1, EHLScreen::Playing, false, 4.5f, TEXT("45_level17_wolf") },
		{ 17, 1, EHLScreen::Playing, true, 6.5f, TEXT("46_level18_shaft") },
		{ 18, 0, EHLScreen::Playing, true, 4.6f, TEXT("47_level19_firebridge") },
		{ 19, 1, EHLScreen::Playing, true, 7.0f, TEXT("48_level20_pack") },
		{ 20, 2, EHLScreen::Playing, true, 3.6f, TEXT("49_level21") },
		{ 2, -1, EHLScreen::Playing, false, 1.6f, TEXT("20_hud_card") },
		{ 0, 2, EHLScreen::Playing, false, 1.5f, nullptr },
		{ -1, 0, EHLScreen::Paused, false, 0.6f, TEXT("21_paused") },
		{ 0, 6, EHLScreen::Playing, false, 9.0f, TEXT("22_complete") },
		{ 20, 5, EHLScreen::Playing, false, 9.5f, TEXT("23_ending") },
		{ -1, 0, EHLScreen::Settings, false, 0.8f, TEXT("24_settings") },
		{ -1, 0, EHLScreen::Calibrate, false, 0.8f, TEXT("25_calibrate") },
		{ 0, 0, EHLScreen::Playing, true, 1.2f, TEXT("30_pose_slide"), (int32)HL::EPose::Slide },
		{ 0, 0, EHLScreen::Playing, true, 1.2f, TEXT("31_pose_crouch"), (int32)HL::EPose::Crouch },
		{ 0, 0, EHLScreen::Playing, true, 1.2f, TEXT("32_pose_hang"), (int32)HL::EPose::Hang },
		{ 0, 0, EHLScreen::Playing, true, 1.2f, TEXT("33_pose_roll"), (int32)HL::EPose::Roll },
		{ 0, 0, EHLScreen::Playing, true, 1.2f, TEXT("34_pose_ladder"), (int32)HL::EPose::Ladder },
		{ 1, 0, EHLScreen::Playing, false, 6.0f, TEXT("35_hud_act_hint") },
		{ 5, 0, EHLScreen::Playing, false, 5.2f, TEXT("36_ghost"), -1, true },
		{ 3, 3, EHLScreen::Playing, false, 2.2f, TEXT("37_tool_note") },
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

	bForceTouch = FParse::Param(FCommandLine::Get(), TEXT("HLForceTouch"));
	bTrailer = FParse::Param(FCommandLine::Get(), TEXT("HLTrailer"));
	if (Audio)
	{
		if (bTrailer) { Audio->BeginOffline(); } else { Audio->Start(); }
	}
	// Capture runs are scripted: never interrupt them with the calibration screen.
	if (FParse::Param(FCommandLine::Get(), TEXT("HLCapture")) || bTrailer) { Save->bTouchCalibrated = true; }
	Game.Init(Save, Audio, IsTouchDevice() || bForceTouch);

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
	if (bTrailer)
	{
		Game.bTrailer = true;
		Game.bNoSave = true;
		Game.bHideUI = true;
		Save->bMusic = Save->bSound = true;
		Save->UnlockedLevels = Game.NumLevels();
	}
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

void AHLPlayerController::GatherInput(float DeltaTime, FHLControls& Controls, FHLMenuInput& Menu)
{
	auto Down = [&](const FKey& K) { return IsInputKeyDown(K); };
	auto Pressed = [&](const FKey& K) { return WasInputKeyJustPressed(K); };

	// Keyboard + gamepad
	const float StickX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float StickY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const bool bLeft = Down(EKeys::Left) || Down(EKeys::A) || Down(EKeys::Gamepad_DPad_Left) || StickX < -0.35f;
	const bool bRight = Down(EKeys::Right) || Down(EKeys::D) || Down(EKeys::Gamepad_DPad_Right) || StickX > 0.35f;
	const bool bJump = Down(EKeys::SpaceBar) || Down(EKeys::W) || Down(EKeys::Up) || Down(EKeys::Gamepad_FaceButton_Bottom);
	const bool bDownKey = Down(EKeys::Down) || Down(EKeys::S) || Down(EKeys::LeftControl) || Down(EKeys::Gamepad_DPad_Down) ||
		Down(EKeys::Gamepad_FaceButton_Left) || StickY < -0.5f;
	const bool bActKey = Down(EKeys::E) || Down(EKeys::F) || Down(EKeys::Gamepad_FaceButton_Top);

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
			Menu.RawPointer = M;
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
	const bool bSwipes = Game.IsGameplay();
	bTouchLeft = bTouchRight = bTouchJump = bTouchSlide = bTouchAct = false;
	bMenuTouchDown = false;
	SwipeJumpTimer -= DeltaTime;
	SwipeSlideTimer -= DeltaTime;

	// Touch calibration ("touch the light"): this screen reports  reported = Scale * true + Offset
	// (in screen fractions), so invert it. Identity until calibrated.
	auto Correct = [&](const FVector2D& Raw)
	{
		if (!Save || !Save->bTouchCalibrated || VX <= 0 || VY <= 0) { return Raw; }
		return FVector2D((Raw.X / VX - Save->TouchOffsetX) / Save->TouchScaleX * VX,
		                 (Raw.Y / VY - Save->TouchOffsetY) / Save->TouchScaleY * VY);
	};

	for (int32 I = 0; I < MaxTouches; ++I)
	{
		double X = 0, Y = 0;
		bool bPressed = false;
		GetInputTouchState((ETouchIndex::Type)(ETouchIndex::Touch1 + I), X, Y, bPressed);
		const FVector2D Raw(X, Y);
		const FVector2D P = Correct(Raw);
		if (bPressed && !TouchDown[I])
		{
			bTouchSeen = true;
			TouchStart[I] = P;
			TouchSwiped[I] = false;
			TouchRole[I] = ETouchRole::None;
			if (bControls)
			{
				if (Layout.HitJump(P)) { TouchRole[I] = ETouchRole::Jump; }
				else if (Layout.HitSlide(P)) { TouchRole[I] = ETouchRole::Slide; }
				else if (Layout.HitAct(P)) { TouchRole[I] = ETouchRole::Act; }
				else if (Layout.HitLeft(P) || Layout.HitRight(P)) { TouchRole[I] = ETouchRole::Move; }
			}
		}
		if (bPressed)
		{
			TouchLast[I] = P;
			TouchLastRaw[I] = Raw;
			if (TouchRole[I] != ETouchRole::None && bControls)
			{
				if (TouchRole[I] == ETouchRole::Jump) { bTouchJump = true; }
				else if (TouchRole[I] == ETouchRole::Slide) { bTouchSlide = true; }
				else if (TouchRole[I] == ETouchRole::Act) { bTouchAct = true; }
				else if (Layout.HitLeft(P)) { bTouchLeft = true; }
				else if (Layout.HitRight(P)) { bTouchRight = true; }
			}
			else if (TouchRole[I] == ETouchRole::None)
			{
				// Swipes anywhere else on the screen: up = jump, down = slide.
				if (bSwipes && !TouchSwiped[I])
				{
					const FVector2D D = P - TouchStart[I];
					if (FMath::Abs(D.Y) > VY * 0.07 && FMath::Abs(D.Y) > FMath::Abs(D.X) * 1.2)
					{
						TouchSwiped[I] = true;
						if (D.Y < 0) { SwipeJumpTimer = 0.2f; } else { SwipeSlideTimer = 0.55f; }
					}
				}
				bMenuTouchDown = true;
				MenuTouch = P;
				Menu.bPointerValid = true;
				Menu.Pointer = P;
				Menu.RawPointer = Raw;
				Menu.bPointerMoved = true;
			}
		}
		else if (TouchDown[I])
		{
			// A tap (not a control press or a swipe) clicks whatever is under it.
			if (TouchRole[I] == ETouchRole::None && !TouchSwiped[I] && FVector2D::Distance(TouchStart[I], TouchLast[I]) < VY * 0.06)
			{
				Menu.bPointerValid = true;
				Menu.Pointer = TouchLast[I];
				Menu.RawPointer = TouchLastRaw[I];
				Menu.bClick = true;
			}
			TouchRole[I] = ETouchRole::None;
		}
		TouchDown[I] = bPressed;
	}

	Controls.Dir = ((bRight || bTouchRight) ? 1 : 0) - ((bLeft || bTouchLeft) ? 1 : 0);
	Controls.Jump = bJump || bTouchJump || SwipeJumpTimer > 0;
	Controls.Down = bDownKey || bTouchSlide || SwipeSlideTimer > 0;
	Controls.Act = bActKey || bTouchAct;
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
	GatherInput(DeltaTime, Controls, Menu);
	if (bCapture)
	{
		Menu = FHLMenuInput();
		TickCapture(DeltaTime);
	}
	if (bTrailer)
	{
		Menu = FHLMenuInput();
		Controls = FHLControls();
		TickTrailer(DeltaTime);
	}
	Game.Tick(DeltaTime, Controls, Menu, Size.X, Size.Y);
	if (bTrailer) { EndTrailerFrame(DeltaTime); }

	if (Game.DebugPose >= 0)
	{
		HL::FPlayer& P = Game.Sim.P;
		P.Pose = (HL::EPose)Game.DebugPose;
		P.HangLedgeY = P.Y - HL::kPlayerH - 2.0;
		P.HangTime = 0;
		P.RollTime = 0.4 - FMath::Fmod(Game.RealTime, 0.4);
	}

	if (Game.bQuitRequested)
	{
		Game.bQuitRequested = false;
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void AHLPlayerController::TickTrailer(float DeltaTime)
{
	if (bTrailerDone) { return; }
	constexpr int32 NumClips = UE_ARRAY_COUNT(TrailerScript);
	if (TrailerClip >= 0) { TrailerClock += DeltaTime; }
	Game.TrailerClipTime += DeltaTime;
	if (TrailerClip >= 0 && TrailerClock < TrailerClipEnd) { return; }

	++TrailerClip;
	if (TrailerClip >= NumClips)
	{
		bTrailerDone = true;
		const FString Wav = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Trailer/sound.wav"));
		if (Audio) { Audio->SaveWav(Wav); }
		UE_LOG(LogHollowlight, Display, TEXT("HLTrailer: %d frames, %.2f s, sound %s"), TrailerFrame, TrailerClock, *Wav);
		ConsoleCommand(TEXT("quit"));
		return;
	}
	const FTrailerClip& C = TrailerScript[TrailerClip];
	TrailerClipEnd = TrailerClock + C.Seconds;
	Game.TrailerCaption = C.Caption;
	Game.TrailerClipTime = 0;
	Game.TrailerClipLen = C.Seconds;
	if (C.Level == -2)
	{
		Game.StartAttract(0);
		Game.GoTo(EHLScreen::Title);
	}
	else
	{
		Game.bAutopilotInPlay = !C.bLinger;
		Game.StartLevel(C.Level, C.Checkpoint);
		if (C.bLinger) { Game.Sim.Linger = HL::kGhostWait - 0.2; }
	}
}

void AHLPlayerController::EndTrailerFrame(float DeltaTime)
{
	if (bTrailerDone) { return; }
	constexpr int32 NumClips = UE_ARRAY_COUNT(TrailerScript);
	if (TrailerClip >= 0 && TrailerClip < NumClips && TrailerScript[TrailerClip].Level == -2)
	{
		Game.TrailerCard = FMath::Min(1.0, Game.TrailerCard + DeltaTime / 0.7);
	}
	if (Audio) { Audio->RenderOffline(DeltaTime); }
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Trailer/f_%05d.png"), TrailerFrame++));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
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
	Game.DebugPose = S.Pose;
	Game.bAutopilotInPlay = !S.bLinger;
	if (S.Level >= 0)
	{
		if (S.Screen == EHLScreen::Title) { Game.StartAttract(S.Level); Game.GoTo(EHLScreen::Title); }
		else
		{
			Game.StartLevel(S.Level, S.Checkpoint);
			if (S.bLinger) { Game.Sim.Linger = HL::kGhostWait - 1.0; }
		}
	}
	else
	{
		Game.GoTo(S.Screen);
		if (S.Screen == EHLScreen::LevelSelect) { Game.ReturnScreen = EHLScreen::Title; Game.LevelPage = S.Checkpoint; }
	}
}
