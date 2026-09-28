// Standalone check of the HOLLOWLIGHT core outside Unreal: the autopilot plays every level.
// Build + run: powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 [-Level N] [-Trace]
#include "../../Source/Hollowlight/Private/Core/HLAutopilot.h"
#include "../../Source/Hollowlight/Private/Core/HLLevel.h"
#include "../../Source/Hollowlight/Private/Core/HLSim.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace HL;

static const char* DeathName(EDeath D)
{
	switch (D)
	{
	case EDeath::Pit: return "pit";
	case EDeath::Trap: return "trap";
	case EDeath::Log: return "log";
	case EDeath::Water: return "water";
	default: return "-";
	}
}

int main(int Argc, char** Argv)
{
	int Only = -1;
	bool bTrace = false;
	bool bBaseOnly = false;
	double FineA = -1, FineB = -1;
	for (int I = 1; I < Argc; ++I)
	{
		if (!std::strcmp(Argv[I], "-level") && I + 1 < Argc) { Only = std::atoi(Argv[++I]) - 1; }
		if (!std::strcmp(Argv[I], "-trace")) { bTrace = true; }
		if (!std::strcmp(Argv[I], "-base")) { bBaseOnly = true; }
		if (!std::strcmp(Argv[I], "-fine") && I + 2 < Argc) { FineA = std::atof(Argv[++I]); FineB = std::atof(Argv[++I]); }
	}

	const std::vector<FLevelDef>& Levels = GetLevels();
	int Failures = 0;
	for (int Li = 0; Li < (int)Levels.size(); ++Li)
	{
		if (Only >= 0 && Li != Only) { continue; }
		FSim Sim;
		Sim.Load(Levels[Li]);
		FAutopilot Pilot;
		const auto T0 = std::chrono::steady_clock::now();
		const int MaxSteps = (int)(180.0 / kStep);
		int Step = 0;
		double LastTrace = -1;
		for (; Step < MaxSteps && Sim.Phase != EPhase::Won && !(bBaseOnly && Sim.Deaths > 0 && Sim.Phase == EPhase::Playing); ++Step)
		{
			const FInput In = bBaseOnly ? FAutopilot::BasePolicy(Sim) : Pilot.Decide(Sim);
			Sim.Events.clear();
			Sim.Step(In);
			for (const FEvent& E : Sim.Events)
			{
				if (E.Type == EEvent::Death)
				{
					std::printf("   L%d death (%s) at x=%.0f y=%.0f t=%.2f\n", Li + 1, DeathName(Sim.LastDeath), E.X, E.Y, Sim.Time);
				}
			}
			if (Sim.Time >= FineA && Sim.Time <= FineB)
			{
				std::printf("   t=%6.3f x=%7.2f y=%6.2f vx=%6.1f vy=%6.1f g=%d sup=%d:%d push=%.2f stuck=%.2f in=%d/%d", Sim.Time, Sim.P.X, Sim.P.Y,
					Sim.P.VX, Sim.P.VY, (int)Sim.P.Grounded, (int)Sim.P.SupportKind, Sim.P.SupportIndex, Sim.P.PushTimer, Sim.P.StuckTime, In.Dir, (int)In.Jump);
				for (const FCrate& C : Sim.Crates) { if (std::fabs(C.X - Sim.P.X) < 300) { std::printf(" crate=(%.1f,%.1f vy %.0f w%d)", C.X, C.Y, C.VY, (int)C.InWater); } }
				std::printf("\n");
			}
			if (bTrace && Sim.Time - LastTrace >= 0.25)
			{
				LastTrace = Sim.Time;
				std::printf("   t=%6.2f x=%7.1f y=%6.1f vx=%6.1f vy=%6.1f g=%d choice=%d cp=%d\n", Sim.Time, Sim.P.X, Sim.P.Y,
					Sim.P.VX, Sim.P.VY, (int)Sim.P.Grounded, Pilot.LastChoice, Sim.CheckpointIndex);
			}
		}
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		const bool bOk = Sim.Phase == EPhase::Won && Sim.Deaths == 0;
		if (!bOk) { ++Failures; }
		std::printf("%s L%-2d %-28s time %6.1fs deaths %d  maxX %6.0f/%6.0f  lookahead %.0f steps/s  (%.0f ms)\n",
			bOk ? "PASS" : "FAIL", Li + 1, Levels[Li].Name.c_str(), Sim.Time, Sim.Deaths, Sim.MaxX, Levels[Li].GoalX,
			Pilot.StepsSimulated / Sim.Time, Ms);
	}
	// Every checkpoint must also be a winnable start (that is where a death puts you).
	int CheckpointFailures = 0;
	for (int Li = 0; Li < (int)Levels.size(); ++Li)
	{
		if (Only >= 0 && Li != Only) { continue; }
		for (int Cp = 0; Cp < (int)Levels[Li].Checkpoints.size(); ++Cp)
		{
			FSim Sim;
			Sim.Load(Levels[Li], Cp);
			FAutopilot Pilot;
			for (int Step = 0; Step < (int)(120.0 / kStep) && Sim.Phase != EPhase::Won; ++Step)
			{
				const FInput In = Pilot.Decide(Sim);
				Sim.Events.clear();
				Sim.Step(In);
			}
			if (Sim.Phase != EPhase::Won || Sim.Deaths > 0)
			{
				++CheckpointFailures;
				std::printf("FAIL L%d from checkpoint %d (x=%.0f): deaths %d, reached %.0f\n", Li + 1, Cp, Levels[Li].Checkpoints[Cp], Sim.Deaths, Sim.MaxX);
			}
		}
	}
	std::printf("checkpoint starts: %s\n", CheckpointFailures ? "FAILURES" : "all winnable");
	Failures += CheckpointFailures;
	std::printf("%s\n", Failures ? "SOME LEVELS FAILED" : "ALL LEVELS PASSED");
	return Failures ? 1 : 0;
}
