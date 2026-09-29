// EMBERHOME core: level definitions (data only). (CLAUDE.md: Core / levels)
#pragma once

#include "HLTypes.h"

#include <string>
#include <vector>

namespace HL
{
	struct FGroundDef { double X0, X1, Top; };                 // solid from Top down to the bottom of the world
	struct FBlockDef { double X0, Y0, X1, Y1; };               // free-standing solid (stone, stump, overhang)
	struct FCrateDef { double X; double Bottom; };             // X = left edge
	struct FTrapDef { double X; };                             // centre; rests on the surface below it
	struct FLogDef { double X, PivotY, Rope, Amp, Period, Phase; };
	// Moving bough: its top-centre travels A <-> B with an eased cosine over Period seconds.
	struct FPlatformDef { double AX, AY, BX, BY, Width, Period, Phase; };
	struct FCrumbleDef { double X0, X1, Top; };                // rotten branch: gives way shortly after you stand on it
	struct FWaterDef { double X0, X1, Surface; };              // deadly to the child; crates float

	// Look of a level. Everything stays monochrome: these only shift greys, density and weather.
	struct FTheme
	{
		double Brightness = 1.0;     // multiplies every grey of the sky and tree layers
		double Fog = 1.0;            // fog band strength
		double Rain = 1.0;           // rain density (1 = ~170 streaks)
		double RainSlant = 0.28;     // horizontal drift per unit fall
		double Darkness = 0.55;      // lantern overlay darkness away from the child
		double LightRadius = 1.0;    // lantern reach
		double Shafts = 1.0;         // light shaft strength
		double Lightning = 0.0;      // chance of a flash per second
		double Wind = 0.0;           // extra scarf flutter and rain slant
		double Dawn = 0.0;           // 0..1 how far the sky has warmed at the goal (last level)
		double ForegroundDensity = 1.0;
		uint32_t Seed = 1;
	};

	struct FLevelDef
	{
		std::string Name;
		std::string Subtitle;
		double StartX = 60;
		double GoalX = 4700;
		double MinX = -800, MaxX = 5500;
		std::vector<FGroundDef> Ground;
		std::vector<FBlockDef> Blocks;
		std::vector<FCrateDef> Crates;
		std::vector<FTrapDef> Traps;
		std::vector<FLogDef> Logs;
		std::vector<FPlatformDef> Platforms;
		std::vector<FCrumbleDef> Crumbles;
		std::vector<FWaterDef> Water;
		std::vector<double> Checkpoints;
		FTheme Theme;
	};

	// The ten levels, in play order.
	const std::vector<FLevelDef>& GetLevels();
}
