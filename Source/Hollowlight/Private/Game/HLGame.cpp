// HOLLOWLIGHT: game flow. (CLAUDE.md: Game flow)
#include "Game/HLGame.h"

#include "Game/HLSaveGame.h"
#include "Kismet/GameplayStatics.h"

using namespace HL;

namespace
{
	constexpr double kCameraLead = 0.36;      // keep the child ~36% from the left edge
	constexpr double kCompleteDelay = 2.8;    // lamp glow before the results card
	constexpr double kAttractNextDelay = 3.5;
}

int32 FHLGame::NumLevels() const
{
	return (int32)GetLevels().size();
}

void FHLGame::Init(UHLSaveGame* InSave, IHLAudioSink* InAudio)
{
	Save = InSave;
	Audio = InAudio;
	if (Save->BestTimes.Num() < NumLevels()) { Save->BestTimes.SetNumZeroed(NumLevels()); }
	if (Save->BestDeaths.Num() < NumLevels()) { Save->BestDeaths.SetNumZeroed(NumLevels()); }
	Save->UnlockedLevels = FMath::Clamp(Save->UnlockedLevels, 1, NumLevels());
	Save->LastLevel = FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1);
	StartAttract(0);
	GoTo(EHLScreen::Title);
}

void FHLGame::SaveProgress()
{
	if (Save && !bNoSave) { UGameplayStatics::SaveGameToSlot(Save, UHLSaveGame::SlotName, 0); }
}

void FHLGame::StartLevel(int32 Index, int32 Checkpoint)
{
	LevelIndex = FMath::Clamp(Index, 0, NumLevels() - 1);
	Sim.Load(GetLevels()[LevelIndex], Checkpoint);
	Sim.P.JumpHeld = true;  // the key that confirmed the menu must be released before it jumps
	Pilot.Reset();
	bAttract = false;
	Accumulator = 0;
	PrevX = RenderX = Sim.P.X;
	PrevY = RenderY = Sim.P.Y;
	LevelTime = 0;
	Lightning = 0;
	Shake = 0;
	UpdateCamera(0, true);
	Save->LastLevel = LevelIndex;
	SaveProgress();
	GoTo(EHLScreen::Playing);
}

void FHLGame::StartAttract(int32 Index)
{
	LevelIndex = FMath::Clamp(Index, 0, NumLevels() - 1);
	Sim.Load(GetLevels()[LevelIndex]);
	Pilot.Reset();
	bAttract = true;
	Accumulator = 0;
	PrevX = RenderX = Sim.P.X;
	PrevY = RenderY = Sim.P.Y;
	LevelTime = 0;
	UpdateCamera(0, true);
}

void FHLGame::GoTo(EHLScreen NewScreen)
{
	Screen = NewScreen;
	ScreenTime = 0;
	Focus = 0;
	Buttons.Reset();
}

void FHLGame::OnAppBackground()
{
	if (Screen == EHLScreen::Playing) { GoTo(EHLScreen::Paused); }
	SaveProgress();
}

void FHLGame::Tick(float DeltaSeconds, const FHLControls& Controls, const FHLMenuInput& Menu, double ScreenW, double ScreenH)
{
	const double Dt = FMath::Clamp((double)DeltaSeconds, 0.0, 0.1);
	RealTime += Dt;
	ScreenTime += Dt;
	Scale = FMath::Max(1.0, ScreenH) / kViewHeight;
	ViewW = FMath::Max(1.0, ScreenW) / Scale;

	HandleMenu(Menu);

	if (Screen != EHLScreen::Paused)
	{
		StepWorld(Dt, Controls);
	}
	UpdateCamera(Dt, false);

	Shake = FMath::Max(0.0, Shake - Dt * 12.0);
	Lightning = FMath::Max(0.0, Lightning - Dt * 3.2);

	if (Audio)
	{
		const FTheme& T = Sim.Level->Theme;
		const bool bMenu = Screen != EHLScreen::Playing;
		double Warm = 0;
		if (T.Dawn > 0)
		{
			Warm = T.Dawn * SmoothStep(0.45, 1.0, (RenderX - Sim.Level->StartX) / FMath::Max(1.0, Sim.Level->GoalX - Sim.Level->StartX));
		}
		Audio->SetMix((float)T.Rain, (float)T.Wind, Save->bMusic, Save->bSound, bMenu, (float)Warm);
	}
}

void FHLGame::StepWorld(double Dt, const FHLControls& Controls)
{
	LevelTime += Dt;
	Accumulator += Dt;
	const FTheme& T = Sim.Level->Theme;
	while (Accumulator >= kStep)
	{
		PrevX = Sim.P.X;
		PrevY = Sim.P.Y;
		FInput In;
		if (bAttract || (bAutopilotInPlay && Screen == EHLScreen::Playing))
		{
			In = Pilot.Decide(Sim);
		}
		else if (Screen == EHLScreen::Playing)
		{
			In.Dir = Controls.Dir;
			In.Jump = Controls.Jump;
		}
		Sim.Events.clear();
		Sim.Step(In);
		for (const HL::FEvent& E : Sim.Events)
		{
			if (E.Type == EEvent::Death && (Sim.LastDeath == EDeath::Trap || Sim.LastDeath == EDeath::Log)) { Shake = 3.0; }
			if (E.Type == EEvent::Land && E.Strength > 0.75) { Shake = FMath::Max(Shake, 1.2); }
			if (E.Type == EEvent::Respawn) { PrevX = Sim.P.X; PrevY = Sim.P.Y; }
			if (Audio)
			{
				const float Pan = (float)FMath::Clamp(((E.X - CamX) / FMath::Max(1.0, ViewW)) * 2.0 - 1.0, -1.0, 1.0);
				const float Dist = (float)FMath::Abs(E.X - Sim.P.X);
				Audio->OnSimEvent(E, Pan, Dist);
			}
		}
		Accumulator -= kStep;
	}

	const double A = Accumulator / kStep;
	if (FMath::Abs(Sim.P.X - PrevX) > 40.0 || FMath::Abs(Sim.P.Y - PrevY) > 40.0)
	{
		RenderX = Sim.P.X;
		RenderY = Sim.P.Y;
	}
	else
	{
		RenderX = FMath::Lerp(PrevX, Sim.P.X, A);
		RenderY = FMath::Lerp(PrevY, Sim.P.Y, A);
	}

	// Storm lightning: a flash, then thunder a moment later.
	if (T.Lightning > 0 && RealTime >= NextLightningCheck)
	{
		NextLightningCheck = RealTime + 1.0;
		if (FMath::FRand() < T.Lightning)
		{
			const float Strength = FMath::FRandRange(0.6f, 1.0f);
			Lightning = Save->bReduceFlashing ? 0.0 : Strength;
			if (Audio) { Audio->OnThunder(Strength, FMath::FRandRange(0.3f, 1.4f)); }
		}
	}

	if (bAttract && Sim.Phase == EPhase::Won && Sim.PhaseTime > kAttractNextDelay)
	{
		// Cycle the attract mode through the levels the player has reached.
		StartAttract((LevelIndex + 1) % FMath::Max(1, Save->UnlockedLevels));
	}
	else if (!bAttract && Screen == EHLScreen::Playing && Sim.Phase == EPhase::Won && Sim.PhaseTime > kCompleteDelay)
	{
		CompleteLevel();
	}
}

void FHLGame::UpdateCamera(double Dt, bool bSnap)
{
	const FLevelDef& L = *Sim.Level;
	double TX = RenderX - kCameraLead * ViewW;
	TX = FMath::Clamp(TX, L.MinX, FMath::Max(L.MinX, L.MaxX - ViewW));
	const double TY = FMath::Min(0.0, (RenderY - kPlayerH) - 110.0);
	if (bSnap)
	{
		CamX = TX;
		CamY = TY;
		return;
	}
	CamX += (TX - CamX) * (1.0 - FMath::Exp(-Dt * 6.0));
	CamY += (TY - CamY) * (1.0 - FMath::Exp(-Dt * 4.0));
}

void FHLGame::CompleteLevel()
{
	ResultTime = (float)Sim.PlayTime;
	ResultDeaths = Sim.Deaths;
	float& Best = Save->BestTimes[LevelIndex];
	bNewBest = Best <= 0 || ResultTime < Best;
	if (bNewBest)
	{
		Best = ResultTime;
		Save->BestDeaths[LevelIndex] = ResultDeaths;
	}
	Save->UnlockedLevels = FMath::Clamp(FMath::Max(Save->UnlockedLevels, LevelIndex + 2), 1, NumLevels());
	const bool bLast = LevelIndex == NumLevels() - 1;
	if (bLast)
	{
		Save->bFinishedGame = true;
		Save->LastLevel = 0;
	}
	else
	{
		Save->LastLevel = LevelIndex + 1;
	}
	SaveProgress();
	GoTo(bLast ? EHLScreen::Ending : EHLScreen::LevelComplete);
	if (Audio) { Audio->OnUiSound(EHLUiSound::Select); }
}

void FHLGame::Back()
{
	switch (Screen)
	{
	case EHLScreen::LevelSelect:
	case EHLScreen::Settings:
		GoTo(ReturnScreen);
		break;
	case EHLScreen::Credits:
		GoTo(EHLScreen::Settings);
		break;
	case EHLScreen::Paused:
		GoTo(EHLScreen::Playing);
		break;
	case EHLScreen::Playing:
		GoTo(EHLScreen::Paused);
		break;
	case EHLScreen::LevelComplete:
		ReturnScreen = EHLScreen::Title;
		GoTo(EHLScreen::LevelSelect);
		break;
	default:
		return;
	}
	if (Audio) { Audio->OnUiSound(EHLUiSound::Back); }
}

void FHLGame::Activate(const FHLButton& B)
{
	if (!B.bEnabled) { return; }
	if (Audio && B.Action != EHLAction::Pause) { Audio->OnUiSound(B.Action == EHLAction::Back ? EHLUiSound::Back : EHLUiSound::Select); }
	switch (B.Action)
	{
	case EHLAction::Play:
		StartLevel(FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1));
		break;
	case EHLAction::Levels:
		ReturnScreen = Screen;
		GoTo(EHLScreen::LevelSelect);
		break;
	case EHLAction::Settings:
		ReturnScreen = Screen;
		GoTo(EHLScreen::Settings);
		break;
	case EHLAction::Credits:
		GoTo(EHLScreen::Credits);
		break;
	case EHLAction::Back:
		Back();
		break;
	case EHLAction::SelectLevel:
		if (B.Param < Save->UnlockedLevels) { StartLevel(B.Param); }
		break;
	case EHLAction::Resume:
		GoTo(EHLScreen::Playing);
		break;
	case EHLAction::RestartCheckpoint:
		StartLevel(LevelIndex, Sim.CheckpointIndex);
		break;
	case EHLAction::RestartLevel:
	case EHLAction::Replay:
		StartLevel(LevelIndex);
		break;
	case EHLAction::QuitToTitle:
		StartAttract(0);
		GoTo(EHLScreen::Title);
		break;
	case EHLAction::NextLevel:
		StartLevel(LevelIndex + 1);
		break;
	case EHLAction::PlayAgain:
		StartLevel(0);
		break;
	case EHLAction::ToggleMusic:
		Save->bMusic = !Save->bMusic;
		SaveProgress();
		break;
	case EHLAction::ToggleSound:
		Save->bSound = !Save->bSound;
		SaveProgress();
		break;
	case EHLAction::CycleTouch:
		Save->TouchMode = (EHLTouchMode)(((int32)Save->TouchMode + 1) % 3);
		SaveProgress();
		break;
	case EHLAction::ToggleGrain:
		Save->bFilmGrain = !Save->bFilmGrain;
		SaveProgress();
		break;
	case EHLAction::ToggleFlashing:
		Save->bReduceFlashing = !Save->bReduceFlashing;
		SaveProgress();
		break;
	case EHLAction::Pause:
		GoTo(EHLScreen::Paused);
		break;
	default:
		break;
	}
}

void FHLGame::HandleMenu(const FHLMenuInput& Menu)
{
	if (Screen == EHLScreen::Playing && (Menu.Pause || Menu.Back))
	{
		GoTo(EHLScreen::Paused);
		if (Audio) { Audio->OnUiSound(EHLUiSound::Back); }
		return;
	}

	const int32 N = Buttons.Num();
	if (Menu.bPointerMoved || Menu.bClick) { bPointerActive = true; }

	int32 Hover = -1;
	if (Menu.bPointerValid)
	{
		for (int32 I = 0; I < N; ++I)
		{
			if (Buttons[I].bEnabled && Buttons[I].Box.IsInside(Menu.Pointer)) { Hover = I; break; }
		}
	}
	if (Menu.bPointerMoved && Hover >= 0 && Hover != Focus)
	{
		Focus = Hover;
		if (Audio) { Audio->OnUiSound(EHLUiSound::Move); }
	}
	if (Menu.bClick && Hover >= 0)
	{
		const FHLButton B = Buttons[Hover];
		Activate(B);
		return;
	}

	// Keyboard / gamepad navigation (not during play: there the only button is pause).
	if (N == 0 || Screen == EHLScreen::Playing) { return; }
	if (Menu.Up || Menu.Down || Menu.Left || Menu.Right || Menu.Confirm) { bPointerActive = false; }
	auto Move = [&](int32 Delta)
	{
		for (int32 Tries = 0; Tries < N; ++Tries)
		{
			Focus = (Focus + Delta + N) % N;
			if (Buttons[Focus].bEnabled) { break; }
		}
		if (Audio) { Audio->OnUiSound(EHLUiSound::Move); }
	};
	const int32 Row = Screen == EHLScreen::LevelSelect ? 5 : 1;
	if (Menu.Up) { Move(-Row); }
	if (Menu.Down) { Move(Row); }
	if (Menu.Left) { Move(-1); }
	if (Menu.Right) { Move(1); }
	Focus = FMath::Clamp(Focus, 0, N - 1);
	if (Menu.Confirm && Buttons.IsValidIndex(Focus))
	{
		const FHLButton B = Buttons[Focus];
		Activate(B);
		return;
	}
	if (Menu.Back) { Back(); }
}
