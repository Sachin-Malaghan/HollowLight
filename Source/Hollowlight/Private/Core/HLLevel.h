// EMBERHOME core: level definitions (data only). (CLAUDE.md: Core / levels)
#pragma once

#include "HLTypes.h"

#include <string>
#include <vector>

namespace HL
{
	// Solid from Top down to the bottom of the world. bStep marks the thin stair-steps a slope is built
	// from: they collide like any ground but are drawn as one smooth hillside (see FSlopeDef).
	struct FGroundDef { double X0, X1, Top; bool bStep = false; };
	struct FBlockDef { double X0, Y0, X1, Y1; };               // free-standing solid (stone, stump, overhang)
	struct FCrateDef { double X; double Bottom; };             // X = left edge
	struct FTrapDef { double X; };                             // centre; rests on the surface below it
	struct FLogDef { double X, PivotY, Rope, Amp, Period, Phase; };
	// Moving bough: its top-centre travels A <-> B with an eased cosine over Period seconds.
	struct FPlatformDef { double AX, AY, BX, BY, Width, Period, Phase; };
	struct FCrumbleDef { double X0, X1, Top; };                // rotten branch: gives way shortly after you stand on it
	struct FWaterDef { double X0, X1, Surface; };              // deadly to the child; crates float

	// --- 2.0: terrain, things to operate, the dog -------------------------------------------------
	struct FSlopeDef { double X0, Y0, X1, Y1; };               // drawn hillside; collision is the bStep grounds under it
	struct FLadderDef { double X, Top, Bottom; };              // hold jump to climb, down to descend; Top is a surface
	// A door: solid while shut, slides up when open. With bBridge it is a drawbridge deck instead:
	// raised (and not solid) until opened, then it lowers and can be walked on.
	struct FGateDef { double X0, Y0, X1, Y1; bool bStartOpen = false; bool bBridge = false; };
	struct FLeverDef { double X; int Gate; double Y = kGroundY; };                  // ACT (or the dog) throws it: latches the gate open / shut
	struct FPlateDef { double X0, X1; int Gate; double Y = kGroundY; };             // gate is open while the child, a crate or the dog stands on it
	enum class EItem : uint8_t { Handle, Crowbar };
	struct FItemDef { double X; EItem Kind; double Y = kGroundY; };                 // a tool lying on the ground: ACT picks it up
	struct FSocketDef { double X; EItem Kind; int Gate; double Y = kGroundY; };     // ACT here with the right tool: opens the gate for good
	// When the child whistles (ACT with nothing else in reach) inside [X0, X1], the dog goes and throws
	// the lever - by a way only it can fit through.
	struct FDogTaskDef { double X0, X1; int Lever; };
	// If the child dithers inside [X0, X1] while gate UntilGate is still shut, the dog runs to PointX and
	// barks at it: a nudge toward the next step, never the whole answer.
	struct FHintDef { double X0, X1, PointX; int UntilGate; };

	// One step of the solution the autopilot follows on puzzle levels (and in the attract mode). It is
	// what proves a level can be solved; without a script the autopilot just heads for the lamp post.
	enum class EStepKind : uint8_t { Go, Act, Wait, WaitGate, WaitDogStay, Climb, CallDog };
	// Go: V = x (and Y = the floor it is on, when that matters)   Wait: V = seconds   WaitGate: V = gate index
	// Climb: up the ladder the child is standing at   CallDog: whistle the dog back to heel if it is staying
	struct FSolveStep { EStepKind Kind; double V = 0; double Y = -1.0e9; };

	enum class ESetting : uint8_t { Forest, Warehouse, Railway, Station };

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
		ESetting Setting = ESetting::Forest;   // what stands behind the play layer
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
		std::vector<FSlopeDef> Slopes;
		std::vector<FLadderDef> Ladders;
		std::vector<FGateDef> Gates;
		std::vector<FLeverDef> Levers;
		std::vector<FPlateDef> Plates;
		std::vector<FItemDef> Items;
		std::vector<FSocketDef> Sockets;
		std::vector<FDogTaskDef> DogTasks;
		std::vector<FHintDef> Hints;
		std::vector<FSolveStep> Solution;
		bool bDog = false;           // the dog comes along on this level
		FTheme Theme;
	};

	// The ten levels, in play order.
	const std::vector<FLevelDef>& GetLevels();
}
