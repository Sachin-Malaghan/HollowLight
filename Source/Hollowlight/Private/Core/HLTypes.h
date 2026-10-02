// EMBERHOME core: engine-agnostic constants and small math. (CLAUDE.md: Core / simulation)
// Nothing under Private/Core may include Unreal headers; the same files build the standalone
// harness in Tools/SimHarness so levels and the autopilot can be verified outside the engine.
#pragma once

#include <cmath>
#include <cstdint>

namespace HL
{
	// World units. The screen always shows kViewHeight units of height; y grows downward.
	constexpr double kViewHeight = 400.0;
	constexpr double kGroundY = 300.0;

	constexpr double kGravity = 1500.0;
	constexpr double kJumpSpeed = 560.0;
	constexpr double kRunSpeed = 240.0;
	constexpr double kPushSpeed = 115.0;
	constexpr double kMaxFallSpeed = 900.0;
	constexpr double kJumpCutFactor = 0.45;   // releasing jump early while rising -> short hop

	constexpr double kStep = 1.0 / 120.0;      // fixed physics substep
	constexpr double kCoyoteTime = 0.10;
	constexpr double kJumpBufferTime = 0.14;

	// Movement added in 2.0 (momentum, slide, vault, ledge grab, roll).
	constexpr double kSprintSpeed = 300.0;      // reached after running without stopping
	constexpr double kSprintDelay = 0.35;       // seconds at a run before speed starts to build
	constexpr double kSprintRamp = 0.8;         // seconds from run to full sprint
	constexpr double kCrawlSpeed = 90.0;
	constexpr double kSlideMinSpeed = 150.0;    // slower than this and "down" is a crouch, not a slide
	constexpr double kSlideTime = 0.75;
	constexpr double kSlideFriction = 170.0;
	constexpr double kLowH = 22.0;              // body height while sliding or crouched
	constexpr double kVaultMax = 36.0;          // obstacles up to this high are vaulted at a run
	constexpr double kGrabReach = 18.0;         // how far below a ledge the hands can still catch it
	constexpr double kHangTime = 0.10;
	constexpr double kClimbTime = 0.30;
	constexpr double kHardLandSpeed = 720.0;    // landing faster than this needs a roll

	constexpr double kPlayerW = 18.0;
	constexpr double kPlayerH = 44.0;
	constexpr double kCrateSize = 56.0;
	constexpr double kTrapW = 40.0;
	constexpr double kLogHalfW = 36.0;
	constexpr double kLogHalfH = 12.0;
	constexpr double kPlatformThickness = 12.0;
	constexpr double kCrumbleThickness = 14.0;

	constexpr double kDeathY = 470.0;           // fallen out of the world
	constexpr double kCrateLostY = 700.0;       // a crate this low has fallen into a pit: it returns home
	constexpr double kWorldBottom = 1.0e6;

	constexpr double kPi = 3.14159265358979323846;

	struct FRect
	{
		double X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;

		bool Overlaps(const FRect& O) const { return X0 < O.X1 && X1 > O.X0 && Y0 < O.Y1 && Y1 > O.Y0; }
	};

	inline double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	inline double Approach(double V, double Target, double MaxDelta)
	{
		if (V < Target) { return (V + MaxDelta > Target) ? Target : V + MaxDelta; }
		return (V - MaxDelta < Target) ? Target : V - MaxDelta;
	}
	inline double SmoothStep(double E0, double E1, double X)
	{
		const double T = Clamp((X - E0) / (E1 - E0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	// Stateless integer hash (lowbias32). Everything procedural is seeded from this so it never changes.
	inline uint32_t Hash32(uint32_t X)
	{
		X ^= X >> 16; X *= 0x7feb352dU;
		X ^= X >> 15; X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}
	inline uint32_t Hash32(uint32_t A, uint32_t B) { return Hash32(A ^ (Hash32(B) + 0x9e3779b9U + (A << 6) + (A >> 2))); }
	inline double Hash01(uint32_t A, uint32_t B = 0) { return (Hash32(A, B) & 0xffffffU) / double(0x1000000); }
	inline double HashRange(uint32_t A, uint32_t B, double Lo, double Hi) { return Lo + (Hi - Lo) * Hash01(A, B); }
}
