// EMBERHOME core: attract-mode autopilot. (CLAUDE.md: Core / autopilot)
// A simple heuristic (run right; jump at pit edges, just before traps and when blocked) checked by
// looking ahead: the sim is cloned and run forward, and if the heuristic would die the autopilot
// waits, backs off or jumps instead - which is also how it times the swinging logs and boughs.
// The same autopilot proves every level can be finished (automation test Hollowlight.Levels.*).
#pragma once

#include "HLSim.h"

namespace HL
{
	class FAutopilot
	{
	public:
		void Reset();
		FInput Decide(const FSim& Sim);

		static FInput BasePolicy(const FSim& Sim);

		double Horizon = 1.6;       // seconds of look-ahead
		int ReplanSteps = 6;        // physics steps between decisions
		int LastChoice = 0;         // index of the chosen macro (debug)
		long long StepsSimulated = 0;

	private:
		struct FMacro
		{
			int Dir = 1;
			bool bJump = false;
			bool bSuppressJump = false;
			double Duration = 0;    // 0 = pure heuristic
			bool bDown = false;     // slide / stay low
		};
		struct FOutcome
		{
			bool bDied = false;
			bool bWon = false;
			double DeathTime = 0;
			double Progress = 0;
		};

		FOutcome Evaluate(const FSim& Sim, const FMacro& Macro);
		static FInput MacroInput(const FSim& Sim, const FMacro& Macro);

		FMacro Current;
		double CurrentLeft = 0;
		int StepsToReplan = 0;
		double StallTimer = 0;
		double BestX = -1e9;
	};
}
