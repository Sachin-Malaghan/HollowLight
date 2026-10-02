// EMBERHOME core: attract-mode autopilot. (CLAUDE.md: Core / autopilot)
#include "HLAutopilot.h"

#include <cmath>

namespace HL
{
	void FAutopilot::Reset()
	{
		Current = FMacro();
		CurrentLeft = 0;
		StepsToReplan = 0;
		StallTimer = 0;
		BestDist = 1e18;
		BestDistStep = -1;
		StepIndex = 0;
		StepTimer = 0;
		bStarted = false;
	}

	FInput FAutopilot::BasePolicy(const FSim& Sim, double TargetX)
	{
		FInput In;
		if (Sim.Phase != EPhase::Playing) { return In; }
		const FPlayer& P = Sim.P;
		if (P.Ladder >= 0)
		{
			In.Jump = true;                 // scripts only ever climb up
			return In;
		}
		if (P.Hang != 0) { return In; }     // the pull-up finishes by itself

		const double ToTarget = TargetX - P.X;
		if (std::fabs(ToTarget) < 7.0 && P.Grounded) { return In; }   // there
		const int D = ToTarget >= 0 ? 1 : -1;
		In.Dir = D;
		if (!P.Grounded)
		{
			In.Jump = P.VY < 0;             // hold for the full arc
			return In;
		}

		const double Front = P.X + D * kPlayerW * 0.5;

		// Water ahead too wide to jump, nothing floating in it, and a dry crate left behind:
		// go back around the crate so it can be pushed in.
		if (D > 0)
		{
			for (const FWaterDef& W : Sim.Level->Water)
			{
				const double BankY = W.Surface - 39.0;   // ground level a floating crate ends up flush with
				if (W.X0 < P.X - 2.0 || W.X0 > P.X + 700.0 || W.X1 - W.X0 < 0.86 * kRunSpeed) { continue; }
				if (P.Y > BankY + 4.0 || P.Y < BankY - kCrateSize - 4.0) { continue; }   // not on this bank (or on its crate)
				bool bBridged = false;
				for (const FCrate& C : Sim.Crates) { bBridged = bBridged || (C.InWater && C.X + kCrateSize > W.X0 && C.X < W.X1); }
				if (bBridged) { break; }
				for (const FCrate& C : Sim.Crates)
				{
					const bool bDry = C.Grounded && !C.InWater && C.X + kCrateSize <= W.X0 + 1.0 && std::fabs(C.Y + kCrateSize - BankY) < 2.0;
					const bool bBehind = C.X < P.X + kPlayerW * 0.5 && C.X > P.X - 700.0;   // the child is on it or past it
					if (bDry && bBehind)
					{
						In.Dir = -1;
						In.Jump = (P.X - kPlayerW * 0.5) - (C.X + kCrateSize) < 30.0 && P.Y > C.Y + 2.0;   // hop back over it
						return In;
					}
				}
				break;
			}
		}

		// Something at head height with room underneath: slide (or keep crawling) under it.
		{
			const double A0 = Front + D * 1.0, A1 = Front + D * 64.0;
			const double X0 = std::min(A0, A1), X1 = std::max(A0, A1);
			const bool bHeadBlocked = !Sim.IsFree({ X0, P.Y - kPlayerH + 1.0, X1, P.Y - kLowH - 1.0 });
			const bool bLowClear = Sim.IsFree({ X0, P.Y - kLowH + 0.5, X1, P.Y - 1.0 });
			if ((bHeadBlocked && bLowClear) || (P.Low && !Sim.CanStand()))
			{
				In.Down = true;
				return In;
			}
		}

		bool bTrigger = false;
		// A wall dead ahead is not a pit edge (there is no ground at foot level because it is all cliff).
		const bool bWallAhead = !Sim.IsFree({ std::min(Front + D * 2.0, Front + D * 12.0), P.Y - 12.0, std::max(Front + D * 2.0, Front + D * 12.0), P.Y - 1.0 });
		if (P.PushTimer <= 0 && !bWallAhead && !Sim.HasSupport(Front + D * 10.0, P.Y, 8.0, 14.0))
		{
			// Pit edge. Look for the nearest thing to land on: step down onto it if it's right
			// there, jump if it's within reach, otherwise wait at the edge (a crate still settling
			// in the water, a bough still drifting over).
			double Nearest = -1;
			const double Reach = 0.78 * kRunSpeed + 4.0;
			for (double DX = 12.0; DX <= Reach; DX += 6.0)
			{
				if (Sim.HasSupport(Front + D * DX, P.Y, 90.0, 320.0)) { Nearest = DX; break; }   // a long drop is fine; a pit has no floor
			}
			if (Nearest < 0) { In.Dir = 0; }
			else if (Nearest > 30.0) { bTrigger = true; }
		}
		for (const FTrap& T : Sim.Traps)
		{
			const double Ahead = (T.X - P.X) * D;
			if (!T.Closed && Ahead > 24.0 && Ahead < 50.0 && std::fabs(T.Y - P.Y) < 4.0) { bTrigger = true; }
		}
		if (P.StuckTime > 0.05) { bTrigger = true; }                               // blocked (or at a ladder)

		// A crate just ahead that is still falling or settling in water: let it come to rest.
		for (const FCrate& C : Sim.Crates)
		{
			const double Near = D > 0 ? C.X - Front : Front - (C.X + kCrateSize);
			const bool bAhead = Near >= -2.0 && Near < 30.0 && C.Y < P.Y + 80.0 && C.Y + kCrateSize > P.Y - kPlayerH;
			const bool bResting = C.Grounded || (C.InWater && std::fabs(C.VY) < 12.0);
			if (bAhead && !bResting)
			{
				In.Dir = 0;
				bTrigger = false;
			}
		}
		In.Jump = bTrigger;
		return In;
	}

	FInput FAutopilot::MacroInput(const FSim& Sim, const FMacro& Macro, double TargetX)
	{
		FInput In;
		const int D = TargetX >= Sim.P.X ? 1 : -1;
		In.Dir = Macro.Dir * D;
		In.Down = Macro.bDown;
		if (Macro.bJump) { In.Jump = true; }
		else if (Macro.bSuppressJump) { In.Jump = !Sim.P.Grounded && Sim.P.VY < 0; }
		else { In.Jump = false; }
		if (Sim.P.Ladder >= 0) { In.Jump = true; In.Dir = 0; In.Down = false; }   // never let go of a ladder half way
		return In;
	}

	FAutopilot::FOutcome FAutopilot::Evaluate(const FSim& Sim, const FMacro& Macro, double TargetX)
	{
		FSim Clone = Sim;
		FOutcome Out;
		const double StartDist = std::fabs(TargetX - Sim.P.X);
		const int Steps = (int)(Horizon / kStep);
		for (int I = 0; I < Steps; ++I)
		{
			const double T = I * kStep;
			const FInput In = T < Macro.Duration ? MacroInput(Clone, Macro, TargetX) : BasePolicy(Clone, TargetX);
			Clone.Events.clear();
			Clone.Step(In);
			++StepsSimulated;
			if (Clone.Phase == EPhase::Dying)
			{
				Out.bDied = true;
				Out.DeathTime = T;
				break;
			}
			if (Clone.Phase == EPhase::Won)
			{
				Out.bWon = true;
				break;
			}
		}
		Out.Progress = StartDist - std::fabs(TargetX - Clone.P.X);
		return Out;
	}

	// Works through the solution script. Returns true when the autopilot should just hold OutHold this
	// step (pressing ACT, waiting); otherwise OutTarget is where to head.
	bool FAutopilot::AdvanceScript(const FSim& Sim, double& OutTarget, FInput& OutHold)
	{
		const std::vector<FSolveStep>& Sol = Sim.Level->Solution;
		const FPlayer& P = Sim.P;
		OutTarget = Sim.Level->GoalX;

		// Started at a checkpoint? Everything in the script before the first "go" at or beyond that
		// checkpoint belongs to puzzles that lie behind, so pick the script up there. (This is also the
		// rule level designers follow: a checkpoint must be a clean cut - nothing behind it is needed.)
		if (!bStarted)
		{
			bStarted = true;
			if (Sim.CheckpointIndex >= 0)
			{
				const double From = Sim.Level->Checkpoints[Sim.CheckpointIndex] - 12.0;
				StepIndex = (int)Sol.size();
				for (int I = 0; I < (int)Sol.size(); ++I)
				{
					if (Sol[I].Kind == EStepKind::Go && Sol[I].V >= From) { StepIndex = I; break; }
				}
			}
		}

		while (StepIndex < (int)Sol.size())
		{
			const FSolveStep& S = Sol[StepIndex];
			bool bDone = false;
			switch (S.Kind)
			{
			case EStepKind::Go:
				if (std::fabs(P.X - S.V) < 7.0 && P.Grounded && P.Ladder < 0 && P.Hang == 0 && (S.Y < -1.0e8 || std::fabs(P.Y - S.Y) < 12.0)) { bDone = true; }
				else { OutTarget = S.V; return false; }
				break;
			case EStepKind::Act:
				StepTimer += kStep;
				OutHold.Act = StepTimer < 0.08;
				if (StepTimer >= 0.2) { bDone = true; } else { return true; }
				break;
			case EStepKind::Wait:
				StepTimer += kStep;
				if (StepTimer >= S.V) { bDone = true; } else { return true; }
				break;
			case EStepKind::WaitGate:
				if (Sim.Gates[(int)S.V].Amount > 0.9) { bDone = true; } else { return true; }
				break;
			case EStepKind::WaitDogStay:
				if (!Sim.Dog.Active || Sim.Dog.Mode == EDogMode::Stay) { bDone = true; } else { return true; }
				break;
			case EStepKind::Climb:
				if (StepTimer == 0) { ClimbFromY = P.Y; }
				StepTimer += kStep;
				if (P.Grounded && P.Ladder < 0 && P.Y < ClimbFromY - 20.0) { bDone = true; }
				else { OutHold.Jump = true; return true; }
				break;
			case EStepKind::CallDog:
				if (!Sim.Dog.Active || (Sim.Dog.Mode != EDogMode::Stay && Sim.Dog.Mode != EDogMode::GoStay)) { bDone = true; break; }
				StepTimer += kStep;
				OutHold.Act = StepTimer < 0.08;
				if (StepTimer >= 0.2) { bDone = true; } else { return true; }
				break;
			}
			if (bDone)
			{
				++StepIndex;
				StepTimer = 0;
			}
		}
		return false;
	}

	FInput FAutopilot::Decide(const FSim& Sim)
	{
		if (Sim.Phase != EPhase::Playing)
		{
			CurrentLeft = 0;
			StepsToReplan = 0;
			return FInput();
		}

		// Something pale is coming: whistle, and the dog sees it off.
		if (Sim.Ghost.State == 1 && std::fabs(Sim.Ghost.X - Sim.P.X) < 150.0 && Sim.P.Grounded && Sim.ActKind() == EActKind::Whistle)
		{
			FInput In;
			In.Act = !Sim.P.ActHeld;
			return In;
		}

		double Target = Sim.Level->GoalX;
		FInput Hold;
		if (AdvanceScript(Sim, Target, Hold))
		{
			CurrentLeft = 0;
			StepsToReplan = 0;
			return Hold;
		}

		const double Dist = std::fabs(Target - Sim.P.X);
		if (BestDistStep != StepIndex || Dist < BestDist - 4.0)
		{
			BestDist = Dist;
			BestDistStep = StepIndex;
			StallTimer = 0;
		}
		else
		{
			StallTimer += kStep;
		}

		if (--StepsToReplan <= 0)
		{
			StepsToReplan = ReplanSteps;

			static const FMacro Candidates[] = {
				{ 1, false, false, 0.0 },   // 0 heuristic
				{ 0, false, true, 0.2 },    // 1 wait
				{ 0, false, true, 0.5 },    // 2 wait longer
				{ 0, false, true, 1.0 },    // 3 wait a second
				{ -1, false, true, 0.3 },   // 4 back off
				{ 1, true, false, 0.45 },   // 5 jump forward now
				{ 0, true, false, 0.45 },   // 6 jump in place
				{ 1, false, true, 0.3 },    // 7 walk on without jumping
				{ -1, false, true, 0.8 },   // 8 back off further
				{ 1, false, true, 0.6, true },   // 9 slide
			};
			constexpr int NumCandidates = sizeof(Candidates) / sizeof(Candidates[0]);

			int Best = 0;
			const FOutcome Base = Evaluate(Sim, Candidates[0], Target);
			const FInput BaseNow = BasePolicy(Sim, Target);
			const bool bRetreat = BaseNow.Dir != 0 && BaseNow.Dir != (Target >= Sim.P.X ? 1 : -1);   // deliberately going back for something
			const bool bClimbing = Sim.P.Ladder >= 0;
			const bool bBaseGood = Base.bWon || (!Base.bDied && (bRetreat || bClimbing || (Base.Progress > 6.0 && StallTimer < 3.0)));
			if (!bBaseGood)
			{
				auto Score = [](const FOutcome& O, int Index)
				{
					if (O.bWon) { return 1.0e6; }
					if (O.bDied) { return -1.0e5 + O.DeathTime * 1000.0 + O.Progress * 0.01; }
					return O.Progress - (Index == 0 ? 0.0 : 20.0);
				};
				double BestScore = Score(Base, 0);
				for (int I = 1; I < NumCandidates; ++I)
				{
					const double S = Score(Evaluate(Sim, Candidates[I], Target), I);
					if (S > BestScore) { BestScore = S; Best = I; }
				}
			}
			LastChoice = Best;
			Current = Candidates[Best];
			CurrentLeft = Current.Duration;
		}

		if (CurrentLeft > 0)
		{
			CurrentLeft -= kStep;
			return MacroInput(Sim, Current, Target);
		}
		return BasePolicy(Sim, Target);
	}
}
