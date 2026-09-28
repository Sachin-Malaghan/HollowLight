// HOLLOWLIGHT core: the ten levels. (CLAUDE.md: Levels)
// Units: the screen shows 400 units of height, ground top at y = 300, y grows downward.
// Reach at full run: a full jump rises ~104 and carries ~179 horizontally.
// Every level is verified by the autopilot (Tools/SimHarness, test Hollowlight.Levels.AutopilotFinishes).
#include "HLLevel.h"

namespace HL
{
	namespace
	{
		std::vector<FLevelDef> BuildLevels()
		{
			std::vector<FLevelDef> Levels;

			// 1 ------------------------------------------------------------------------------------
			// The original level: pits, a crate to climb a ledge, jaw traps, one swinging log.
			{
				FLevelDef L;
				L.Name = "The Edge of the Wood";
				L.Subtitle = "Bring the light home.";
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
			// Stepping stones over a gully, a stump too tall to jump without the crate, a first pond.
			{
				FLevelDef L;
				L.Name = "Stepping Stones";
				L.Subtitle = "The stones remember every foot.";
				L.GoalX = 4650;
				L.MinX = -600;
				L.MaxX = 5000;
				L.Ground = { { -600, 520, 300 }, { 1180, 1900, 300 }, { 1900, 2700, 186 }, { 2700, 3400, 300 },
				             { 3530, 4150, 300 }, { 4250, 5000, 300 } };
				L.Blocks = { { 640, 280, 700, 480 }, { 820, 265, 880, 480 }, { 1000, 285, 1060, 480 }, { 4940, -400, 5000, 300 } };
				L.Crates = { { 1450, 300 } };
				L.Traps = { { 3000 }, { 3800 } };
				L.Water = { { 3400, 3530, 332 } };
				L.Checkpoints = { 150, 1200, 1950, 2750, 3560, 4270 };
				L.Theme.Seed = 23;
				L.Theme.Brightness = 1.05;
				L.Theme.Fog = 0.8;
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
			// Logs hung all through the wood, two of them swinging against each other.
			{
				FLevelDef L;
				L.Name = "The Hanging Wood";
				L.Subtitle = "Listen for the rope.";
				L.GoalX = 5400;
				L.MinX = -600;
				L.MaxX = 5800;
				L.Ground = { { -600, 1000, 300 }, { 1110, 1900, 300 }, { 2010, 3200, 300 }, { 3300, 4600, 300 },
				             { 4700, 5800, 300 } };
				L.Blocks = { { 5740, -400, 5800, 300 } };
				L.Traps = { { 4200 } };
				L.Logs = { { 600, 50, 212, 0.9, 2.4, 0.0 }, { 1400, 50, 212, 0.95, 2.6, 1.5 },
				           { 2350, 50, 212, 0.9, 2.2, 0.0 }, { 2750, 50, 212, 0.9, 2.2, kPi },
				           { 3800, 50, 212, 0.95, 2.8, 0.7 }, { 5050, 50, 212, 0.95, 3.0, 2.0 } };
				L.Checkpoints = { 150, 1130, 2030, 3320, 4720 };
				L.Theme.Seed = 41;
				L.Theme.Brightness = 0.92;
				L.Theme.Fog = 1.3;
				L.Theme.Shafts = 1.4;
				Levels.push_back(L);
			}

			// 5 ------------------------------------------------------------------------------------
			// Drifting boughs over wide gaps, and a lift up to a high shelf.
			{
				FLevelDef L;
				L.Name = "Drifting Boughs";
				L.Subtitle = "Wait for the wood to come to you.";
				L.GoalX = 5500;
				L.MinX = -600;
				L.MaxX = 6000;
				L.Ground = { { -600, 700, 300 }, { 1150, 1800, 300 }, { 2500, 3200, 300 }, { 3290, 4100, 150 },
				             { 4100, 4600, 300 }, { 5050, 6000, 300 } };
				L.Blocks = { { 5940, -400, 6000, 300 } };
				L.Traps = { { 1450 }, { 3700 } };
				L.Platforms = { { 790, 290, 1060, 290, 90, 3.2, 0.0 },
				                { 1880, 285, 2100, 285, 80, 2.8, 0.0 }, { 2220, 285, 2420, 285, 80, 2.8, 0.5 },
				                { 3250, 300, 3250, 150, 80, 3.4, 0.0 },
				                { 4690, 280, 4960, 280, 90, 3.0, 0.25 } };
				L.Checkpoints = { 150, 1170, 2520, 3310, 4120, 5070 };
				L.Theme.Seed = 53;
				L.Theme.Rain = 1.2;
				L.Theme.Wind = 0.5;
				Levels.push_back(L);
			}

			// 6 ------------------------------------------------------------------------------------
			// Rotten branches give way a breath after you land. Don't stop.
			{
				FLevelDef L;
				L.Name = "Rotten Branches";
				L.Subtitle = "Don't stand still.";
				L.GoalX = 5000;
				L.MinX = -600;
				L.MaxX = 5400;
				L.Ground = { { -600, 700, 300 }, { 1300, 2000, 300 }, { 2800, 3600, 300 }, { 4300, 5400, 300 } };
				L.Blocks = { { 5340, -400, 5400, 300 } };
				L.Traps = { { 1600 }, { 4700 } };
				L.Logs = { { 3200, 50, 212, 0.95, 2.6, 0.4 } };
				L.Platforms = { { 3690, 290, 3900, 290, 80, 2.6, 0.0 } };
				L.Crumbles = { { 780, 880, 290 }, { 960, 1060, 285 }, { 1140, 1240, 290 },
				               { 2080, 2180, 280 }, { 2260, 2360, 265 }, { 2440, 2540, 280 }, { 2620, 2720, 290 },
				               { 4000, 4100, 280 }, { 4180, 4260, 285 } };
				L.Checkpoints = { 150, 1320, 2820, 4320 };
				L.Theme.Seed = 67;
				L.Theme.Brightness = 0.85;
				L.Theme.Darkness = 0.65;
				L.Theme.LightRadius = 0.9;
				Levels.push_back(L);
			}

			// 7 ------------------------------------------------------------------------------------
			// Black water too wide to jump: push a crate in and it floats.
			{
				FLevelDef L;
				L.Name = "Still Water";
				L.Subtitle = "What sinks the child floats the box.";
				L.GoalX = 5450;
				L.MinX = -600;
				L.MaxX = 5700;
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

			// 8 ------------------------------------------------------------------------------------
			// Lifts up into the canopy, then down again over rotten branches.
			{
				FLevelDef L;
				L.Name = "Into the Canopy";
				L.Subtitle = "Up where the rain begins.";
				L.GoalX = 5300;
				L.MinX = -600;
				L.MaxX = 5600;
				L.Ground = { { -600, 900, 300 }, { 1000, 1600, 170 }, { 1680, 2200, 40 }, { 2820, 3600, 300 },
				             { 4100, 4600, 200 }, { 4600, 5600, 300 } };
				L.Blocks = { { 5540, -400, 5600, 300 } };
				L.Traps = { { 1300 }, { 5000 } };
				L.Logs = { { 3200, 50, 212, 0.95, 2.6, 1.0 } };
				L.Platforms = { { 960, 300, 960, 170, 80, 3.4, 0.0 }, { 1640, 170, 1640, 40, 80, 3.6, 0.0 },
				                { 3690, 290, 4010, 200, 90, 3.2, 0.0 } };
				L.Crumbles = { { 2280, 2380, 110 }, { 2460, 2560, 180 }, { 2640, 2740, 250 } };
				L.Checkpoints = { 150, 1020, 1700, 2840, 4120, 4620 };
				L.Theme.Seed = 89;
				L.Theme.Shafts = 1.6;
				L.Theme.Fog = 0.9;
				Levels.push_back(L);
			}

			// 9 ------------------------------------------------------------------------------------
			// Everything at once, in a storm.
			{
				FLevelDef L;
				L.Name = "The Storm";
				L.Subtitle = "Hold the light close.";
				L.GoalX = 6200;
				L.MinX = -600;
				L.MaxX = 6400;
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

			// 10 -----------------------------------------------------------------------------------
			// The way home: the rain thins, the sky warms, a lamp post at the edge of the village.
			{
				FLevelDef L;
				L.Name = "Homecoming";
				L.Subtitle = "Someone left a lamp unlit.";
				L.GoalX = 6700;
				L.MinX = -600;
				L.MaxX = 7000;
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
