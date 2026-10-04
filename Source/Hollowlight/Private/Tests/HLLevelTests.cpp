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
	TestTrue(TEXT("at least eleven levels"), Levels.size() >= 11);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLParkour, "Hollowlight.Physics.SlideVaultLedgeGrab", HLTests::Flags)
bool FHLParkour::RunTest(const FString& Parameters)
{
	FLevelDef Base;
	Base.Ground = { { -1000, 3000, 300 } };
	Base.StartX = 0;
	Base.GoalX = 5000;
	Base.MinX = -1000;
	Base.MaxX = 5000;

	auto Run = [](const FLevelDef& L, double Seconds, const TFunction<FInput(const FSim&)>& Policy, TArray<EEvent>& OutEvents)
	{
		FSim Sim;
		Sim.Load(L);
		for (int32 I = 0; I < (int32)(Seconds / kStep); ++I)
		{
			const FInput In = Policy(Sim);
			Sim.Events.clear();
			Sim.Step(In);
			for (const HL::FEvent& E : Sim.Events) { OutEvents.Add(E.Type); }
		}
		return Sim;
	};

	// Sprint: unbroken running builds from 240 to 300.
	{
		TArray<EEvent> Ev;
		const FSim Sim = Run(Base, 2.5, [](const FSim&) { FInput In; In.Dir = 1; return In; }, Ev);
		TestNearlyEqual(TEXT("sprint speed"), Sim.P.VX, kSprintSpeed, 1.0);
	}
	// A beam 26 above the ground blocks a standing child but a slide passes under it.
	{
		FLevelDef L = Base;
		L.Blocks = { { 400, 60, 470, 274 } };
		TArray<EEvent> Ev;
		const FSim Stand = Run(L, 3.0, [](const FSim&) { FInput In; In.Dir = 1; return In; }, Ev);
		TestTrue(TEXT("standing is blocked by the beam"), Stand.P.X < 400.0);
		Ev.Reset();
		const FSim Slid = Run(L, 3.0, [](const FSim& S) { FInput In; In.Dir = 1; In.Down = S.P.X > 300.0 && S.P.X < 500.0; return In; }, Ev);
		TestTrue(TEXT("sliding passes under the beam"), Slid.P.X > 480.0);
		TestTrue(TEXT("slide event"), Ev.Contains(EEvent::Slide));
	}
	// A knee-high step is vaulted without stopping.
	{
		FLevelDef L = Base;
		L.Blocks = { { 400, 270, 460, 300 } };
		TArray<EEvent> Ev;
		const FSim Sim = Run(L, 3.0, [](const FSim&) { FInput In; In.Dir = 1; return In; }, Ev);
		TestTrue(TEXT("vaulted the step"), Sim.P.X > 480.0 && Ev.Contains(EEvent::Vault));
	}
	// A ledge too high to jump onto (140) is caught by the hands and climbed.
	{
		FLevelDef L = Base;
		L.Ground = { { -1000, 500, 300 }, { 500, 3000, 160 } };
		TArray<EEvent> Ev;
		const FSim Sim = Run(L, 4.0, [](const FSim& S) { FInput In; In.Dir = 1; In.Jump = S.P.X > 440.0 && (S.P.Grounded || S.P.VY < 0); return In; }, Ev);
		TestTrue(TEXT("grabbed the ledge"), Ev.Contains(EEvent::Grab));
		TestTrue(TEXT("climbed"), Ev.Contains(EEvent::Climb));
		TestTrue(TEXT("ended up on top of the ledge"), Sim.P.X > 520.0 && Sim.P.Y <= 160.5);   // the test's input keeps hopping
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLGhost, "Hollowlight.Ghost.ComesWhenYouLingerAndFleesTheDog", HLTests::Flags)
bool FHLGhost::RunTest(const FString& Parameters)
{
	const FLevelDef* Haunted = nullptr;
	for (const FLevelDef& L : GetLevels()) { if (L.bGhost && !Haunted) { Haunted = &L; } }
	TestNotNull(TEXT("some level has a ghost"), Haunted);
	if (!Haunted) { return true; }

	// Standing still: it comes, and it puts the light out.
	{
		FSim Sim;
		Sim.Load(*Haunted);
		bool bCame = false;
		for (int32 I = 0; I < (int32)(60.0 / kStep) && Sim.Deaths == 0; ++I)
		{
			Sim.Events.clear();
			Sim.Step(FInput());
			bCame = bCame || Sim.Ghost.State == 1;
		}
		TestTrue(TEXT("the ghost comes for a child who lingers"), bCame);
		TestTrue(TEXT("and takes the light"), Sim.Deaths == 1 && Sim.LastDeath == EDeath::Ghost);
	}
	// A whistle: the dog sees it off.
	{
		FSim Sim;
		Sim.Load(*Haunted);
		bool bFled = false;
		for (int32 I = 0; I < (int32)(40.0 / kStep); ++I)
		{
			FInput In;
			In.Act = Sim.Ghost.State == 1 && Sim.Ghost.Alpha > 0.5 && !Sim.P.ActHeld;
			Sim.Events.clear();
			Sim.Step(In);
			bFled = bFled || Sim.Ghost.State == 2;
		}
		TestTrue(TEXT("a whistle sends it away"), bFled);
		TestEqual(TEXT("and nobody is hurt"), Sim.Deaths, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHLEmbers, "Hollowlight.Embers.ThreePerLevelWithinReach", HLTests::Flags)
bool FHLEmbers::RunTest(const FString& Parameters)
{
	const std::vector<FLevelDef>& Levels = GetLevels();
	for (int32 L = 0; L < (int32)Levels.size(); ++L)
	{
		TestEqual(FString::Printf(TEXT("level %d has three embers"), L + 1), (int32)Levels[L].Embers.size(), 3);
		FSim Sim;
		Sim.Load(Levels[L]);
		for (const FEmberDef& E : Levels[L].Embers)
		{
			// Something to stand on (ground, a block, or water a crate can float in) within a jump below it,
			// and the ember itself in open air.
			bool bReach = false;
			for (double DX = -70.0; DX <= 70.0 && !bReach; DX += 10.0)
			{
				const double Surface = Sim.SurfaceAt(E.X + DX, E.Y - 5.0);
				bReach = Surface - E.Y > 0.0 && Surface - E.Y < 150.0;
			}
			for (const FWaterDef& W : Levels[L].Water) { bReach = bReach || (E.X > W.X0 && E.X < W.X1 && W.Surface - E.Y < 190.0); }
			for (const FPlatformDef& Pf : Levels[L].Platforms) { bReach = bReach || (E.X > Pf.AX - 60 && E.X < Pf.BX + 60 && Pf.AY - E.Y < 150.0); }
			TestTrue(FString::Printf(TEXT("level %d ember at (%.0f, %.0f) is within a jump of somewhere to stand"), L + 1, E.X, E.Y), bReach);
			TestTrue(FString::Printf(TEXT("level %d ember at (%.0f, %.0f) is not buried"), L + 1, E.X, E.Y), Sim.IsFree({ E.X - 4, E.Y - 4, E.X + 4, E.Y + 4 }));
		}
	}
	return true;
}

#endif
