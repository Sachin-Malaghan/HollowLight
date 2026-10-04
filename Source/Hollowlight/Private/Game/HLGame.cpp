// EMBERHOME: game flow. (CLAUDE.md: Game flow)
#include "Game/HLGame.h"

#include "Game/HLSaveGame.h"
#include "HAL/PlatformProcess.h"
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

void FHLGame::Init(UHLSaveGame* InSave, IHLAudioSink* InAudio, bool bTouchDevice)
{
	Save = InSave;
	Audio = InAudio;
	if (Save->Version < 2)
	{
		// Saves from the ten-level game: The Mountain was added as level 9, so later records move up one.
		if (Save->BestTimes.Num() == 10) { Save->BestTimes.Insert(0.f, 8); }
		if (Save->BestDeaths.Num() == 10) { Save->BestDeaths.Insert(0, 8); }
		Save->Version = 2;
	}
	if (Save->BestTimes.Num() < NumLevels()) { Save->BestTimes.SetNumZeroed(NumLevels()); }
	if (Save->ReachedCheckpoints.Num() < NumLevels()) { Save->ReachedCheckpoints.SetNumZeroed(NumLevels()); }
	if (Save->EmberMask.Num() < NumLevels()) { Save->EmberMask.SetNumZeroed(NumLevels()); }
	if (Save->BestDeaths.Num() < NumLevels()) { Save->BestDeaths.SetNumZeroed(NumLevels()); }
	Save->UnlockedLevels = FMath::Clamp(Save->UnlockedLevels, 1, NumLevels());
	Save->LastLevel = FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1);
	StartAttract(0);
	GoTo(EHLScreen::Title);
	if (bTouchDevice && !Save->bTouchCalibrated)
	{
		// First run on a phone: two taps on "the light" measure how this screen reports touches.
		ReturnScreen = EHLScreen::Title;
		CalibrateStep = 0;
		GoTo(EHLScreen::Calibrate);
	}
}

int32 FHLGame::NumCleared() const
{
	int32 N = 0;
	for (int32 I = 0; I < NumLevels() && I < Save->BestTimes.Num(); ++I) { N += Save->BestTimes[I] > 0 ? 1 : 0; }
	return N;
}

int32 FHLGame::ReachedCheckpoints(int32 Level) const
{
	const int32 Total = (int32)GetLevels()[Level].Checkpoints.size();
	if (Save->BestTimes.IsValidIndex(Level) && Save->BestTimes[Level] > 0) { return Total; }
	return Save->ReachedCheckpoints.IsValidIndex(Level) ? FMath::Clamp(Save->ReachedCheckpoints[Level], 0, Total) : 0;
}

int32 FHLGame::EmbersFound(int32 Level) const
{
	int32 N = 0;
	for (int32 I = 0; I < Save->EmberMask.Num() && I < NumLevels(); ++I)
	{
		if (Level >= 0 && I != Level) { continue; }
		const int32 Count = (int32)GetLevels()[I].Embers.size();
		for (int32 K = 0; K < Count; ++K) { N += (Save->EmberMask[I] >> K) & 1; }
	}
	return N;
}

int32 FHLGame::EmbersTotal() const
{
	int32 N = 0;
	for (const FLevelDef& L : GetLevels()) { N += (int32)L.Embers.size(); }
	return N;
}

double FHLGame::TotalBestTime() const
{
	double Sum = 0;
	for (int32 I = 0; I < NumLevels(); ++I)
	{
		if (!Save->BestTimes.IsValidIndex(I) || Save->BestTimes[I] <= 0) { return 0; }
		Sum += Save->BestTimes[I];
	}
	return Sum;
}

double FHLGame::BodyStretch() const
{
	const FPlayer& P = Sim.P;
	if (Sim.Phase == EPhase::Dying || P.Hang != 0 || P.Ladder >= 0) { return 0; }
	double V = 0;
	const double TL = RealTime - LandStamp, TJ = RealTime - JumpStamp;
	if (TL >= 0 && TL < 0.5) { V -= LandStrength * 0.26 * FMath::Exp(-TL / 0.075); }   // squash on landing
	if (TJ >= 0 && TJ < 0.5) { V += 0.2 * FMath::Exp(-TJ / 0.1); }                     // stretch on take-off
	if (!P.Grounded) { V += FMath::Clamp(FMath::Abs(P.VY) / 900.0, 0.0, 1.0) * 0.07; } // and a little while flying
	return FMath::Clamp(V, -0.3, 0.25);
}

void FHLGame::Burst(double X, double Y, int32 Count, double Speed, double Up, double Size, double Life, float Grey, float Alpha, double Gravity, bool bGrow, bool bWarm)
{
	for (int32 I = 0; I < Count && Particles.Num() < 360; ++I)
	{
		FHLParticle P;
		const double A = FMath::FRandRange(0.0, 2.0 * PI);
		const double V = Speed * FMath::FRandRange(0.35, 1.0);
		P.X = X + FMath::FRandRange(-3.0, 3.0);
		P.Y = Y + FMath::FRandRange(-1.5, 1.0);
		P.VX = FMath::Cos(A) * V;
		P.VY = FMath::Sin(A) * V * 0.45 - Up * FMath::FRandRange(0.4, 1.0);
		P.Life = P.MaxLife = Life * FMath::FRandRange(0.6, 1.0);
		P.Size = Size * FMath::FRandRange(0.6, 1.2);
		P.Gravity = Gravity;
		P.Grey = Grey;
		P.Alpha = Alpha;
		P.bGrow = bGrow;
		P.bWarm = bWarm;
		Particles.Add(P);
	}
}

// Game feel: dust, grit, ash, a jolt of the camera, a squash of the body. None of it touches the simulation.
void FHLGame::OnEventJuice(const HL::FEvent& E)
{
	switch (E.Type)
	{
	case EEvent::Footstep:
		if (E.Strength > 0.7) { Burst(E.X, E.Y, 1, 14, 8, 2.2, 0.45, 0.42f, 0.22f, -6, true); }
		break;
	case EEvent::Jump:
		JumpStamp = RealTime;
		Burst(E.X, E.Y, 4, 26, 6, 2.4, 0.4, 0.42f, 0.26f, -6, true);
		break;
	case EEvent::Land:
		LandStamp = RealTime;
		LandStrength = FMath::Max(0.4, E.Strength);
		Burst(E.X, E.Y, 4 + (int32)(E.Strength * 8.0), 30 + 50 * E.Strength, 8, 2.6, 0.55, 0.42f, 0.28f, -8, true);
		break;
	case EEvent::HardLand:
	case EEvent::Roll:
		Shake = FMath::Max(Shake, 2.2);
		Burst(E.X, E.Y, 14, 90, 14, 3.2, 0.7, 0.42f, 0.32f, -8, true);
		break;
	case EEvent::Vault:
		JumpStamp = RealTime;
		break;
	case EEvent::Slide:
		Burst(E.X, E.Y, 6, 40, 6, 2.6, 0.5, 0.42f, 0.26f, -6, true);
		break;
	case EEvent::CrateLand:
		Shake = FMath::Max(Shake, 0.8 + 1.2 * E.Strength);
		Burst(E.X, E.Y, 10, 70, 10, 3.0, 0.6, 0.42f, 0.3f, -8, true);
		break;
	case EEvent::TrapSnap:
	case EEvent::TrapSnapCrate:
		Shake = FMath::Max(Shake, E.Type == EEvent::TrapSnap ? 3.0 : 1.8);
		Burst(E.X, E.Y - 6, 8, 110, 60, 1.6, 0.5, 0.0f, 0.9f, 420, false);   // flecks of rust and earth
		break;
	case EEvent::GateShut:
		Shake = FMath::Max(Shake, 1.2);
		Burst(E.X, E.Y, 8, 60, 6, 2.8, 0.6, 0.42f, 0.28f, -8, true);
		break;
	case EEvent::Death:
		Shake = FMath::Max(Shake, 3.0);
		Burst(E.X, E.Y - 22, 26, 150, 70, 2.6, 0.9, 0.0f, 0.95f, 380, false);   // the dark comes apart
		Burst(E.X, E.Y - 14, 8, 60, 50, 1.4, 0.7, 1.0f, 0.9f, 60, false, true);  // the last sparks of the lantern
		break;
	case EEvent::EmberCollect:
		Burst(E.X, E.Y, 16, 90, 20, 1.6, 0.9, 1.0f, 0.9f, -30, false, true);
		break;
	case EEvent::DogPoof:
		Burst(E.X, E.Y, 10, 50, 10, 3.0, 0.5, 0.5f, 0.3f, -10, true);
		break;
	default:
		break;
	}
}

FString FHLGame::WantedNote() const
{
	if (Screen != EHLScreen::Playing || bAttract || Sim.Phase != EPhase::Playing) { return FString(); }
	if (Sim.Ghost.State == 1) { return TEXT("Something has come for the light.\nWHISTLE: the dog will see it off."); }
	for (const FNoteDef& N : Sim.Level->Notes)
	{
		if (Sim.P.X >= N.X0 && Sim.P.X <= N.X1 && (N.UntilGate < 0 || !Sim.Gates[N.UntilGate].Open)) { return FString(UTF8_TO_TCHAR(N.Text.c_str())); }
	}
	return FString();
}

void FHLGame::SaveProgress()
{
	if (Save && !bNoSave) { UGameplayStatics::SaveGameToSlot(Save, UHLSaveGame::SlotName, 0); }
}

void FHLGame::StartLevel(int32 Index, int32 Checkpoint)
{
	LevelIndex = FMath::Clamp(Index, 0, NumLevels() - 1);
	// Coming back to the run that was left (after closing the app, or from the title): same level, same
	// checkpoint, and the clock and falls as they stood when that cairn was lit.
	const bool bResume = Checkpoint >= 0 && LevelIndex == Save->LastLevel && Checkpoint == Save->LastCheckpoint;
	Sim.Load(GetLevels()[LevelIndex], Checkpoint);
	if (bResume)
	{
		Sim.PlayTime = Save->LastTime;
		Sim.Deaths = Save->LastDeaths;
	}
	Sim.P.JumpHeld = true;  // the key that confirmed the menu must be released before it jumps
	// Embers found on an earlier visit stay found.
	for (int32 K = 0; K < (int32)Sim.EmberTaken.size() && Save->EmberMask.IsValidIndex(LevelIndex); ++K)
	{
		if ((Save->EmberMask[LevelIndex] >> K) & 1) { Sim.EmberTaken[K] = true; }
	}
	Particles.Reset();
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
	Save->LastCheckpoint = Sim.CheckpointIndex;
	Save->LastTime = (float)Sim.PlayTime;
	Save->LastDeaths = Sim.Deaths;
	NoteText.Reset();
	NoteAlpha = 0;
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
	// Leaving the app mid-level: keep the clock as it stands, so coming back does not lose time played
	// since the last cairn (the place itself is always the last cairn).
	if (!bAttract && !bAutopilotInPlay && (Screen == EHLScreen::Playing || Screen == EHLScreen::Paused) && Sim.Phase != EPhase::Won)
	{
		Save->LastLevel = LevelIndex;
		Save->LastCheckpoint = Sim.CheckpointIndex;
		Save->LastTime = (float)Sim.PlayTime;
		Save->LastDeaths = Sim.Deaths;
	}
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
	PixelW = FMath::Max(1.0, ScreenW);
	PixelH = FMath::Max(1.0, ScreenH);

	HandleMenu(Menu);

	if (Screen != EHLScreen::Paused)
	{
		StepWorld(Dt, Controls);
	}
	UpdateCamera(Dt, false);

	// The script line at the top right: fade the old one out before the new one comes in.
	{
		const FString Want = WantedNote();
		if (Want == NoteText && !Want.IsEmpty()) { NoteAlpha = FMath::Min(1.0, NoteAlpha + Dt * 2.5); }
		else
		{
			NoteAlpha -= Dt * 3.0;
			if (NoteAlpha <= 0) { NoteAlpha = 0; NoteText = Want; }
		}
	}

	// Particles drift, rise or fall, and fade (frozen while paused).
	if (Screen != EHLScreen::Paused)
	{
		for (int32 I = Particles.Num() - 1; I >= 0; --I)
		{
			FHLParticle& Pt = Particles[I];
			Pt.Life -= Dt;
			if (Pt.Life <= 0) { Particles.RemoveAtSwap(I, EAllowShrinking::No); continue; }
			Pt.VY += Pt.Gravity * Dt;
			const double Drag = FMath::Exp(-Dt * (Pt.bGrow ? 3.0 : 1.2));
			Pt.VX *= Drag;
			if (Pt.bGrow) { Pt.VY *= Drag; }
			Pt.X += Pt.VX * Dt;
			Pt.Y += Pt.VY * Dt;
		}
		// Shoving a crate scuffs up a steady trail of dust.
		if (Sim.Phase == EPhase::Playing && Sim.P.PushTimer > 0 && Sim.P.Grounded)
		{
			PushDust -= Dt;
			if (PushDust <= 0)
			{
				PushDust = 0.07;
				Burst(Sim.P.X + Sim.P.Facing * 12.0, Sim.P.Y, 1, 22, 8, 2.6, 0.5, 0.42f, 0.26f, -6, true);
			}
		}
	}

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
			In.Down = Controls.Down;
			In.Act = Controls.Act;
		}
		Sim.Events.clear();
		Sim.Step(In);
		for (const HL::FEvent& E : Sim.Events)
		{
			OnEventJuice(E);
			if (E.Type == EEvent::Land && E.Strength > 0.75) { Shake = FMath::Max(Shake, 1.2); }
			if (E.Type == EEvent::EmberCollect && !bAttract && !bAutopilotInPlay && Save->EmberMask.IsValidIndex(LevelIndex))
			{
				for (int32 K = 0; K < (int32)Sim.EmberTaken.size(); ++K) { if (Sim.EmberTaken[K]) { Save->EmberMask[LevelIndex] |= (1 << K); } }
				SaveProgress();
			}
			if (E.Type == EEvent::Respawn) { PrevX = Sim.P.X; PrevY = Sim.P.Y; }
			if (E.Type == EEvent::Checkpoint && !bAttract && !bAutopilotInPlay && Screen == EHLScreen::Playing)
			{
				// Progress is kept checkpoint by checkpoint: CONTINUE comes back to this one.
				Save->LastCheckpoint = Sim.CheckpointIndex;
				Save->LastTime = (float)Sim.PlayTime;
				Save->LastDeaths = Sim.Deaths;
				if (Save->ReachedCheckpoints.IsValidIndex(LevelIndex))
				{
					Save->ReachedCheckpoints[LevelIndex] = FMath::Max(Save->ReachedCheckpoints[LevelIndex], Sim.CheckpointIndex + 1);
				}
				SaveProgress();
			}
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
	Save->LastCheckpoint = -1;
	Save->LastTime = 0.f;
	Save->LastDeaths = 0;
	if (Save->ReachedCheckpoints.IsValidIndex(LevelIndex)) { Save->ReachedCheckpoints[LevelIndex] = (int32)Sim.Level->Checkpoints.size(); }
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
	case EHLScreen::Title:
		// Android convention: Back on the first screen leaves the app. (Esc on desktop does nothing
		// here, so a stray key press can't close the game; use the QUIT button.)
#if PLATFORM_ANDROID
		SaveProgress();
		bQuitRequested = true;
#endif
		return;
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
		StartLevel(FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1), Save->LastCheckpoint);
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
		// The level that was left half-way carries on from its last cairn; any other starts from the beginning.
		if (B.Param < Save->UnlockedLevels) { StartLevel(B.Param, B.Param == Save->LastLevel ? Save->LastCheckpoint : -1); }
		break;
	case EHLAction::Resume:
		GoTo(EHLScreen::Playing);
		break;
	case EHLAction::RestartCheckpoint:
		{
			// The clock keeps running through a retry: going back to the cairn does not wind it back.
			const double Time = Sim.PlayTime;
			const int32 Falls = Sim.Deaths;
			StartLevel(LevelIndex, Sim.CheckpointIndex);
			Sim.PlayTime = Time;
			Sim.Deaths = Falls;
		}
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
	case EHLAction::ToggleTimer:
		Save->bTimer = !Save->bTimer;
		SaveProgress();
		break;
	case EHLAction::ToggleFlashing:
		Save->bReduceFlashing = !Save->bReduceFlashing;
		SaveProgress();
		break;
	case EHLAction::Calibrate:
		ReturnScreen = Screen;
		CalibrateStep = 0;
		GoTo(EHLScreen::Calibrate);
		break;
	case EHLAction::SkipCalibrate:
		Save->bTouchCalibrated = true;
		SaveProgress();
		GoTo(ReturnScreen);
		break;
	case EHLAction::PrivacyPolicy:
		// Store policy: the privacy policy must be reachable from inside the app.
		FPlatformProcess::LaunchURL(TEXT("https://sachin-malaghan.github.io/HollowLight/privacy.html"), nullptr, nullptr);
		break;
	case EHLAction::Quit:
		SaveProgress();
		bQuitRequested = true;
		break;
	case EHLAction::Pause:
		GoTo(EHLScreen::Paused);
		break;
	default:
		break;
	}
}

void FHLGame::HandleCalibration(const FHLMenuInput& Menu)
{
	if (!Menu.bClick || ScreenTime < 0.4) { return; }
	const FVector2D Raw(Menu.RawPointer.X / PixelW, Menu.RawPointer.Y / PixelH);
	const FVector2D Target = CalibrateTarget(CalibrateStep);
	// Ignore stray taps far from the light (the SKIP button is handled as a normal button).
	if (FMath::Abs(Raw.X - Target.X) > 0.2 || FMath::Abs(Raw.Y - Target.Y) > 0.3) { return; }
	CalibrateRaw[CalibrateStep] = Raw;
	if (Audio) { Audio->OnUiSound(EHLUiSound::Select); }
	if (CalibrateStep == 0)
	{
		CalibrateStep = 1;
		ScreenTime = 0;
		return;
	}

	// reported = Scale * true + Offset, solved per axis from the two taps.
	const FVector2D T0 = CalibrateTarget(0), T1 = CalibrateTarget(1);
	double SX = (CalibrateRaw[1].X - CalibrateRaw[0].X) / (T1.X - T0.X);
	double SY = (CalibrateRaw[1].Y - CalibrateRaw[0].Y) / (T1.Y - T0.Y);
	double OX = CalibrateRaw[0].X - SX * T0.X;
	double OY = CalibrateRaw[0].Y - SY * T0.Y;
	// A fingertip is not a precise pointer: only correct errors bigger than a finger's wobble, and
	// refuse anything implausible.
	const bool bSaneX = SX > 0.8 && SX < 1.25 && FMath::Abs(OX) < 0.15;
	const bool bSaneY = SY > 0.8 && SY < 1.25 && FMath::Abs(OY) < 0.15;
	const double ErrX = FMath::Max(FMath::Abs(CalibrateRaw[0].X - T0.X), FMath::Abs(CalibrateRaw[1].X - T1.X));
	const double ErrY = FMath::Max(FMath::Abs(CalibrateRaw[0].Y - T0.Y), FMath::Abs(CalibrateRaw[1].Y - T1.Y));
	if (!bSaneX || ErrX < 0.012) { SX = 1; OX = 0; }
	if (!bSaneY || ErrY < 0.02) { SY = 1; OY = 0; }
	Save->TouchScaleX = (float)SX;
	Save->TouchOffsetX = (float)OX;
	Save->TouchScaleY = (float)SY;
	Save->TouchOffsetY = (float)OY;
	Save->bTouchCalibrated = true;
	SaveProgress();
	GoTo(ReturnScreen);
}

void FHLGame::HandleMenu(const FHLMenuInput& Menu)
{
	if (Screen == EHLScreen::Calibrate) { HandleCalibration(Menu); }
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
			if (Buttons[I].bEnabled && Buttons[I].Box.IsInside(Screen == EHLScreen::Calibrate ? Menu.RawPointer : Menu.Pointer)) { Hover = I; break; }
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
	const int32 Row = Screen == EHLScreen::LevelSelect ? (NumLevels() > 10 ? 6 : 5) : 1;
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
