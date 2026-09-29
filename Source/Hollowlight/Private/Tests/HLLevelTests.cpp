// EMBERHOME: automation tests - every level (and every checkpoint start) can be finished,
// and the core physics rules from the spec hold. (CLAUDE.md: Testing)
#include "Misc/AutomationTest.h"

#include "Core/HLAutopilot.h"
#include "Core/HLLevel.h"
#include "Core/HLSim.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace HL;

namespace HLTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	// Runs the autopilot; returns true if it reached the lamp post without dying.
	bool RunAutopilot(const FLevelDef& Level, int32 Checkpoint, double MaxSeconds, double& OutTime, int32& OutDeaths)
	{
		FSim Sim;
		Sim.Load(Level, Checkpoint);
		FAutopilot Pilot;
		const int32 Steps = (int32)(MaxSeconds / kStep);
		for (int32 I = 0; I < Steps && Sim.Phase != EPhase::Won; ++I)
		{
			const FInput In = Pilot.Decide(Sim);
			Sim.Events.clear();
			Sim.Step(In);
		}
		OutTime = Sim.Time;
		OutDeaths = Sim.Deaths;
		return Sim.Phase == EPhase::Won && Sim.Deaths == 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLAutopilotFinishes, "Hollowlight.Levels.AutopilotFinishes", HLTests::Flags)
bool FHLAutopilotFinishes::RunTest(const FString& Parameters)
{
	const std::vector<FLevelDef>& Levels = GetLevels();
	TestTrue(TEXT("at least ten levels"), Levels.size() >= 10);
	for (int32 L = 0; L < (int32)Levels.size(); ++L)
	{
		double Time = 0;
		int32 Deaths = 0;
		const bool bOk = HLTests::RunAutopilot(Levels[L], -1, 180.0, Time, Deaths);
		TestTrue(FString::Printf(TEXT("level %d (%hs) finished without dying (%.1fs, %d deaths)"), L + 1, Levels[L].Name.c_str(), Time, Deaths), bOk);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLCheckpointsWinnable, "Hollowlight.Levels.EveryCheckpointWinnable", HLTests::Flags)
bool FHLCheckpointsWinnable::RunTest(const FString& Parameters)
{
	const std::vector<FLevelDef>& Levels = GetLevels();
	for (int32 L = 0; L < (int32)Levels.size(); ++L)
	{
		for (int32 C = 0; C < (int32)Levels[L].Checkpoints.size(); ++C)
		{
			double Time = 0;
			int32 Deaths = 0;
			TestTrue(FString::Printf(TEXT("level %d from checkpoint %d"), L + 1, C), HLTests::RunAutopilot(Levels[L], C, 120.0, Time, Deaths));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLAttractTiming, "Hollowlight.Levels.AttractFinishesLevelOneInAbout20s", HLTests::Flags)
bool FHLAttractTiming::RunTest(const FString& Parameters)
{
	double Time = 0;
	int32 Deaths = 0;
	const bool bOk = HLTests::RunAutopilot(GetLevels()[0], -1, 60.0, Time, Deaths);
	TestTrue(TEXT("level 1 finished"), bOk);
	TestTrue(FString::Printf(TEXT("about 20 s (%.1f)"), Time), Time > 15.0 && Time < 26.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLJumpPhysics, "Hollowlight.Physics.JumpHopAndCoyote", HLTests::Flags)
bool FHLJumpPhysics::RunTest(const FString& Parameters)
{
	FLevelDef Flat;
	Flat.Ground = { { -1000, 1000, 300 } };
	Flat.StartX = 0;
	Flat.GoalX = 5000;
	Flat.MinX = -1000;
	Flat.MaxX = 5000;

	// Full jump apex: v^2 / 2g = 104.5 continuous; semi-implicit Euler at 1/120 s peaks v*dt/2 lower (~102.2).
	{
		FSim Sim;
		Sim.Load(Flat);
		double MinY = 1e9;
		for (int32 I = 0; I < 120; ++I)
		{
			FInput In;
			In.Jump = true;
			Sim.Step(In);
			MinY = FMath::Min(MinY, Sim.P.Y);
		}
		TestNearlyEqual(TEXT("full jump height"), 300.0 - MinY, kJumpSpeed * kJumpSpeed / (2.0 * kGravity) - kJumpSpeed * kStep * 0.5, 1.0);
	}
	// Releasing early gives a short hop
	{
		FSim Sim;
		Sim.Load(Flat);
		double MinY = 1e9;
		for (int32 I = 0; I < 120; ++I)
		{
			FInput In;
			In.Jump = I < 6;
			Sim.Step(In);
			MinY = FMath::Min(MinY, Sim.P.Y);
		}
		TestTrue(TEXT("short hop is lower"), 300.0 - MinY < 50.0);
	}
	// Coyote time: jump pressed 0.06 s after running off a ledge still jumps
	{
		FLevelDef Ledge = Flat;
		Ledge.Ground = { { -1000, 100, 300 } };
		FSim Sim;
		Sim.Load(Ledge);
		bool bLeft = false;
		int32 AirSteps = 0;
		bool bJumped = false;
		for (int32 I = 0; I < 240 && !bJumped; ++I)
		{
			FInput In;
			In.Dir = 1;
			if (!Sim.P.Grounded && !bLeft) { bLeft = true; }
			if (bLeft) { ++AirSteps; }
			In.Jump = bLeft && AirSteps >= 7;
			Sim.Events.clear();
			Sim.Step(In);
			for (const HL::FEvent& E : Sim.Events) { bJumped = bJumped || E.Type == EEvent::Jump; }
		}
		TestTrue(TEXT("coyote jump"), bJumped);
	}
	return true;
}

#endif
