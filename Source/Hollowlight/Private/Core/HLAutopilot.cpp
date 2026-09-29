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
		BestX = -1e9;
	}

	FInput FAutopilot::BasePolicy(const FSim& Sim)
	{
		FInput In;
		if (Sim.Phase != EPhase::Playing) { return In; }
		const FPlayer& P = Sim.P;
		In.Dir = 1;
		if (!P.Grounded)
		{
			In.Jump = P.VY < 0;  // hold for the full arc
			return In;
		}

		bool bTrigger = false;
		const double Front = P.X + kPlayerW * 0.5;
		if (P.PushTimer <= 0 && !Sim.HasSupport(Front + 10.0, P.Y, 8.0, 14.0))
		{
			// Pit edge. Look for the nearest thing to land on: step down onto it if it's right
			// there, jump if it's within reach, otherwise wait at the edge (a crate still settling
			// in the water, a bough still drifting over).
			double Nearest = -1;
			for (double DX = 12.0; DX <= 186.0; DX += 6.0)
			{
				if (Sim.HasSupport(Front + DX, P.Y, 90.0, 120.0)) { Nearest = DX; break; }
			}
			if (Nearest < 0) { In.Dir = 0; }
			else if (Nearest > 30.0) { bTrigger = true; }
		}
		for (const FTrap& T : Sim.Traps)
		{
			const double Ahead = T.X - P.X;
			if (!T.Closed && Ahead > 24.0 && Ahead < 50.0 && std::fabs(T.Y - P.Y) < 4.0) { bTrigger = true; }
		}
		if (P.StuckTime > 0.05) { bTrigger = true; }                               // blocked

		// A crate just ahead that is still falling or settling in water: let it come to rest.
		for (const FCrate& C : Sim.Crates)
		{
			const bool bAhead = C.X >= Front - 2.0 && C.X < Front + 30.0 && C.Y < P.Y + 80.0 && C.Y + kCrateSize > P.Y - kPlayerH;
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

	FInput FAutopilot::MacroInput(const FSim& Sim, const FMacro& Macro)
	{
		FInput In;
		In.Dir = Macro.Dir;
		if (Macro.bJump) { In.Jump = true; }
		else if (Macro.bSuppressJump) { In.Jump = !Sim.P.Grounded && Sim.P.VY < 0; }
		else { In.Jump = false; }
		return In;
	}

	FAutopilot::FOutcome FAutopilot::Evaluate(const FSim& Sim, const FMacro& Macro)
	{
		FSim Clone = Sim;
		FOutcome Out;
		const double StartX = Sim.P.X;
		const int Steps = (int)(Horizon / kStep);
		for (int I = 0; I < Steps; ++I)
		{
			const double T = I * kStep;
			const FInput In = T < Macro.Duration ? MacroInput(Clone, Macro) : BasePolicy(Clone);
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
		Out.Progress = Clone.P.X - StartX;
		return Out;
	}

	FInput FAutopilot::Decide(const FSim& Sim)
	{
		if (Sim.Phase != EPhase::Playing)
		{
			CurrentLeft = 0;
			StepsToReplan = 0;
			return FInput();
		}

		if (Sim.P.X > BestX + 4.0) { BestX = Sim.P.X; StallTimer = 0; }
		else { StallTimer += kStep; }

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
			};
			constexpr int NumCandidates = sizeof(Candidates) / sizeof(Candidates[0]);

			int Best = 0;
			const FOutcome Base = Evaluate(Sim, Candidates[0]);
			const bool bBaseGood = Base.bWon || (!Base.bDied && Base.Progress > 6.0 && StallTimer < 3.0);
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
					const double S = Score(Evaluate(Sim, Candidates[I]), I);
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
			return MacroInput(Sim, Current);
		}
		return BasePolicy(Sim);
	}
}
