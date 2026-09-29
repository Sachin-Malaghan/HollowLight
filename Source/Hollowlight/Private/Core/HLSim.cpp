// EMBERHOME core: deterministic fixed-step simulation. (CLAUDE.md: Core / simulation)
#include "HLSim.h"

#include <algorithm>

namespace HL
{
	namespace
	{
		constexpr double kEps = 0.01;

		struct FSolid
		{
			FRect R;
			ESupport Kind;
			int Index;
			bool OneWay;
		};

		// Calls Fn(const FSolid&) for every solid in the level. Crates are skipped when SkipCrates,
		// and the crate SkipCrate is always skipped (a crate never collides with itself).
		template <typename FnType>
		void ForEachSolid(const FSim& S, bool bIncludeOneWay, bool bIncludeCrates, int SkipCrate, FnType&& Fn)
		{
			const FLevelDef& L = *S.Level;
			for (int I = 0; I < (int)L.Ground.size(); ++I)
			{
				const FGroundDef& G = L.Ground[I];
				Fn(FSolid{ { G.X0, G.Top, G.X1, kWorldBottom }, ESupport::Ground, I, false });
			}
			for (int I = 0; I < (int)L.Blocks.size(); ++I)
			{
				const FBlockDef& B = L.Blocks[I];
				Fn(FSolid{ { B.X0, B.Y0, B.X1, B.Y1 }, ESupport::Block, I, false });
			}
			if (bIncludeCrates)
			{
				for (int I = 0; I < (int)S.Crates.size(); ++I)
				{
					if (I != SkipCrate) { Fn(FSolid{ S.Crates[I].Box(), ESupport::Crate, I, false }); }
				}
			}
			if (bIncludeOneWay)
			{
				for (int I = 0; I < (int)S.Platforms.size(); ++I)
				{
					Fn(FSolid{ S.PlatformBox(I), ESupport::Platform, I, true });
				}
				for (int I = 0; I < (int)S.Crumbles.size(); ++I)
				{
					if (S.Crumbles[I].State < 2) { Fn(FSolid{ S.CrumbleBox(I), ESupport::Crumble, I, true }); }
				}
			}
		}

		bool CrateRestsOnTrap(const FCrate& C, const FTrap& T)
		{
			return std::fabs(C.Y + kCrateSize - T.Y) < 3.0 && T.X > C.X + 4.0 && T.X < C.X + kCrateSize - 4.0;
		}
	}

	void FSim::Load(const FLevelDef& InLevel, int StartCheckpoint)
	{
		Level = &InLevel;
		Time = 0;
		Phase = EPhase::Playing;
		PhaseTime = 0;
		LastDeath = EDeath::None;
		Deaths = 0;
		PlayTime = 0;
		Lantern = 1;
		Fade = 0;
		Events.clear();

		Crates.clear();
		for (const FCrateDef& D : InLevel.Crates)
		{
			FCrate C;
			C.X = C.PrevX = C.HomeX = D.X;
			C.Y = C.PrevY = C.HomeY = D.Bottom - kCrateSize;
			C.Grounded = true;
			Crates.push_back(C);
		}
		Traps.clear();
		for (const FTrapDef& D : InLevel.Traps)
		{
			FTrap T;
			T.X = D.X;
			T.Y = SurfaceAt(D.X, -kWorldBottom);
			Traps.push_back(T);
		}
		Platforms.assign(InLevel.Platforms.size(), FPlatform());
		Crumbles.assign(InLevel.Crumbles.size(), FCrumble());
		UpdateMovers();
		for (FPlatform& Pl : Platforms) { Pl.PrevX = Pl.X; Pl.PrevY = Pl.Y; }

		CheckpointIndex = std::min(StartCheckpoint, (int)InLevel.Checkpoints.size() - 1);
		P = FPlayer();
		P.X = SpawnX();
		P.Y = SurfaceAt(P.X, -kWorldBottom);
		P.Grounded = true;
		P.SupportKind = ESupport::Ground;
		MaxX = P.X;
	}

	double FSim::SpawnX() const
	{
		return CheckpointIndex >= 0 ? Level->Checkpoints[CheckpointIndex] : Level->StartX;
	}

	void FSim::Emit(EEvent Type, double X, double Y, double Strength)
	{
		Events.push_back(FEvent{ Type, X, Y, Strength });
	}

	double FSim::SurfaceAt(double X, double FromY) const
	{
		double Best = kWorldBottom;
		for (const FGroundDef& G : Level->Ground)
		{
			if (X >= G.X0 && X <= G.X1 && G.Top >= FromY) { Best = std::min(Best, G.Top); }
		}
		for (const FBlockDef& B : Level->Blocks)
		{
			if (X >= B.X0 && X <= B.X1 && B.Y0 >= FromY) { Best = std::min(Best, B.Y0); }
		}
		return Best;
	}

	bool FSim::HasSupport(double X, double FeetY, double Up, double Down) const
	{
		bool bFound = false;
		ForEachSolid(*this, true, true, -1, [&](const FSolid& S)
		{
			if (!bFound && X >= S.R.X0 && X <= S.R.X1 && S.R.Y0 >= FeetY - Up && S.R.Y0 <= FeetY + Down)
			{
				bFound = true;
			}
		});
		return bFound;
	}

	double FSim::LogAngle(int I) const
	{
		const FLogDef& D = Level->Logs[I];
		return D.Amp * std::sin(2.0 * kPi * Time / D.Period + D.Phase);
	}

	FRect FSim::LogBox(int I) const
	{
		const FLogDef& D = Level->Logs[I];
		const double A = LogAngle(I);
		const double CX = D.X + D.Rope * std::sin(A);
		const double CY = D.PivotY + D.Rope * std::cos(A);
		return { CX - kLogHalfW, CY - kLogHalfH, CX + kLogHalfW, CY + kLogHalfH };
	}

	FRect FSim::PlatformBox(int I) const
	{
		const double HalfW = Level->Platforms[I].Width * 0.5;
		const FPlatform& Pl = Platforms[I];
		return { Pl.X - HalfW, Pl.Y, Pl.X + HalfW, Pl.Y + kPlatformThickness };
	}

	FRect FSim::CrumbleBox(int I) const
	{
		const FCrumbleDef& D = Level->Crumbles[I];
		return { D.X0, D.Top, D.X1, D.Top + kCrumbleThickness };
	}

	const FWaterDef* FSim::WaterAt(double X) const
	{
		for (const FWaterDef& W : Level->Water)
		{
			if (X >= W.X0 && X <= W.X1) { return &W; }
		}
		return nullptr;
	}

	void FSim::Step(const FInput& In)
	{
		Time += kStep;
		PhaseTime += kStep;
		UpdateMovers();
		UpdateCrates();

		switch (Phase)
		{
		case EPhase::Playing:
			PlayTime += kStep;
			UpdatePlayer(In);
			CheckHazards();
			break;
		case EPhase::Dying:
			if (LastDeath == EDeath::Pit && P.Y < 700.0)
			{
				P.VY = std::min(P.VY + kGravity * kStep, kMaxFallSpeed);
				P.Y += P.VY * kStep;
			}
			else if (LastDeath == EDeath::Water)
			{
				P.Y += 40.0 * kStep;
			}
			if (PhaseTime >= kRespawnAt) { Respawn(); }
			break;
		case EPhase::Won:
			UpdatePlayer(FInput());
			break;
		}

		if (Phase == EPhase::Dying)
		{
			Lantern = Clamp(1.0 - PhaseTime / kDeathLanternOut, 0.0, 1.0);
			Fade = SmoothStep(kDeathLanternOut * 0.6, kDeathFadeEnd, PhaseTime);
		}
		else
		{
			if (Fade < 0.35) { Lantern = Approach(Lantern, 1.0, kStep / 0.3); }
			Fade = Approach(Fade, 0.0, kStep / 0.55);
		}
	}

	void FSim::UpdateMovers()
	{
		for (int I = 0; I < (int)Platforms.size(); ++I)
		{
			const FPlatformDef& D = Level->Platforms[I];
			FPlatform& Pl = Platforms[I];
			Pl.PrevX = Pl.X;
			Pl.PrevY = Pl.Y;
			const double S = 0.5 - 0.5 * std::cos(2.0 * kPi * (Time / D.Period + D.Phase));
			Pl.X = Lerp(D.AX, D.BX, S);
			Pl.Y = Lerp(D.AY, D.BY, S);
		}

		for (int I = 0; I < (int)Crumbles.size(); ++I)
		{
			FCrumble& C = Crumbles[I];
			if (C.State == 1)
			{
				C.Timer -= kStep;
				if (C.Timer <= 0)
				{
					C.State = 2;
					C.Timer = 3.0;
					C.Drop = 0;
					C.DropV = 0;
					const FRect B = CrumbleBox(I);
					Emit(EEvent::CrumbleFall, (B.X0 + B.X1) * 0.5, B.Y0);
				}
			}
			else if (C.State == 2)
			{
				C.DropV = std::min(C.DropV + kGravity * kStep, kMaxFallSpeed);
				C.Drop += C.DropV * kStep;
				C.Timer -= kStep;
				if (C.Timer <= 0 && !CrumbleBox(I).Overlaps(P.Box()))
				{
					C.State = 0;
					C.Drop = 0;
				}
			}
		}

		for (int I = 0; I < (int)Level->Logs.size(); ++I)
		{
			const FLogDef& D = Level->Logs[I];
			const double Now = std::sin(2.0 * kPi * Time / D.Period + D.Phase);
			const double Before = std::sin(2.0 * kPi * (Time - kStep) / D.Period + D.Phase);
			if ((Now >= 0) != (Before >= 0))
			{
				const FRect B = LogBox(I);
				Emit(EEvent::LogSwoosh, (B.X0 + B.X1) * 0.5, B.Y0);
			}
		}
	}

	void FSim::UpdateCrates()
	{
		for (int I = 0; I < (int)Crates.size(); ++I)
		{
			FCrate& C = Crates[I];
			C.PrevX = C.X;
			C.PrevY = C.Y;

			const FWaterDef* W = WaterAt(C.X + kCrateSize * 0.5);
			const bool bWasInWater = C.InWater;
			C.VY += kGravity * kStep;
			C.InWater = W && C.Y + kCrateSize > W->Surface;
			if (C.InWater)
			{
				// Floats with ~17 units submerged; the child's weight pushes it a little lower.
				const double Submerged = Clamp(C.Y + kCrateSize - W->Surface, 0.0, kCrateSize);
				C.VY -= kGravity * (Submerged / 17.0) * kStep;
				C.VY *= std::exp(-3.5 * kStep);
				if (P.Grounded && P.SupportKind == ESupport::Crate && P.SupportIndex == I && Phase == EPhase::Playing)
				{
					C.VY += 300.0 * kStep;
				}
				if (!bWasInWater && C.VY > 150.0) { Emit(EEvent::CrateSplash, C.X + kCrateSize * 0.5, W->Surface, C.VY / 600.0); }
				C.Used = true;
			}
			C.VY = std::min(C.VY, kMaxFallSpeed);
			MoveCrateY(I, C.VY * kStep);

			if (C.Y > kCrateLostY)
			{
				C.X = C.PrevX = C.HomeX;
				C.Y = C.PrevY = C.HomeY;
				C.VY = 0;
				C.InWater = false;
				Emit(EEvent::CrateReset, C.X + kCrateSize * 0.5, C.Y);
			}

			for (FTrap& T : Traps)
			{
				if (!T.Closed && CrateRestsOnTrap(C, T))
				{
					T.Closed = true;
					T.ByCrate = true;
					T.ClosedTime = Time;
					Emit(EEvent::TrapSnapCrate, T.X, T.Y);
				}
			}
		}
	}

	void FSim::MoveCrateY(int Index, double DY)
	{
		FCrate& C = Crates[Index];
		const FRect B = C.Box();
		double Allowed = DY;
		bool bHit = false;
		ForEachSolid(*this, true, true, Index, [&](const FSolid& S)
		{
			if (!(B.X0 < S.R.X1 - kEps && B.X1 > S.R.X0 + kEps)) { return; }
			if (DY > 0 && B.Y1 <= S.R.Y0 + 0.5 && B.Y1 + DY >= S.R.Y0)
			{
				const double D = S.R.Y0 - B.Y1;
				if (D < Allowed) { Allowed = D; bHit = true; }
			}
			else if (DY < 0 && !S.OneWay && B.Y0 >= S.R.Y1 - kEps && B.Y0 + DY < S.R.Y1)
			{
				const double D = S.R.Y1 - B.Y0;
				if (D > Allowed) { Allowed = D; bHit = true; }
			}
		});
		C.Y += Allowed;
		const bool bWasGrounded = C.Grounded;
		C.Grounded = bHit && DY > 0;
		if (bHit)
		{
			if (DY > 0 && !bWasGrounded && C.VY > 250.0) { Emit(EEvent::CrateLand, C.X + kCrateSize * 0.5, C.Y + kCrateSize, C.VY / 900.0); }
			C.VY = 0;
		}
	}

	double FSim::MoveCrateX(int Index, double DX)
	{
		FCrate& C = Crates[Index];
		if (!C.Grounded && !C.InWater) { return 0; }
		const FRect B = C.Box();
		double Allowed = DX;
		ForEachSolid(*this, false, true, Index, [&](const FSolid& S)
		{
			if (!(B.Y0 < S.R.Y1 - kEps && B.Y1 > S.R.Y0 + kEps)) { return; }
			if (DX > 0 && B.X1 <= S.R.X0 + kEps && B.X1 + DX > S.R.X0) { Allowed = std::min(Allowed, S.R.X0 - B.X1); }
			if (DX < 0 && B.X0 >= S.R.X1 - kEps && B.X0 + DX < S.R.X1) { Allowed = std::max(Allowed, S.R.X1 - B.X0); }
		});
		if (DX > 0) { Allowed = std::max(Allowed, 0.0); } else { Allowed = std::min(Allowed, 0.0); }
		C.X += Allowed;
		if (std::fabs(Allowed) > 1e-6) { C.Used = true; }
		return Allowed;
	}

	void FSim::UpdatePlayer(const FInput& In)
	{
		FPlayer& Pl = P;

		if (In.Jump && !Pl.JumpHeld) { Pl.JumpBuffer = kJumpBufferTime; }
		if (!In.Jump && Pl.JumpHeld && Pl.VY < 0 && !Pl.JumpCut)
		{
			Pl.VY *= kJumpCutFactor;
			Pl.JumpCut = true;
		}
		Pl.JumpHeld = In.Jump;

		// Ride whatever we stand on.
		if (Pl.Grounded)
		{
			if (Pl.SupportKind == ESupport::Platform && Pl.SupportIndex >= 0)
			{
				const FPlatform& Mover = Platforms[Pl.SupportIndex];
				Pl.Y = Mover.Y;
				MovePlayerX(Mover.X - Mover.PrevX, false);
			}
			else if (Pl.SupportKind == ESupport::Crate && Pl.SupportIndex >= 0)
			{
				const FCrate& C = Crates[Pl.SupportIndex];
				Pl.Y = C.Y;
				MovePlayerX(C.X - C.PrevX, false);
			}
		}

		double Target = In.Dir * kRunSpeed;
		if (Pl.PushTimer > 0) { Target = Clamp(Target, -kPushSpeed, kPushSpeed); }
		const double Accel = Pl.Grounded ? (In.Dir != 0 ? 2600.0 : 3000.0) : 1600.0;
		Pl.VX = Approach(Pl.VX, Target, Accel * kStep);
		if (In.Dir != 0) { Pl.Facing = In.Dir; }
		Pl.PushTimer -= kStep;

		Pl.Coyote = Pl.Grounded ? kCoyoteTime : Pl.Coyote - kStep;
		Pl.JumpBuffer -= kStep;
		if (Pl.JumpBuffer > 0 && Pl.Coyote > 0)
		{
			Pl.VY = -kJumpSpeed;
			Pl.Grounded = false;
			Pl.Coyote = 0;
			Pl.JumpBuffer = 0;
			Pl.JumpCut = false;
			if (!In.Jump)
			{
				Pl.VY *= kJumpCutFactor;
				Pl.JumpCut = true;
			}
			Emit(EEvent::Jump, Pl.X, Pl.Y);
		}

		Pl.VY = std::min(Pl.VY + kGravity * kStep, kMaxFallSpeed);

		const double StartX = Pl.X;
		MovePlayerX(Pl.VX * kStep, true);

		const bool bWasGrounded = Pl.Grounded;
		const double FallSpeed = Pl.VY;
		Pl.Grounded = false;
		MovePlayerY(Pl.VY * kStep);
		if (Pl.Grounded && !bWasGrounded && Pl.AirTime > 0.08)
		{
			Emit(EEvent::Land, Pl.X, Pl.Y, Clamp(FallSpeed / 900.0, 0.0, 1.0));
		}
		Pl.AirTime = Pl.Grounded ? 0.0 : Pl.AirTime + kStep;

		const double Moved = std::fabs(Pl.X - StartX);
		Pl.StuckTime = (In.Dir != 0 && Moved < 0.25 * kRunSpeed * kStep) ? Pl.StuckTime + kStep : 0.0;

		if (Pl.Grounded)
		{
			const double Before = Pl.RunPhase;
			Pl.RunPhase += Moved * 0.075;
			if (std::floor(Pl.RunPhase / kPi) != std::floor(Before / kPi))
			{
				Emit(EEvent::Footstep, Pl.X, Pl.Y, Clamp(std::fabs(Pl.VX) / kRunSpeed, 0.2, 1.0));
			}
		}
	}

	void FSim::MovePlayerX(double DX, bool bAllowPush)
	{
		if (DX == 0) { return; }
		FPlayer& Pl = P;
		FRect B = Pl.Box();
		double Allowed = DX;
		bool bBlocked = false;

		// Walls first so a crate is never pushed through a wall the child is also touching.
		ForEachSolid(*this, false, true, -1, [&](const FSolid& S)
		{
			if (!(B.Y0 < S.R.Y1 - kEps && B.Y1 > S.R.Y0 + kEps)) { return; }
			const bool bEntering = DX > 0 ? (B.X1 <= S.R.X0 + kEps && B.X1 + DX > S.R.X0)
			                              : (B.X0 >= S.R.X1 - kEps && B.X0 + DX < S.R.X1);
			if (!bEntering) { return; }

			// Small lips (a floating crate riding a little high) are stepped over.
			if (Pl.Grounded && S.R.Y0 >= B.Y1 - 6.0)
			{
				Pl.Y = S.R.Y0;
				B = Pl.Box();
				return;
			}

			if (S.Kind == ESupport::Crate && bAllowPush && Pl.Grounded)
			{
				const double Need = DX > 0 ? (B.X1 + DX) - S.R.X0 : (B.X0 + DX) - S.R.X1;
				const double Pushed = MoveCrateX(S.Index, Need);
				if (std::fabs(Pushed) > 1e-4)
				{
					if (Pl.PushTimer <= 0) { Emit(EEvent::PushStart, Pl.X, Pl.Y); }
					Pl.PushTimer = 0.1;
				}
				const FRect CB = Crates[S.Index].Box();
				const double Limit = DX > 0 ? CB.X0 - B.X1 : CB.X1 - B.X0;
				if (DX > 0 ? Limit < Allowed : Limit > Allowed) { Allowed = Limit; bBlocked = std::fabs(Pushed) < 1e-4; }
				return;
			}

			const double Limit = DX > 0 ? S.R.X0 - B.X1 : S.R.X1 - B.X0;
			if (DX > 0 ? Limit < Allowed : Limit > Allowed)
			{
				Allowed = Limit;
				bBlocked = true;
			}
		});

		if (DX > 0) { Allowed = std::max(Allowed, 0.0); } else { Allowed = std::min(Allowed, 0.0); }
		Pl.X += Allowed;
		if (bBlocked) { Pl.VX = 0; }
		Pl.X = Clamp(Pl.X, Level->MinX + kPlayerW, Level->MaxX - kPlayerW);
	}

	void FSim::MovePlayerY(double DY)
	{
		FPlayer& Pl = P;
		const FRect B = Pl.Box();
		double Allowed = DY;
		ESupport Kind = ESupport::None;
		int Index = -1;

		ForEachSolid(*this, true, true, -1, [&](const FSolid& S)
		{
			if (!(B.X0 < S.R.X1 - kEps && B.X1 > S.R.X0 + kEps)) { return; }
			if (DY > 0 && B.Y1 <= S.R.Y0 + 0.5 && B.Y1 + DY >= S.R.Y0)
			{
				const double D = S.R.Y0 - B.Y1;
				if (D <= Allowed) { Allowed = D; Kind = S.Kind; Index = S.Index; }
			}
			else if (DY < 0 && !S.OneWay && B.Y0 >= S.R.Y1 - kEps && B.Y0 + DY < S.R.Y1)
			{
				const double D = S.R.Y1 - B.Y0;
				if (D > Allowed)
				{
					Allowed = D;
					Pl.VY = 0;
				}
			}
		});

		Pl.Y += Allowed;
		if (Kind != ESupport::None)
		{
			Pl.Grounded = true;
			Pl.VY = 0;
			Pl.SupportKind = Kind;
			Pl.SupportIndex = Index;
		}
		else
		{
			Pl.SupportKind = ESupport::None;
			Pl.SupportIndex = -1;
		}
	}

	void FSim::CheckHazards()
	{
		if (P.Y > kDeathY) { Kill(EDeath::Pit); return; }

		if (const FWaterDef* W = WaterAt(P.X))
		{
			if (P.Y > W->Surface + 10.0)
			{
				Emit(EEvent::Splash, P.X, W->Surface);
				Kill(EDeath::Water);
				return;
			}
		}

		for (FTrap& T : Traps)
		{
			if (!T.Closed && P.Grounded && std::fabs(P.X - T.X) < 15.0 && std::fabs(P.Y - T.Y) < 2.0)
			{
				T.Closed = true;
				T.ByCrate = false;
				T.ClosedTime = Time;
				Emit(EEvent::TrapSnap, T.X, T.Y);
				Kill(EDeath::Trap);
				return;
			}
		}

		const FRect Body = P.Box();
		const FRect Inner = { Body.X0 + 2, Body.Y0 + 3, Body.X1 - 2, Body.Y1 - 1 };
		for (int I = 0; I < (int)Level->Logs.size(); ++I)
		{
			const FRect L = LogBox(I);
			const FRect LogInner = { L.X0 + 3, L.Y0 + 2, L.X1 - 3, L.Y1 - 2 };
			if (Inner.Overlaps(LogInner))
			{
				Kill(EDeath::Log);
				return;
			}
		}

		if (P.Grounded && P.SupportKind == ESupport::Crumble && P.SupportIndex >= 0)
		{
			FCrumble& C = Crumbles[P.SupportIndex];
			if (C.State == 0)
			{
				C.State = 1;
				C.Timer = 0.55;
				Emit(EEvent::CrumbleCreak, P.X, P.Y);
			}
		}

		const std::vector<double>& CPs = Level->Checkpoints;
		for (int I = CheckpointIndex + 1; I < (int)CPs.size(); ++I)
		{
			if (P.X >= CPs[I] && P.Grounded)
			{
				CheckpointIndex = I;
				Emit(EEvent::Checkpoint, CPs[I], SurfaceAt(CPs[I], -kWorldBottom));
			}
		}

		MaxX = std::max(MaxX, P.X);

		if (P.X >= Level->GoalX - 20.0 && P.Grounded)
		{
			Phase = EPhase::Won;
			PhaseTime = 0;
			Emit(EEvent::Goal, Level->GoalX, P.Y);
		}
	}

	void FSim::Kill(EDeath Cause)
	{
		Phase = EPhase::Dying;
		PhaseTime = 0;
		LastDeath = Cause;
		++Deaths;
		P.VX = 0;
		if (Cause != EDeath::Pit) { P.VY = 0; }
		Emit(EEvent::Death, P.X, P.Y, (double)(int)Cause);
	}

	void FSim::ResetTransient()
	{
		for (FTrap& T : Traps)
		{
			bool bHeld = false;
			for (const FCrate& C : Crates) { bHeld = bHeld || CrateRestsOnTrap(C, T); }
			T.Closed = bHeld;
			T.ByCrate = bHeld;
		}
		for (FCrumble& C : Crumbles) { C = FCrumble(); }
	}

	void FSim::Respawn()
	{
		ResetTransient();
		P = FPlayer();
		P.X = SpawnX();
		P.Y = SurfaceAt(P.X, -kWorldBottom);
		for (int I = 0; I < (int)Crates.size(); ++I)
		{
			const FRect CB = Crates[I].Box();
			if (P.X > CB.X0 - kPlayerW * 0.5 && P.X < CB.X1 + kPlayerW * 0.5 && CB.Y0 < P.Y) { P.Y = std::min(P.Y, CB.Y0); }
		}
		P.Grounded = true;
		P.SupportKind = ESupport::Ground;
		Phase = EPhase::Playing;
		PhaseTime = 0;
		Emit(EEvent::Respawn, P.X, P.Y);
	}
}
