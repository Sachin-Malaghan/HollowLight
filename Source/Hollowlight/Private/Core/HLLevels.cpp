// EMBERHOME core: the levels. (CLAUDE.md: Levels)
// Units: the screen shows 400 units of height, ground top at y = 300, y grows downward.
// Reach: a full jump rises ~102 and carries ~160 at a run (~198 at a sprint); the hands catch a ledge
// up to ~160 above the ground; a slide fits under anything 22 or more above the ground; the dog fits
// under 16. A crate top is 56 up. Water surface 339 puts a floating crate's top flush with ground at 300.
// Every level is verified by the autopilot (Tools/SimHarness, test Hollowlight.Levels.AutopilotFinishes),
// puzzle levels by following their Solution script. Checkpoints must be clean cuts: nothing behind one
// may be needed to finish from it.
#include "HLLevel.h"

#include <cmath>

namespace HL
{
	namespace
	{
		// A hillside from (X0, Y0) to (X1, Y1): thin stair-steps the child (and the dog) walk up without
		// noticing, drawn as one smooth slope. Keep it gentler than ~0.8 so each step is under 6 high.
		void AddSlope(FLevelDef& L, double X0, double Y0, double X1, double Y1)
		{
			const double StepW = 6.0;
			const int N = (int)std::ceil((X1 - X0) / StepW);
			for (int I = 0; I < N; ++I)
			{
				const double A = X0 + (X1 - X0) * I / N, B = X0 + (X1 - X0) * (I + 1) / N;
				const double Top = Y0 + (Y1 - Y0) * (I + 0.5) / N;
				FGroundDef G{ A, B, Top };
				G.bStep = true;
				L.Ground.push_back(G);
			}
			L.Slopes.push_back({ X0, Y0, X1, Y1 });
		}

			// Scree: a slope too steep and loose to stand on. The child slides down it (see FGroundDef::Slide).
		void AddScree(FLevelDef& L, double X0, double Y0, double X1, double Y1)
		{
			const size_t First = L.Ground.size();
			AddSlope(L, X0, Y0, X1, Y1);
			for (size_t I = First; I < L.Ground.size(); ++I) { L.Ground[I].Slide = Y1 > Y0 ? 1 : -1; }
			L.Slopes.back().bScree = true;
		}

		FSolveStep Go(double X) { return { EStepKind::Go, X }; }
		FSolveStep GoOn(double X, double FloorY) { return { EStepKind::Go, X, FloorY }; }
		FSolveStep Act() { return { EStepKind::Act }; }
		FSolveStep WaitGate(int Gate) { return { EStepKind::WaitGate, (double)Gate }; }
		FSolveStep WaitDogStay() { return { EStepKind::WaitDogStay }; }
		FSolveStep Climb() { return { EStepKind::Climb }; }
		FSolveStep CallDog() { return { EStepKind::CallDog }; }

		std::vector<FLevelDef> BuildLevels()
		{
			std::vector<FLevelDef> Levels;

			// 1 ------------------------------------------------------------------------------------
			// The original level: pits, a crate to climb a ledge, jaw traps, one swinging log.
			{
				FLevelDef L;
				L.Name = "The Edge of the Wood";
				L.Subtitle = "Bring the light home.";
				L.Notes = { { -600, 500, "Carry the lantern to the lamp post\nat the far edge of the wood." },
				            { 1050, 1445, "The ledge is too high to jump.\nPush the crate up to it." } };
				L.StartX = 60;
				L.GoalX = 4700;
				L.MinX = -800;
				L.MaxX = 5500;
				L.Ground = { { -800, 600, 300 }, { 690, 1450, 300 }, { 1450, 2350, 220 }, { 2350, 2850, 300 },
				             { 2960, 4200, 300 }, { 4300, 5500, 300 } };
				L.Blocks = { { 5440, -400, 5500, 300 } };  // wall at the end
				L.Crates = { { 1222, 300 } };
				L.Traps = { { 950 }, { 2600 }, { 4450 } };
				L.Logs = { { 3700, 50, 212, 0.95, 2.6, 0.0 } };
				L.Checkpoints = { 160, 740, 1500, 2400, 3000, 3450, 4320 };
				L.Theme.Seed = 11;
				Levels.push_back(L);
			}

			// 2 ------------------------------------------------------------------------------------
			// Hills, and a stray dog. It points out jaws in the grass, and it fits where the child cannot:
			// a lever to throw yourself, then one only the dog can reach.
			{
				FLevelDef L;
				L.Name = "A Stray";
				L.Subtitle = "Not alone any more.";
				L.GoalX = 4000;
				L.MinX = -600;
				L.MaxX = 4300;
				L.bDog = true;
				L.Notes = { { -600, 450, "A stray has decided to come along.\nWHISTLE: it stays. WHISTLE again: it comes." },
				            { 1900, 2295, "A lever works this gate.\nStand by it and press PULL.", 0 },
				            { 2400, 2895, "The lever is on the far side,\nand the gap is dog-sized. WHISTLE.", 1 } };
				L.Ground = { { -600, 500, 300 }, { 800, 1200, 220 }, { 1500, 3400, 300 }, { 3520, 4300, 300 } };
				AddSlope(L, 500, 300, 800, 220);
				AddSlope(L, 1200, 220, 1500, 300);
				L.Blocks = { { 4240, -400, 4300, 300 } };
				L.Traps = { { 300 }, { 1000 }, { 1750 } };
				L.Gates = { { 2300, 120, 2330, 300 },      // 0: log gate, opened by the lever in front of it
				            { 2900, 60, 2960, 282 } };     // 1: heavy gate with a gap only the dog fits under
				L.Levers = { { 2180, 0 }, { 3040, 1 } };
				L.DogTasks = { { 2560, 2895, 1 } };
				L.Hints = { { 1900, 2290, 2180, 0 }, { 2400, 2895, 2880, 1 } };
				L.Checkpoints = { 150, 820, 1520, 2340, 2980, 3540 };
				L.Solution = { Go(2180), Act(), WaitGate(0), Go(2700), Act(), WaitGate(1) };
				L.Theme.Seed = 23;
				L.Theme.Brightness = 1.05;
				L.Theme.Fog = 0.9;
				L.Theme.Rain = 0.7;
				Levels.push_back(L);
			}

			// 3 ------------------------------------------------------------------------------------
			// Jaw traps everywhere; a field of three too wide to jump - push the crate to spring them.
			{
				FLevelDef L;
				L.Name = "Teeth in the Grass";
				L.Subtitle = "Let something else step first.";
				L.GoalX = 5100;
				L.MinX = -600;
				L.MaxX = 5400;
				L.bDog = true;
				L.Notes = { { 1000, 1480, "Three jaws, too wide to jump.\nLet the crate step on them first." } };
				L.Ground = { { -600, 900, 300 }, { 1000, 2200, 300 }, { 2320, 3000, 300 }, { 3000, 3800, 230 },
				             { 3800, 4400, 300 }, { 4520, 5400, 300 } };
				L.Blocks = { { 5340, -400, 5400, 300 } };
				L.Crates = { { 1200, 300 } };
				L.Traps = { { 400 }, { 650 }, { 750 }, { 1500 }, { 1560 }, { 1620 }, { 2600 }, { 3200 }, { 3450 }, { 4100 }, { 4800 } };
				L.Checkpoints = { 150, 1020, 2340, 3020, 3820, 4540 };
				L.Theme.Seed = 37;
				L.Theme.Fog = 1.2;
				L.Theme.Darkness = 0.6;
				Levels.push_back(L);
			}

			// 4 ------------------------------------------------------------------------------------
			// Over a ridge: slopes, a cliff with a ladder, and a winch handle to carry down to a drawbridge.
			{
				FLevelDef L;
				L.Name = "Over the Ridge";
				L.Subtitle = "Carry what the bridge needs.";
				L.GoalX = 4300;
				L.MinX = -600;
				L.MaxX = 4600;
				L.bDog = true;
				L.Notes = { { 1450, 1700, "A ladder. Hold JUMP to climb,\nSLIDE to come down." },
				            { 2050, 2450, "A winch handle, lying in the grass.\nStand over it and press TAKE.", 0 },
				            { 2900, 3300, "The bridge winch has lost its handle.\nBring it here and press USE.", 0 } };
				L.Ground = { { -600, 400, 300 }, { 700, 1000, 180 }, { 1000, 1700, 300 }, { 1700, 2500, 130 },
				             { 2500, 3300, 300 }, { 3620, 3800, 300 }, { 4100, 4600, 200 } };
				AddSlope(L, 400, 300, 700, 180);
				AddSlope(L, 3800, 300, 4100, 200);
				L.Blocks = { { 4540, -400, 4600, 200 } };
				L.Ladders = { { 1688, 130, 300 } };
				L.Traps = { { 1350 }, { 2000 } };
				L.Water = { { 3300, 3620, 339 } };
				L.Gates = { { 3300, 300, 3620, 312, false, true } };   // 0: the drawbridge
				L.Items = { { 2250, EItem::Handle, 130 } };
				L.Sockets = { { 3240, EItem::Handle, 0 } };
				L.Hints = { { 2050, 2450, 2250, 0 }, { 2900, 3300, 3240, 0 } };
				L.Checkpoints = { 150, 720, 1020, 1710, 3640 };
				L.Solution = { Go(1688), Climb(), GoOn(2250, 130), Act(), Go(3240), Act(), WaitGate(0) };
				L.Theme.Seed = 43;
				L.Theme.Brightness = 0.95;
				L.Theme.Shafts = 1.4;
				L.Theme.Wind = 0.4;
				Levels.push_back(L);
			}

			// 5 ------------------------------------------------------------------------------------
			// Black water too wide to jump: push a crate in and it floats.
			{
				FLevelDef L;
				L.Name = "Still Water";
				L.Subtitle = "What sinks the child floats the box.";
				L.GoalX = 5450;
				L.MinX = -600;
				L.MaxX = 5700;
				L.bDog = true;
				L.Notes = { { 300, 800, "The water is deep and the child cannot swim.\nA crate floats." } };
				L.Ground = { { -600, 800, 300 }, { 1010, 1900, 300 }, { 2110, 2900, 300 }, { 2900, 3600, 190 },
				             { 3600, 4200, 300 }, { 4400, 5700, 300 } };
				L.Blocks = { { 5640, -400, 5700, 300 } };
				L.Crates = { { 500, 300 }, { 1450, 300 }, { 2500, 300 }, { 3900, 300 } };
				L.Traps = { { 1300 }, { 1700 }, { 5150 } };
				L.Logs = { { 4800, 50, 212, 0.95, 2.6, 0.0 } };
				L.Water = { { 800, 1010, 339 }, { 1900, 2110, 339 }, { 4200, 4400, 339 } };
				L.Checkpoints = { 150, 1030, 2130, 2920, 3620, 4420 };
				L.Theme.Seed = 79;
				L.Theme.Fog = 1.6;
				L.Theme.Rain = 0.5;
				Levels.push_back(L);
			}

			// 6 ------------------------------------------------------------------------------------
			// A warehouse. A door that only stays open while something stands on its plate; a crowbar
			// up on the racking; a second door that wants a crate.
			{
				FLevelDef L;
				L.Name = "The Warehouse";
				L.Subtitle = "Something has to hold the door.";
				L.GoalX = 3900;
				L.MinX = -400;
				L.MaxX = 4200;
				L.bDog = true;
				L.bGhost = true;
				L.Notes = { { 300, 895, "The door stays open only while\nsomething stands on the plate.", 0 },
				            { 1000, 2595, "This door is jammed.\nThere is a crowbar up on the racking.", 1 },
				            { 2640, 3395, "The dog cannot stay here for ever.\nSomething heavy could.", 2 } };
				L.Ground = { { -400, 4200, 300 } };
				L.Blocks = { { 1500, 120, 1900, 136 },     // racking: reached by the ladder at its near end
				             { 3208, 288, 3220, 300 },     // a kerb: the crate stops here, on the plate
				             { 4140, -400, 4200, 300 } };
				L.Ladders = { { 1486, 120, 300 } };
				L.Crates = { { 2850, 300 } };
				L.Gates = { { 900, 100, 930, 300 },        // 0: sliding door held by plate 0
				            { 2600, 100, 2630, 300 },      // 1: jammed door, wants the crowbar
				            { 3400, 100, 3430, 300 } };    // 2: sliding door held by plate 1
				L.Plates = { { 600, 660, 0 }, { 3150, 3210, 2 } };
				L.Items = { { 1700, EItem::Crowbar, 120 } };
				L.Sockets = { { 2560, EItem::Crowbar, 1 } };
				L.Hints = { { 300, 895, 630, 0 }, { 1000, 2595, 1486, 1 }, { 2640, 3395, 3180, 2 } };
				L.Checkpoints = { 150, 960, 1300, 2660, 3450 };
				L.Solution = { Go(630), Act(), WaitDogStay(), Go(1000), CallDog(), Go(1486), Climb(), GoOn(1700, 120), Act(),
				               Go(2000), Go(2560), Act(), WaitGate(1), Go(3143) };
				L.Theme.Seed = 61;
				L.Theme.Setting = ESetting::Warehouse;
				L.Theme.Rain = 0.0;
				L.Theme.Fog = 0.6;
				L.Theme.Brightness = 0.8;
				L.Theme.Darkness = 0.68;
				L.Theme.Shafts = 1.8;
				L.Theme.ForegroundDensity = 0.0;
				Levels.push_back(L);
			}

			// 7 ------------------------------------------------------------------------------------
			// A railway yard at night: slide under a wagon, climb over another, send the dog into the
			// signal hut, and fetch the handle that swings the bridge over the inspection pit.
			{
				FLevelDef L;
				L.Name = "Sidings";
				L.Subtitle = "The points are set against you.";
				L.GoalX = 4200;
				L.MinX = -600;
				L.MaxX = 4600;
				L.bDog = true;
				L.bGhost = true;
				L.Notes = { { 300, 900, "Run and press SLIDE to go under the wagon." },
				            { 1950, 2595, "The barrier lever is inside the hut.\nOnly the dog fits. WHISTLE.", 0 },
				            { 2640, 3295, "The swing bridge needs its handle.\nTry the flat wagon.", 1 } };
				L.Ground = { { -600, 3300, 300 }, { 3640, 4600, 300 } };
				L.Blocks = { { 500, 150, 900, 276 },       // box wagon on its wheels: slide under, or climb over
				             { 1500, 100, 1900, 300 },     // loaded wagon: too tall, take the ladder
				             { 2300, 150, 2320, 282 },     // signal hut: near wall, with a gap only the dog fits under
				             { 2300, 150, 2460, 166 },     //   roof
				             { 2440, 150, 2460, 300 },     //   far wall
				             { 2800, 130, 3100, 300 },     // flat wagon carrying the bridge handle
				             { 4540, -400, 4600, 300 } };
				L.Ladders = { { 1486, 100, 300 }, { 2786, 130, 300 } };
				L.Traps = { { 1150 } };
				L.Gates = { { 2600, 120, 2630, 300 },                      // 0: crossing barrier, lever in the hut
				            { 3300, 300, 3640, 312, false, true } };       // 1: swing bridge over the pit
				L.Levers = { { 2380, 0 } };
				L.DogTasks = { { 1950, 2295, 0 } };
				L.Items = { { 2950, EItem::Handle, 130 } };
				L.Sockets = { { 3250, EItem::Handle, 1 } };
				L.Hints = { { 1950, 2295, 2290, 0 }, { 2640, 3295, 2786, 1 } };
				L.Checkpoints = { 150, 920, 1960, 2660, 3660 };
				L.Solution = { Go(1486), Climb(), Go(2100), Act(), WaitGate(0), Go(2786), Climb(), GoOn(2950, 130), Act(),
				               Go(3250), Act(), WaitGate(1) };
				L.Theme.Seed = 71;
				L.Theme.Setting = ESetting::Railway;
				L.Theme.Brightness = 0.82;
				L.Theme.Rain = 1.3;
				L.Theme.Wind = 0.5;
				L.Theme.Darkness = 0.66;
				L.Theme.ForegroundDensity = 0.3;
				Levels.push_back(L);
			}

			// 8 ------------------------------------------------------------------------------------
			// A station. Luggage on one plate and the dog on another to get through the barriers, a
			// footbridge over the tracks, and a crowbar for the gate to the far platform's ladder.
			{
				FLevelDef L;
				L.Name = "The Station";
				L.Subtitle = "Two barriers, one of you.";
				L.GoalX = 3700;
				L.MinX = -600;
				L.MaxX = 4200;
				L.bDog = true;
				L.bGhost = true;
				L.Notes = { { 620, 1195, "Luggage is heavy enough to hold a plate.", 0 },
				            { 1235, 1495, "One more plate, and nothing left to push.\nWHISTLE: the dog can stay.", 1 },
				            { 2620, 2935, "A locked gate, and somewhere near, a crowbar.", 2 } };
				L.Ground = { { -600, 600, 300 }, { 600, 1700, 240 }, { 1900, 2300, 120 }, { 2500, 2600, 240 },
				             { 2600, 3000, 300 }, { 3000, 4200, 130 } };
				AddSlope(L, 1700, 240, 1900, 120);
				AddSlope(L, 2300, 120, 2500, 240);
				L.Blocks = { { 1010, 228, 1022, 240 },     // kerb: the luggage stops here, on the plate
				             { 4140, -400, 4200, 130 } };
				L.Ladders = { { 2986, 130, 300 } };
				L.Crates = { { 800, 240 } };
				L.Gates = { { 1200, 60, 1230, 240 },       // 0: first barrier, plate 0 (luggage)
				            { 1500, 60, 1530, 240 },       // 1: second barrier, plate 1 (the dog)
				            { 2940, 100, 2960, 300 } };    // 2: gate to the ladder, wants the crowbar
				L.Plates = { { 950, 1010, 0, 240 }, { 1300, 1360, 1, 240 } };
				L.Items = { { 2750, EItem::Crowbar, 300 } };
				L.Sockets = { { 2900, EItem::Crowbar, 2, 300 } };
				L.Hints = { { 620, 1195, 980, 0 }, { 1235, 1495, 1330, 1 }, { 2620, 2935, 2750, 2 } };
				L.Checkpoints = { 150, 620, 1560, 2620, 3020 };
				L.Solution = { Go(945), WaitGate(0), Go(1330), Act(), WaitDogStay(), Go(1600), CallDog(), Go(2750), Act(),
				               Go(2900), Act(), WaitGate(2), Go(2986), Climb() };
				L.Theme.Seed = 83;
				L.Theme.Setting = ESetting::Station;
				L.Theme.Rain = 0.9;
				L.Theme.Fog = 1.2;
				L.Theme.ForegroundDensity = 0.2;
				Levels.push_back(L);
			}

			// 9 ------------------------------------------------------------------------------------
			// A mountain. Rock terraces to climb hand over hand, then scree: sit back and slide, low
			// under a fallen trunk at the foot of the first slope, and jump the chasm that splits the second.
			{
				FLevelDef L;
				L.Name = "The Mountain";
				L.Subtitle = "Up is earned. Down is quick.";
				L.GoalX = 5250;
				L.MinX = -600;
				L.MaxX = 5500;
				L.bDog = true;
				L.Notes = { { 150, 700, "Jump at a rock face: the child catches\nthe ledge and pulls up." },
				            { 1020, 1500, "Scree. Nobody can stand on it:\nyou will slide. JUMP still works." },
				            { 3470, 3800, "A chasm splits the slope ahead.\nJUMP before the edge." } };
				L.Ground = { { -600, 400, 300 }, { 400, 700, 190 }, { 700, 1000, 80 }, { 1000, 1500, -100 },
				             { 2000, 2700, 300 }, { 2700, 2950, 190 }, { 2950, 3200, 80 }, { 3200, 3450, -30 },
				             { 3450, 3800, -140 }, { 4500, 5500, 300 } };
				AddScree(L, 1500, -100, 2000, 300);
				AddScree(L, 3800, -140, 4100, 85);
				AddScree(L, 4220, 130, 4500, 300);
				L.Blocks = { { 2030, 238, 2210, 274 },     // fallen trunk at the foot of the scree: the slide carries you under
				             { 5440, -400, 5500, 300 } };
				L.Ladders = { { 988, -100, 80 } };
				L.Traps = { { 1250 }, { 2450 } };
				L.Checkpoints = { 150, 420, 1020, 2240, 2720, 3470, 4560 };
				L.Theme.Seed = 89;
				L.Theme.Setting = ESetting::Mountain;
				L.Theme.Fog = 1.3;
				L.Theme.Rain = 0.6;
				L.Theme.Wind = 0.7;
				L.Theme.Shafts = 0.6;
				L.Theme.ForegroundDensity = 0.4;
				Levels.push_back(L);
			}

			// 10 -----------------------------------------------------------------------------------
			// Everything at once, in a storm.
			{
				FLevelDef L;
				L.Name = "The Storm";
				L.Subtitle = "Hold the light close.";
				L.GoalX = 6200;
				L.MinX = -600;
				L.MaxX = 6400;
				L.bDog = true;
				L.bGhost = true;
				L.Ground = { { -600, 700, 300 }, { 820, 1500, 300 }, { 1500, 2100, 186 }, { 2300, 3100, 300 },
				             { 3310, 3900, 300 }, { 4400, 4900, 300 }, { 5400, 6400, 300 } };
				L.Blocks = { { 6340, -400, 6400, 300 } };
				L.Crates = { { 1000, 300 }, { 2980, 300 } };
				L.Traps = { { 400 }, { 1800 }, { 4600 }, { 6050 } };
				L.Logs = { { 560, 50, 212, 0.9, 2.5, 0.0 }, { 2550, 50, 212, 0.9, 2.4, 0.0 },
				           { 5800, 50, 212, 0.95, 2.6, 1.2 } };
				L.Platforms = { { 4990, 290, 5310, 290, 80, 2.6, 0.0 } };
				L.Crumbles = { { 3980, 4080, 285 }, { 4160, 4260, 280 } };
				L.Water = { { 3100, 3310, 339 } };
				L.Checkpoints = { 150, 840, 1520, 2320, 3330, 4420, 5420 };
				L.Theme.Seed = 97;
				L.Theme.Brightness = 0.78;
				L.Theme.Rain = 1.8;
				L.Theme.Lightning = 0.25;
				L.Theme.Wind = 1.0;
				L.Theme.Darkness = 0.68;
				Levels.push_back(L);
			}

			// 11 -----------------------------------------------------------------------------------
			// The way home: the rain thins, the sky warms, a lamp post at the edge of the village.
			{
				FLevelDef L;
				L.Name = "Homecoming";
				L.Subtitle = "Someone left a lamp unlit.";
				L.GoalX = 6700;
				L.MinX = -600;
				L.MaxX = 7000;
				L.bDog = true;
				L.Ground = { { -600, 800, 300 }, { 920, 1700, 300 }, { 1700, 2400, 190 }, { 2400, 3200, 300 },
				             { 3410, 4000, 300 }, { 4500, 5100, 300 }, { 5500, 7000, 300 } };
				L.Blocks = { { 6940, -400, 7000, 300 } };
				L.Crates = { { 1400, 300 }, { 3050, 300 } };
				L.Traps = { { 1200 }, { 5800 } };
				L.Logs = { { 2800, 50, 212, 0.9, 2.6, 0.0 }, { 6200, 50, 212, 0.95, 2.8, 0.5 } };
				L.Platforms = { { 4090, 290, 4410, 290, 80, 3.0, 0.0 } };
				L.Crumbles = { { 5180, 5280, 285 }, { 5360, 5460, 285 } };
				L.Water = { { 3200, 3410, 339 } };
				L.Checkpoints = { 150, 940, 1720, 2420, 3430, 4520, 5520, 6300 };
				L.Theme.Seed = 101;
				L.Theme.Rain = 0.4;
				L.Theme.Fog = 1.1;
				L.Theme.Dawn = 1.0;
				Levels.push_back(L);
			}

			return Levels;
		}
	}

	const std::vector<FLevelDef>& GetLevels()
	{
		static const std::vector<FLevelDef> Levels = BuildLevels();
		return Levels;
	}
}
