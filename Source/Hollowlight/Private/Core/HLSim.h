// EMBERHOME core: deterministic fixed-step simulation of one level. (CLAUDE.md: Core / simulation)
// Copyable by value: the autopilot clones it to look ahead.
#pragma once

#include "HLLevel.h"

#include <vector>

namespace HL
{
	struct FInput
	{
		int Dir = 0;        // -1 left, 0, +1 right
		bool Jump = false;  // held
	};

	enum class EEvent : uint8_t
	{
		Jump, Land, Footstep, PushStart, TrapSnap, TrapSnapCrate, Death, Respawn,
		Checkpoint, Goal, CrateLand, CrateSplash, CrateReset, Splash, CrumbleCreak, CrumbleFall, LogSwoosh
	};

	enum class EDeath : uint8_t { None, Pit, Trap, Log, Water };

	enum class EPhase : uint8_t { Playing, Dying, Won };

	enum class ESupport : uint8_t { None, Ground, Block, Crate, Platform, Crumble };

	struct FEvent
	{
		EEvent Type;
		double X, Y;
		double Strength;
	};

	struct FPlayer
	{
		double X = 0, Y = 0;          // X = centre, Y = feet
		double VX = 0, VY = 0;
		bool Grounded = false;
		ESupport SupportKind = ESupport::None;
		int SupportIndex = -1;
		double Coyote = 0, JumpBuffer = 0;
		bool JumpHeld = false, JumpCut = true;
		int Facing = 1;
		double RunPhase = 0;          // radians, advances with distance walked
		double PushTimer = 0;         // > 0 while pushing a crate
		double StuckTime = 0;         // holding a direction but not moving
		double AirTime = 0;
		double LastFootstep = 0;

		FRect Box() const { return { X - kPlayerW * 0.5, Y - kPlayerH, X + kPlayerW * 0.5, Y }; }
	};

	struct FCrate
	{
		double X = 0, Y = 0;          // top-left
		double VY = 0;
		double PrevX = 0, PrevY = 0;
		double HomeX = 0, HomeY = 0;
		bool Grounded = false, InWater = false, Used = false;

		FRect Box() const { return { X, Y, X + kCrateSize, Y + kCrateSize }; }
	};

	struct FTrap
	{
		double X = 0, Y = 0;          // centre, surface it rests on
		bool Closed = false;
		bool ByCrate = false;
		double ClosedTime = 0;
	};

	struct FPlatform
	{
		double X = 0, Y = 0, PrevX = 0, PrevY = 0;  // top centre
	};

	struct FCrumble
	{
		int State = 0;                 // 0 solid, 1 creaking, 2 fallen
		double Timer = 0;
		double Drop = 0, DropV = 0;    // visual fall offset while fallen
	};

	class FSim
	{
	public:
		void Load(const FLevelDef& InLevel, int StartCheckpoint = -1);
		void Step(const FInput& In);

		// Geometry queries (also used by the renderer and autopilot).
		bool HasSupport(double X, double FeetY, double Up, double Down) const;
		double SurfaceAt(double X, double FromY) const;  // highest static surface top at X at or below FromY
		double LogAngle(int I) const;
		FRect LogBox(int I) const;
		FRect PlatformBox(int I) const;
		FRect CrumbleBox(int I) const;
		double SpawnX() const;

		const FLevelDef* Level = nullptr;
		double Time = 0;
		EPhase Phase = EPhase::Playing;
		double PhaseTime = 0;
		EDeath LastDeath = EDeath::None;
		FPlayer P;
		std::vector<FCrate> Crates;
		std::vector<FTrap> Traps;
		std::vector<FPlatform> Platforms;
		std::vector<FCrumble> Crumbles;
		int CheckpointIndex = -1;
		double Lantern = 1;       // 0 = out
		double Fade = 0;          // 0 = clear, 1 = black
		int Deaths = 0;
		double PlayTime = 0;      // time spent playing (not dying), for the results screen
		double MaxX = 0;
		std::vector<FEvent> Events;

		static constexpr double kDeathLanternOut = 0.35;
		static constexpr double kDeathFadeEnd = 1.0;
		static constexpr double kRespawnAt = 1.1;

	private:
		void Emit(EEvent Type, double X, double Y, double Strength = 1.0);
		void UpdateMovers();
		void UpdateCrates();
		void UpdatePlayer(const FInput& In);
		void MovePlayerX(double DX, bool bAllowPush);
		void MovePlayerY(double DY);
		double MoveCrateX(int Index, double DX);
		void MoveCrateY(int Index, double DY);
		void CheckHazards();
		void Kill(EDeath Cause);
		void Respawn();
		void ResetTransient();
		const FWaterDef* WaterAt(double X) const;
	};
}
