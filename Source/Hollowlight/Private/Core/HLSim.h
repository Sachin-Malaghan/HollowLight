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
		bool Down = false;  // held: slide when running, crouch when not, let go of a ledge when hanging
		bool Act = false;   // held; the press picks up / uses a tool, throws a lever, or whistles for the dog
	};

	enum class EEvent : uint8_t
	{
		Jump, Land, Footstep, PushStart, TrapSnap, TrapSnapCrate, Death, Respawn,
		Checkpoint, Goal, CrateLand, CrateSplash, CrateReset, Splash, CrumbleCreak, CrumbleFall, LogSwoosh,
		Slide, Vault, Grab, Climb, Roll, HardLand,
		Lever, GateOpen, GateShut, PlateDown, PlateUp, Pickup, UseTool, Whistle, Bark, LadderStep, DogPoof,
		GhostAppear, GhostFlee
	};

	// What the body is doing, for the renderer.
	enum class EPose : uint8_t { Stand, Slide, Crouch, Vault, Hang, Climb, Roll, Stunned, Ladder };

	enum class EDeath : uint8_t { None, Pit, Trap, Log, Water, Ghost };

	// What the ACT button would do right now (it is labelled with this).
	enum class EActKind : uint8_t { Whistle, Take, Use, Lever };

	enum class EPhase : uint8_t { Playing, Dying, Won };

	enum class ESupport : uint8_t { None, Ground, Block, Crate, Platform, Crumble, Gate };

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

		// 2.0 movement
		double H = kPlayerH;          // current body height (kLowH while sliding / crouched)
		bool Low = false;             // sliding or crouched
		bool Sliding = false;
		double SlideTime = 0;
		int Scree = 0;                // sliding down a scree slope: its downhill direction
		double SprintTime = 0;        // seconds of unbroken running
		double VaultTimer = 0;        // > 0 just after a vault: keep the momentum
		int Hang = 0;                 // 0 no, 1 hanging from a ledge, 2 pulling up
		int HangDir = 1;
		double HangTime = 0;
		double HangLedgeY = 0;        // top of the ledge being held
		double HangFromX = 0, HangToX = 0;
		ESupport HangKind = ESupport::None;
		int HangIndex = -1;
		double GrabCooldown = 0;
		double RollTime = 0;
		double StunTime = 0;
		EPose Pose = EPose::Stand;

		// 2.0 interaction
		bool ActHeld = false;
		int Ladder = -1;              // index of the ladder being climbed
		double LadderPhase = 0;
		int Carry = -1;               // index of the tool in hand
		double IdleTime = 0;          // seconds without real progress (the dog's cue to hint)
		double IdleX = 0;

		FRect Box() const { return { X - kPlayerW * 0.5, Y - H, X + kPlayerW * 0.5, Y }; }
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

	struct FGate
	{
		bool Latched = false;          // thrown open by a lever or a tool
		bool Open = false;             // latched, or a plate is held down
		double Amount = 0;             // 0 shut .. 1 fully raised
	};

	struct FItem
	{
		double X = 0, Y = 0;
		bool Carried = false, Used = false;
	};

	enum class EDogMode : uint8_t { Follow, GoStay, Stay, TaskGo, Point };

	// The companion. It cannot be hurt and never triggers a trap; if it is left behind it catches up.
	struct FDog
	{
		bool Active = false;
		double X = 0, Y = 0, VX = 0, VY = 0;
		bool Grounded = false;
		int Facing = 1;
		EDogMode Mode = EDogMode::Follow;
		double TargetX = 0;
		int Task = -1;                 // lever it has been sent to throw
		int PointTrap = -1;            // trap it is pointing at
		double BarkTimer = 0;
		double BarkFlash = 0;          // > 0 just after a bark (renderer)
		double RunPhase = 0;
		double StuckTime = 0;
		double LostTime = 0;           // how long it has been unable to reach the child
		double SitTime = 0;

		static constexpr double W = 26.0, H = 16.0;
		FRect Box() const { return { X - W * 0.5, Y - H, X + W * 0.5, Y }; }
	};

	// Something pale that comes for the light when the child lingers (levels with bGhost). It drifts through
	// walls. Getting on with the level, or a whistle - the dog barks at it - sends it away.
	struct FGhost
	{
		int State = 0;                 // 0 not here, 1 coming, 2 fleeing
		double X = 0, Y = 0;
		double Alpha = 0;              // 0..1 how solid it looks
		double Phase = 0;
		int Side = 1;
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
		bool IsFree(const FRect& R, int IgnoreCrate = -1) const;   // no full solid overlaps R
		bool CanStand() const;
		FRect GateBox(int I) const;                               // where the gate is now (raised when open)
		bool GateSolid(int I) const { return Level->Gates[I].bBridge ? Gates[I].Amount > 0.9 : Gates[I].Amount < 0.55; }
		int LadderAt(double X, double FeetY) const;               // ladder the child at (X, FeetY) can hold, or -1
		EActKind ActKind(int* OutIndex = nullptr) const;          // what ACT would do now, and to which thing

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
		std::vector<FGate> Gates;
		std::vector<bool> LeverOn;
		std::vector<bool> PlateDown;
		std::vector<FItem> Items;
		std::vector<bool> SocketUsed;
		FDog Dog;
		FGhost Ghost;
		double Linger = 0;        // seconds since the child last got further (the ghost's cue)
		double ProgressX = 0;
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
		void UpdateHang(const FInput& In);
		void UpdateLadder(const FInput& In);
		void UpdateMechanisms();
		void DoAct();
		void ThrowLever(int Index, double X, double Y);
		void UpdateDog();
		void UpdateGhost();
		void PlaceDogNearPlayer();
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
