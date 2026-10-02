// EMBERHOME core: attract-mode autopilot. (CLAUDE.md: Core / autopilot)
// A simple heuristic (head for a target; jump at pit edges, just before traps and when blocked; slide
// under low things; climb ladders) checked by looking ahead: the sim is cloned and run forward, and if
// the heuristic would die the autopilot waits, backs off or jumps instead - which is also how it times
// the swinging logs and boughs.
// On puzzle levels it follows the level's solution script (FLevelDef::Solution): go here, ACT, wait for
// the gate... That script is what proves the level can be solved (automation test Hollowlight.Levels.*).
#pragma once

#include "HLSim.h"

namespace HL
{
	class FAutopilot
	{
	public:
		void Reset();
		FInput Decide(const FSim& Sim);

		// The heuristic alone, heading for TargetX.
		static FInput BasePolicy(const FSim& Sim, double TargetX);

		double Horizon = 1.6;       // seconds of look-ahead
		int ReplanSteps = 6;        // physics steps between decisions
		int LastChoice = 0;         // index of the chosen macro (debug)
		int StepIndex = 0;          // position in the level's solution script
		long long StepsSimulated = 0;

	private:
		struct FMacro
		{
			int Dir = 1;            // relative to the way to the target: 1 toward, -1 away, 0 stand
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
			double Progress = 0;    // how much closer to the target
		};

		FOutcome Evaluate(const FSim& Sim, const FMacro& Macro, double TargetX);
		static FInput MacroInput(const FSim& Sim, const FMacro& Macro, double TargetX);
		bool AdvanceScript(const FSim& Sim, double& OutTarget, FInput& OutHold);

		FMacro Current;
		double CurrentLeft = 0;
		int StepsToReplan = 0;
		double StallTimer = 0;
		double BestDist = 1e18;
		int BestDistStep = -1;
		double StepTimer = 0;       // time spent in the current Act / Wait step
		double ClimbFromY = 0;
		bool bStarted = false;
	};
}
