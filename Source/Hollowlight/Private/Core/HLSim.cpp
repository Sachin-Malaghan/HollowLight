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
			for (int I = 0; I < (int)S.Gates.size(); ++I)
			{
				if (S.GateSolid(I))
				{
					const FGateDef& G = L.Gates[I];
					Fn(FSolid{ { G.X0, G.Y0, G.X1, G.Y1 }, ESupport::Gate, I, false });
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
		Gates.assign(InLevel.Gates.size(), FGate());
		for (int I = 0; I < (int)Gates.size(); ++I)
		{
			Gates[I].Latched = Gates[I].Open = InLevel.Gates[I].bStartOpen;
			Gates[I].Amount = Gates[I].Open ? 1.0 : 0.0;
		}
		LeverOn.assign(InLevel.Levers.size(), false);
		PlateDown.assign(InLevel.Plates.size(), false);
		SocketUsed.assign(InLevel.Sockets.size(), false);
		EmberTaken.assign(InLevel.Embers.size(), false);
		ResetDangers();
		Items.clear();
		for (const FItemDef& D : InLevel.Items)
		{
			FItem It;
			It.X = D.X;
			It.Y = D.Y;
			Items.push_back(It);
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
		P.IdleX = P.X;
		MaxX = P.X;
		Ghost = FGhost();
		Linger = 0;
		ProgressX = P.X;

		Dog = FDog();
		Dog.Active = InLevel.bDog;
		if (Dog.Active) { PlaceDogNearPlayer(); }
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
		UpdateDangers();
		UpdateCrates();
		UpdateMechanisms();
		if (Phase != EPhase::Dying) { UpdateDog(); }

		switch (Phase)
		{
		case EPhase::Playing:
			PlayTime += kStep;
			UpdatePlayer(In);
			CheckHazards();
			if (Phase == EPhase::Playing) { UpdateGhost(); }
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

	bool FSim::IsFree(const FRect& R, int IgnoreCrate) const
	{
		bool bFree = true;
		ForEachSolid(*this, false, true, IgnoreCrate, [&](const FSolid& S)
		{
			if (bFree && R.Overlaps(S.R)) { bFree = false; }
		});
		return bFree;
	}

	bool FSim::CanStand() const
	{
		// Only the space the body would grow into matters.
		return IsFree({ P.X - kPlayerW * 0.5 + 0.5, P.Y - kPlayerH, P.X + kPlayerW * 0.5 - 0.5, P.Y - kLowH - kEps });
	}

	void FSim::UpdateHang(const FInput& In)
	{
		FPlayer& Pl = P;
		Pl.JumpHeld = In.Jump;
		Pl.HangTime += kStep;
		Pl.VX = Pl.VY = 0;

		// Let go if asked, or if the thing being held has moved (a crate that slid or sank).
		bool bDrop = Pl.Hang == 1 && In.Down;
		if (Pl.HangKind == ESupport::Crate && Pl.HangIndex >= 0)
		{
			const FCrate& C = Crates[Pl.HangIndex];
			if (std::fabs(C.Y - Pl.HangLedgeY) > 1.5 || std::fabs(C.X - C.PrevX) > 1e-6) { bDrop = true; }
		}
		if (bDrop)
		{
			Pl.Hang = 0;
			Pl.GrabCooldown = 0.35;
			Pl.Pose = EPose::Stand;
			return;
		}

		if (Pl.Hang == 1)
		{
			Pl.Pose = EPose::Hang;
			if (Pl.HangTime >= kHangTime)
			{
				Pl.Hang = 2;
				Pl.HangTime = 0;
				Emit(EEvent::Climb, Pl.X, Pl.Y);
			}
			return;
		}

		// Pull up: rise first, then swing over the edge.
		Pl.Pose = EPose::Climb;
		const double T = Pl.HangTime / kClimbTime;
		Pl.Y = Lerp(Pl.HangLedgeY + kPlayerH + 2.0, Pl.HangLedgeY, SmoothStep(0.0, 0.65, T));
		Pl.X = Lerp(Pl.HangFromX, Pl.HangToX, SmoothStep(0.45, 1.0, T));
		if (T >= 1.0)
		{
			Pl.Hang = 0;
			Pl.X = Pl.HangToX;
			Pl.Y = Pl.HangLedgeY;
			Pl.Grounded = true;
			Pl.SupportKind = Pl.HangKind;
			Pl.SupportIndex = Pl.HangIndex;
			Pl.VX = Pl.HangDir * kRunSpeed * 0.5;
			Pl.AirTime = 0;
			Pl.Pose = EPose::Stand;
		}
	}

	void FSim::UpdatePlayer(const FInput& InRaw)
	{
		FPlayer& Pl = P;
		if (Pl.Hang != 0)
		{
			UpdateHang(InRaw);
			return;
		}
		if (Pl.Ladder >= 0)
		{
			UpdateLadder(InRaw);
			return;
		}

		// How long since the child last got anywhere: the dog's cue to point something out.
		if (std::fabs(Pl.X - Pl.IdleX) > 40.0) { Pl.IdleX = Pl.X; Pl.IdleTime = 0; }
		else { Pl.IdleTime += kStep; }

		// After a hard landing without a roll there is a beat before control returns.
		FInput In = InRaw;
		if (Pl.StunTime > 0)
		{
			Pl.StunTime -= kStep;
			In = FInput();
		}
		Pl.GrabCooldown -= kStep;
		Pl.VaultTimer -= kStep;
		Pl.RollTime -= kStep;

		// ACT: pick up / use a tool, throw a lever, or whistle.
		if (In.Act && !Pl.ActHeld && Pl.Grounded) { DoAct(); }
		Pl.ActHeld = In.Act;

		// Ladders: jump (up) at a ladder takes hold of it; down at the top of one climbs onto it.
		if (In.Jump && !Pl.Low)
		{
			const int Lad = LadderAt(Pl.X, Pl.Y);
			if (Lad >= 0 && (Pl.Grounded || Pl.VY > -200.0))
			{
				Pl.Ladder = Lad;
				Pl.JumpBuffer = 0;
				Pl.JumpHeld = true;
				Pl.VX = Pl.VY = 0;
				Pl.Y -= 1.0;
				UpdateLadder(In);
				return;
			}
		}
		if (In.Down && Pl.Grounded && std::fabs(Pl.VX) < 70.0)
		{
			for (int I = 0; I < (int)Level->Ladders.size(); ++I)
			{
				const FLadderDef& Ld = Level->Ladders[I];
				if (std::fabs(Pl.Y - Ld.Top) < 2.0 && std::fabs(Pl.X - Ld.X) < 26.0)
				{
					Pl.Ladder = I;
					Pl.Y = Ld.Top + 2.0;
					UpdateLadder(In);
					return;
				}
			}
		}

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

		// Scree: too steep and loose to stand on. The child sits back and slides, faster and faster.
		const int Scree = (Pl.Grounded && Pl.SupportKind == ESupport::Ground && Pl.SupportIndex >= 0) ? Level->Ground[Pl.SupportIndex].Slide : 0;
		if (Scree != 0)
		{
			if (Pl.Scree == 0) { Emit(EEvent::Slide, Pl.X, Pl.Y); }
			Pl.Low = true;
			Pl.H = kLowH;
			Pl.Sliding = true;
			Pl.SlideTime = 0;
			Pl.Facing = Scree;
		}
		Pl.Scree = Scree;

		// Stance: "down" at a run is a slide, otherwise a crouch. Stay low under anything too low to stand in.
		if (Pl.Grounded && In.Down && !Pl.Low)
		{
			Pl.Low = true;
			Pl.H = kLowH;
			if (std::fabs(Pl.VX) >= kSlideMinSpeed)
			{
				Pl.Sliding = true;
				Pl.SlideTime = 0;
				Emit(EEvent::Slide, Pl.X, Pl.Y);
			}
		}
		if (Pl.Low)
		{
			if (Pl.Sliding && Scree == 0)
			{
				Pl.SlideTime += kStep;
				if (Pl.SlideTime >= kSlideTime || std::fabs(Pl.VX) < 50.0) { Pl.Sliding = false; }
			}
			const bool bHold = In.Down || (Pl.Sliding && Pl.SlideTime < 0.25);
			if (!bHold && CanStand())
			{
				Pl.Low = false;
				Pl.Sliding = false;
				Pl.H = kPlayerH;
			}
		}

		// Momentum: keep running and the pace builds from a run to a sprint.
		const bool bRunning = In.Dir != 0 && In.Dir * Pl.VX > 0.75 * kRunSpeed && !Pl.Low && Pl.PushTimer <= 0;
		if (!bRunning) { Pl.SprintTime = 0; }
		else if (Pl.Grounded) { Pl.SprintTime += kStep; }
		const double TopSpeed = kRunSpeed + (kSprintSpeed - kRunSpeed) * Clamp((Pl.SprintTime - kSprintDelay) / kSprintRamp, 0.0, 1.0);

		if (Pl.Sliding)
		{
			// No steering in a slide. On scree the hill does the pushing.
			if (Scree != 0) { Pl.VX = Approach(Pl.VX, Scree * kScreeSpeed, kScreeAccel * kStep); }
			else { Pl.VX = Approach(Pl.VX, 0.0, kSlideFriction * kStep); }
		}
		else
		{
			double Target = In.Dir * (Pl.Low ? kCrawlSpeed : TopSpeed);
			if (Pl.PushTimer > 0) { Target = Clamp(Target, -kPushSpeed, kPushSpeed); }
			const double Accel = Pl.Grounded ? (In.Dir != 0 ? 2600.0 : 3000.0) : 1600.0;
			Pl.VX = Approach(Pl.VX, Target, Accel * kStep);
			if (In.Dir != 0) { Pl.Facing = In.Dir; }
		}
		Pl.PushTimer -= kStep;

		Pl.Coyote = Pl.Grounded ? kCoyoteTime : Pl.Coyote - kStep;
		Pl.JumpBuffer -= kStep;
		if (Pl.JumpBuffer > 0 && Pl.Coyote > 0 && Pl.Low && !CanStand())
		{
			Pl.JumpBuffer = 0;   // no room to jump under a low beam
		}
		if (Pl.JumpBuffer > 0 && Pl.Coyote > 0)
		{
			Pl.Low = false;
			Pl.Sliding = false;
			Pl.H = kPlayerH;
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
		if (Pl.Hang != 0) { return; }   // caught a ledge during the move

		const bool bWasGrounded = Pl.Grounded;
		const bool bWasOnGround = bWasGrounded && Pl.SupportKind == ESupport::Ground;
		const double FallSpeed = Pl.VY;
		Pl.Grounded = false;
		MovePlayerY(Pl.VY * kStep);
		if (!Pl.Grounded && bWasOnGround && FallSpeed >= 0 && FallSpeed < 60.0)
		{
			// Going down a hillside's steps: keep the feet on the ground instead of hopping from step to step.
			const double Y0 = Pl.Y;
			MovePlayerY(8.0);
			if (!Pl.Grounded) { Pl.Y = Y0; }
		}
		if (Pl.Grounded && !bWasGrounded && Pl.AirTime > 0.08)
		{
			Emit(EEvent::Land, Pl.X, Pl.Y, Clamp(FallSpeed / 900.0, 0.0, 1.0));
			if (FallSpeed > kHardLandSpeed)
			{
				// A long drop: roll out of it if moving (or holding down), otherwise land heavily.
				if (InRaw.Down || std::fabs(Pl.VX) > 120.0)
				{
					Pl.RollTime = 0.4;
					Emit(EEvent::Roll, Pl.X, Pl.Y);
				}
				else
				{
					Pl.StunTime = 0.22;
					Pl.VX = 0;
					Emit(EEvent::HardLand, Pl.X, Pl.Y);
				}
			}
		}
		Pl.AirTime = Pl.Grounded ? 0.0 : Pl.AirTime + kStep;

		Pl.Pose = Pl.StunTime > 0 ? EPose::Stunned
		        : Pl.RollTime > 0 ? EPose::Roll
		        : Pl.Sliding ? EPose::Slide
		        : Pl.Low ? EPose::Crouch
		        : (Pl.VaultTimer > 0 && !Pl.Grounded) ? EPose::Vault
		        : EPose::Stand;

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
		const int Dir = DX > 0 ? 1 : -1;
		double VaultHeight = -1;          // > 0: hop over this obstacle without breaking stride
		bool bGrab = false;               // catch the top edge of this wall
		FSolid GrabSolid{};

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

			// Vault: at a run, anything up to knee height is hopped without slowing down.
			if (bAllowPush && Pl.Grounded && !Pl.Low && S.Kind != ESupport::Crate && std::fabs(Pl.VX) > 150.0 &&
				S.R.Y0 >= B.Y1 - kVaultMax &&
				IsFree({ std::min(B.X0, B.X0 + Dir * kPlayerW), S.R.Y0 - kPlayerH, std::max(B.X1, B.X1 + Dir * kPlayerW), S.R.Y0 - kEps }))
			{
				VaultHeight = std::max(VaultHeight, B.Y1 - S.R.Y0);
			}

			// Ledge grab: in the air, hands near the top edge of a wall with room to stand on it.
			if (bAllowPush && !Pl.Grounded && !Pl.Low && Pl.GrabCooldown <= 0 && Pl.VY > -300.0 && !bGrab &&
				B.Y0 >= S.R.Y0 - 6.0 && B.Y0 <= S.R.Y0 + kGrabReach &&
				(S.Kind != ESupport::Crate || Crates[S.Index].Grounded))
			{
				const double EdgeX = Dir > 0 ? S.R.X0 : S.R.X1;
				const FRect Above = { std::min(EdgeX, EdgeX + Dir * (kPlayerW + 1.0)) + 0.25, S.R.Y0 - kPlayerH,
				                      std::max(EdgeX, EdgeX + Dir * (kPlayerW + 1.0)) - 0.25, S.R.Y0 - kEps };
				if (IsFree(Above))
				{
					bGrab = true;
					GrabSolid = S;
				}
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

		if (bGrab && bBlocked)
		{
			const double EdgeX = Dir > 0 ? GrabSolid.R.X0 : GrabSolid.R.X1;
			Pl.Hang = 1;
			Pl.HangDir = Dir;
			Pl.HangTime = 0;
			Pl.HangLedgeY = GrabSolid.R.Y0;
			Pl.HangFromX = EdgeX - Dir * kPlayerW * 0.5;
			Pl.HangToX = EdgeX + Dir * (kPlayerW * 0.5 + 1.0);
			Pl.HangKind = GrabSolid.Kind;
			Pl.HangIndex = GrabSolid.Index;
			Pl.X = Pl.HangFromX;
			Pl.Y = Pl.HangLedgeY + kPlayerH + 2.0;
			Pl.VX = Pl.VY = 0;
			Pl.Facing = Dir;
			Pl.Pose = EPose::Hang;
			Emit(EEvent::Grab, Pl.X, Pl.HangLedgeY);
			return;
		}

		if (VaultHeight > 0 && bBlocked)
		{
			Pl.VY = -std::sqrt(2.0 * kGravity * (VaultHeight + 10.0));
			Pl.Grounded = false;
			Pl.Coyote = 0;
			Pl.JumpCut = true;
			Pl.VaultTimer = 0.3;
			bBlocked = false;       // keep the momentum
			Emit(EEvent::Vault, Pl.X, Pl.Y);
		}
		else if (Pl.VaultTimer > 0 && bBlocked)
		{
			bBlocked = false;       // still clearing the obstacle
		}

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

		for (int I = 0; I < (int)Level->Hazards.size(); ++I)
		{
			if (!HazardActive(I)) { continue; }
			const FRect H = HazardBox(I);
			const FRect HIn = { H.X0 + 2, H.Y0 + 2, H.X1 - 2, H.Y1 - 1 };
			if (Inner.Overlaps(HIn))
			{
				Kill(Level->Hazards[I].Kind == EHazard::Fire ? EDeath::Fire : EDeath::Machine);
				return;
			}
		}
		for (int I = 0; I < (int)Level->Floods.size(); ++I)
		{
			const FFloodDef& F = Level->Floods[I];
			if (FloodOn[I] && P.X >= F.X0 && P.X <= F.X1 && P.Y > FloodY[I] + 10.0)
			{
				Emit(EEvent::Splash, P.X, FloodY[I]);
				Kill(EDeath::Water);
				return;
			}
		}
		for (const FChaser& C : Chasers)
		{
			if (C.State == 1 && std::fabs(C.X - P.X) < 18.0 && std::fabs(C.Y - P.Y) < 50.0)
			{
				Kill(EDeath::Wolf);
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

		for (int I = 0; I < (int)Level->Embers.size(); ++I)
		{
			const FEmberDef& E = Level->Embers[I];
			if (!EmberTaken[I] && E.X > Body.X0 - 12.0 && E.X < Body.X1 + 12.0 && E.Y > Body.Y0 - 12.0 && E.Y < Body.Y1 + 12.0)
			{
				EmberTaken[I] = true;
				Emit(EEvent::EmberCollect, E.X, E.Y, (double)EmbersTaken());
			}
		}

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
		ResetDangers();
	}

	void FSim::Respawn()
	{
		ResetTransient();
		const int Carried = P.Carry;
		P = FPlayer();
		P.Carry = Carried;
		P.X = SpawnX();
		P.Y = SurfaceAt(P.X, -kWorldBottom);
		for (int I = 0; I < (int)Crates.size(); ++I)
		{
			const FRect CB = Crates[I].Box();
			if (P.X > CB.X0 - kPlayerW * 0.5 && P.X < CB.X1 + kPlayerW * 0.5 && CB.Y0 < P.Y) { P.Y = std::min(P.Y, CB.Y0); }
		}
		P.Grounded = true;
		P.SupportKind = ESupport::Ground;
		P.IdleX = P.X;
		Phase = EPhase::Playing;
		PhaseTime = 0;
		Ghost = FGhost();
		Linger = 0;
		ProgressX = P.X;
		if (Dog.Active)
		{
			Dog.Mode = EDogMode::Follow;
			PlaceDogNearPlayer();
		}
		Emit(EEvent::Respawn, P.X, P.Y);
	}

	// ---------------------------------------------------------------------------------------------
	// 2.0: ladders, gates, levers, plates, tools

	FRect FSim::GateBox(int I) const
	{
		const FGateDef& G = Level->Gates[I];
		if (G.bBridge) { return { G.X0, G.Y0, G.X1, G.Y1 }; }   // a bridge deck swings; the renderer draws that
		const double Lift = Gates[I].Amount * (G.Y1 - G.Y0 - 6.0);
		return { G.X0, G.Y0 - Lift, G.X1, G.Y1 - Lift };
	}

	int FSim::LadderAt(double X, double FeetY) const
	{
		for (int I = 0; I < (int)Level->Ladders.size(); ++I)
		{
			const FLadderDef& L = Level->Ladders[I];
			if (std::fabs(X - L.X) < 13.0 && FeetY > L.Top + 1.0 && FeetY <= L.Bottom + 1.0) { return I; }
		}
		return -1;
	}

	void FSim::UpdateLadder(const FInput& In)
	{
		FPlayer& Pl = P;
		const FLadderDef& L = Level->Ladders[Pl.Ladder];
		Pl.JumpHeld = In.Jump;
		Pl.ActHeld = In.Act;
		Pl.Pose = EPose::Ladder;
		Pl.Grounded = false;
		Pl.VX = Pl.VY = 0;
		Pl.X = L.X;
		Pl.Low = false;
		Pl.Sliding = false;
		Pl.H = kPlayerH;
		Pl.AirTime = 0;

		const double V = In.Jump ? -105.0 : (In.Down ? 140.0 : 0.0);
		const double Before = Pl.LadderPhase;
		Pl.Y += V * kStep;
		Pl.LadderPhase += std::fabs(V) * kStep * 0.09;
		if (std::floor(Pl.LadderPhase / kPi) != std::floor(Before / kPi)) { Emit(EEvent::LadderStep, Pl.X, Pl.Y, L.bRope ? 2.0 : 1.0); }

		auto StepOff = [&](double Y)
		{
			Pl.Ladder = -1;
			Pl.Y = Y;
			Pl.Pose = EPose::Stand;
			Pl.GrabCooldown = 0.2;
			// Step onto whichever side has ground at this height.
			if (HasSupport(L.X + 20.0, Y, 2.0, 2.0)) { Pl.X = L.X + 20.0; Pl.Facing = 1; }
			else if (HasSupport(L.X - 20.0, Y, 2.0, 2.0)) { Pl.X = L.X - 20.0; Pl.Facing = -1; }
			Pl.Grounded = HasSupport(Pl.X, Y, 2.0, 2.0);
			Pl.Coyote = kCoyoteTime;
		};

		if (Pl.Y <= L.Top) { StepOff(L.Top); return; }
		if (Pl.Y >= L.Bottom) { StepOff(L.Bottom); return; }
		if (In.Dir != 0 && !In.Jump && !In.Down)
		{
			// Let go sideways.
			Pl.Ladder = -1;
			Pl.VX = In.Dir * 120.0;
			Pl.Facing = In.Dir;
			Pl.GrabCooldown = 0.25;
			Pl.Pose = EPose::Stand;
		}
	}

	void FSim::ThrowLever(int Index, double X, double Y)
	{
		LeverOn[Index] = !LeverOn[Index];
		Linger = 0;
		FGate& G = Gates[Level->Levers[Index].Gate];
		G.Latched = !G.Latched;
		Emit(EEvent::Lever, X, Y);
	}

	EActKind FSim::ActKind(int* OutIndex) const
	{
		const FPlayer& Pl = P;
		const FLevelDef& L = *Level;
		int Dummy = -1;
		int& Index = OutIndex ? *OutIndex : Dummy;
		// 1. A socket that wants the tool in hand.
		for (int I = 0; I < (int)L.Sockets.size(); ++I)
		{
			const FSocketDef& S = L.Sockets[I];
			if (!SocketUsed[I] && Pl.Carry >= 0 && L.Items[Pl.Carry].Kind == S.Kind && std::fabs(Pl.X - S.X) <= 28.0 && std::fabs(Pl.Y - S.Y) <= 8.0)
			{
				Index = I;
				return EActKind::Use;
			}
		}
		// 2. A tool on the ground.
		if (Pl.Carry < 0)
		{
			for (int I = 0; I < (int)Items.size(); ++I)
			{
				if (!Items[I].Used && !Items[I].Carried && std::fabs(Pl.X - Items[I].X) < 26.0 && std::fabs(Pl.Y - Items[I].Y) < 8.0)
				{
					Index = I;
					return EActKind::Take;
				}
			}
		}
		// 3. A lever within reach.
		for (int I = 0; I < (int)L.Levers.size(); ++I)
		{
			if (std::fabs(Pl.X - L.Levers[I].X) < 26.0 && std::fabs(Pl.Y - L.Levers[I].Y) < 8.0)
			{
				Index = I;
				return EActKind::Lever;
			}
		}
		// 4. Nothing in reach: whistle for the dog.
		return EActKind::Whistle;
	}

	void FSim::DoAct()
	{
		FPlayer& Pl = P;
		const FLevelDef& L = *Level;
		int Index = -1;
		switch (ActKind(&Index))
		{
		case EActKind::Use:
			SocketUsed[Index] = true;
			Gates[L.Sockets[Index].Gate].Latched = true;
			Items[Pl.Carry].Used = true;
			Items[Pl.Carry].Carried = false;
			Pl.Carry = -1;
			Linger = 0;
			Emit(EEvent::UseTool, L.Sockets[Index].X, L.Sockets[Index].Y);
			return;
		case EActKind::Take:
			Items[Index].Carried = true;
			Pl.Carry = Index;
			Linger = 0;
			Emit(EEvent::Pickup, Items[Index].X, Items[Index].Y);
			return;
		case EActKind::Lever:
			ThrowLever(Index, L.Levers[Index].X, L.Levers[Index].Y);
			return;
		default:
			break;
		}

		Emit(EEvent::Whistle, Pl.X, Pl.Y - 36.0);
		if (!Dog.Active) { return; }
		if (Ghost.State == 1)
		{
			// The dog goes for it, barking, and it does not stay to argue.
			Ghost.State = 2;
			Linger = 0;
			Dog.Facing = Ghost.X > Dog.X ? 1 : -1;
			Dog.BarkFlash = 0.3;
			Dog.BarkTimer = 0.8;
			Emit(EEvent::Bark, Dog.X + Dog.Facing * 14.0, Dog.Y - 14.0);
			Emit(EEvent::GhostFlee, Ghost.X, Ghost.Y);
			return;
		}
		for (const FDogTaskDef& T : L.DogTasks)
		{
			if (Pl.X >= T.X0 && Pl.X <= T.X1 && !LeverOn[T.Lever])
			{
				Dog.Mode = EDogMode::TaskGo;
				Dog.Task = T.Lever;
				Dog.TargetX = L.Levers[T.Lever].X;
				return;
			}
		}
		if (Dog.Mode == EDogMode::Stay || Dog.Mode == EDogMode::GoStay)
		{
			Dog.Mode = EDogMode::Follow;
		}
		else
		{
			Dog.Mode = EDogMode::GoStay;   // "come here and stay"
			Dog.TargetX = Pl.X;
		}
	}

	void FSim::UpdateGhost()
	{
		FGhost& G = Ghost;
		if (!Level->bGhost || !Dog.Active) { return; }
		if (P.X > ProgressX + 30.0) { ProgressX = P.X; Linger = 0; }
		else { Linger += kStep; }
		G.Phase += kStep * 2.2;

		if (G.State == 0)
		{
			if (Linger > kGhostWait && P.Grounded)
			{
				G.State = 1;
				G.Side = -P.Facing;
				G.X = P.X + G.Side * 260.0;
				G.Y = P.Y - 34.0;
				G.Alpha = 0;
				Emit(EEvent::GhostAppear, G.X, G.Y);
			}
			return;
		}
		if (G.State == 1)
		{
			if (Linger < 1.0)
			{
				G.State = 2;   // the child got moving again
				Emit(EEvent::GhostFlee, G.X, G.Y);
				return;
			}
			G.Alpha = Approach(G.Alpha, 1.0, kStep * 0.7);
			G.X = Approach(G.X, P.X, kGhostSpeed * kStep);
			G.Y = Approach(G.Y, P.Y - 30.0 + std::sin(G.Phase) * 5.0, 60.0 * kStep);
			if (std::fabs(G.X - P.X) < 14.0 && std::fabs(G.Y - (P.Y - 28.0)) < 40.0)
			{
				G = FGhost();
				Kill(EDeath::Ghost);   // the light goes out
			}
			return;
		}
		// Fleeing: away and up, thinning to nothing.
		G.X += (G.X >= P.X ? 1.0 : -1.0) * 170.0 * kStep;
		G.Y -= 35.0 * kStep;
		G.Alpha -= kStep * 1.3;
		if (G.Alpha <= 0) { G = FGhost(); }
	}

	void FSim::UpdateMechanisms()
	{
		const FLevelDef& L = *Level;

		for (int I = 0; I < (int)L.Plates.size(); ++I)
		{
			const FPlateDef& Pd = L.Plates[I];
			bool bDown = Phase != EPhase::Dying && P.Grounded && std::fabs(P.Y - Pd.Y) < 3.0 && P.X >= Pd.X0 && P.X <= Pd.X1;
			for (const FCrate& C : Crates)
			{
				bDown = bDown || (std::fabs(C.Y + kCrateSize - Pd.Y) < 3.0 && C.X + kCrateSize * 0.5 >= Pd.X0 && C.X + kCrateSize * 0.5 <= Pd.X1);
			}
			bDown = bDown || (Dog.Active && Dog.Grounded && std::fabs(Dog.Y - Pd.Y) < 3.0 && Dog.X >= Pd.X0 && Dog.X <= Pd.X1);
			if (bDown != PlateDown[I])
			{
				PlateDown[I] = bDown;
				Emit(bDown ? EEvent::PlateDown : EEvent::PlateUp, (Pd.X0 + Pd.X1) * 0.5, Pd.Y);
			}
		}

		for (int I = 0; I < (int)Gates.size(); ++I)
		{
			FGate& G = Gates[I];
			bool bOpen = G.Latched;
			for (int Pi = 0; Pi < (int)L.Plates.size(); ++Pi) { bOpen = bOpen || (L.Plates[Pi].Gate == I && PlateDown[Pi]); }
			if (bOpen != G.Open)
			{
				G.Open = bOpen;
				const FGateDef& D = L.Gates[I];
				Emit(bOpen ? EEvent::GateOpen : EEvent::GateShut, (D.X0 + D.X1) * 0.5, D.Y1);
			}
			if (G.Open)
			{
				G.Amount = Approach(G.Amount, 1.0, kStep * 2.4);
			}
			else
			{
				// Never close on top of the child, a crate or the dog.
				const FGateDef& D = L.Gates[I];
				const FRect R = { D.X0, D.Y0, D.X1, D.Y1 };
				bool bClear = !R.Overlaps(P.Box()) && !(Dog.Active && R.Overlaps(Dog.Box()));
				for (const FCrate& C : Crates) { bClear = bClear && !R.Overlaps(C.Box()); }
				if (bClear || G.Amount < 0.55) { G.Amount = Approach(G.Amount, 0.0, kStep * 2.4); }
			}
		}

		// A carried tool goes where the child goes.
		if (P.Carry >= 0)
		{
			Items[P.Carry].X = P.X;
			Items[P.Carry].Y = P.Y;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Chapter two: machines, fire, rising water, the wolf

	double FSim::HazardPhase(int I) const
	{
		const FHazardDef& H = Level->Hazards[I];
		if (H.Period <= 0) { return 0; }
		const double U = Time / H.Period + H.Phase;
		return U - std::floor(U);
	}

	bool FSim::HazardActive(int I) const
	{
		const FHazardDef& H = Level->Hazards[I];
		return H.Kind != EHazard::Fire || H.Period <= 0 || HazardPhase(I) < H.Duty;
	}

	FRect FSim::HazardBox(int I) const
	{
		const FHazardDef& H = Level->Hazards[I];
		FRect R = { H.X0, H.Y0, H.X1, H.Y1 };
		const double U = HazardPhase(I);
		if (H.Kind == EHazard::Crusher)
		{
			// held up, a fast slam, held down a moment, a slow climb back
			const double D = U < 0.45 ? 0.0 : U < 0.55 ? ((U - 0.45) / 0.1) * ((U - 0.45) / 0.1) : U < 0.70 ? 1.0 : 1.0 - (U - 0.70) / 0.30;
			R.Y0 += D * H.Travel;
			R.Y1 += D * H.Travel;
		}
		else if (H.Kind == EHazard::Saw)
		{
			const double D = 0.5 - 0.5 * std::cos(2.0 * kPi * U);
			R.X0 += D * H.Travel;
			R.X1 += D * H.Travel;
		}
		return R;
	}

	void FSim::ResetDangers()
	{
		FloodY.assign(Level->Floods.size(), 0.0);
		for (int I = 0; I < (int)FloodY.size(); ++I) { FloodY[I] = Level->Floods[I].StartY; }
		FloodOn.assign(Level->Floods.size(), false);
		Chasers.assign(Level->Chasers.size(), FChaser());
	}

	void FSim::UpdateDangers()
	{
		const FLevelDef& L = *Level;
		for (int I = 0; I < (int)L.Hazards.size(); ++I)
		{
			const FHazardDef& H = L.Hazards[I];
			if (H.Period <= 0) { continue; }
			const double Now = HazardPhase(I);
			double Before = (Time - kStep) / H.Period + H.Phase;
			Before -= std::floor(Before);
			if (H.Kind == EHazard::Crusher && Before < 0.55 && Now >= 0.55) { Emit(EEvent::Crush, (H.X0 + H.X1) * 0.5, H.Y1 + H.Travel); }
			if (H.Kind == EHazard::Fire && H.Duty < 1.0 && Now < Before) { Emit(EEvent::FireOn, (H.X0 + H.X1) * 0.5, H.Y1); }
		}
		if (Phase != EPhase::Playing) { return; }

		for (int I = 0; I < (int)L.Floods.size(); ++I)
		{
			const FFloodDef& F = L.Floods[I];
			if (!FloodOn[I] && P.X >= F.TriggerX)
			{
				FloodOn[I] = true;
				Emit(EEvent::FloodStart, P.X, F.StartY);
			}
			if (FloodOn[I]) { FloodY[I] = std::max(F.EndY, FloodY[I] - F.Speed * kStep); }
		}

		for (int I = 0; I < (int)L.Chasers.size(); ++I)
		{
			const FChaserDef& D = L.Chasers[I];
			FChaser& C = Chasers[I];
			if (C.State == 0)
			{
				if (P.X >= D.TriggerX)
				{
					C.State = 1;
					C.X = D.StartX;
					C.Y = SurfaceAt(D.StartX, -kWorldBottom);
					Emit(EEvent::WolfHowl, C.X, C.Y - 20.0);
				}
			}
			else if (C.State == 1)
			{
				C.X += D.Speed * kStep;
				C.RunPhase += D.Speed * kStep / 105.0 * 2.0 * kPi;
				// It keeps to the ground, and clears a pit in its stride.
				const double Ground = SurfaceAt(C.X, C.Y - 60.0);
				if (Ground < kWorldBottom * 0.5 && Ground - C.Y < 260.0) { C.Y = Approach(C.Y, Ground, 520.0 * kStep); }
				if (C.X >= D.EndX)
				{
					C.State = 2;
					C.Timer = 0;
					Emit(EEvent::WolfGiveUp, C.X, C.Y - 20.0);
				}
			}
			else if (C.State == 2)
			{
				C.Timer += kStep;
				if (C.Timer > 0.9)
				{
					C.X -= 130.0 * kStep;   // it slinks back the way it came
					C.RunPhase += 130.0 * kStep / 60.0 * 2.0 * kPi;
				}
				if (C.Timer > 4.0) { C.State = 3; }
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The dog

	void FSim::PlaceDogNearPlayer()
	{
		Dog.X = P.X - P.Facing * 30.0;
		if (!IsFree({ Dog.X - FDog::W * 0.5, P.Y - FDog::H, Dog.X + FDog::W * 0.5, P.Y - 0.5 })) { Dog.X = P.X; }
		Dog.Y = P.Y;
		Dog.VX = Dog.VY = 0;
		Dog.Grounded = true;
		Dog.StuckTime = 0;
		Dog.LostTime = 0;
	}

	void FSim::UpdateDog()
	{
		FDog& D = Dog;
		if (!D.Active) { return; }
		const FLevelDef& L = *Level;
		D.BarkFlash -= kStep;
		D.BarkTimer -= kStep;
		const int PF = P.Facing;

		// What is it doing? Following, unless something needs pointing out.
		if (D.Mode == EDogMode::Follow || D.Mode == EDogMode::Point)
		{
			int Trap = -1;
			double Best = 200.0;
			for (int I = 0; I < (int)Traps.size(); ++I)
			{
				const double Ahead = (Traps[I].X - P.X) * PF;
				if (!Traps[I].Closed && Ahead > 12.0 && Ahead < Best && std::fabs(Traps[I].Y - P.Y) < 80.0)
				{
					Best = Ahead;
					Trap = I;
				}
			}
			int Hint = -1;
			if (Trap < 0 && P.IdleTime > 5.0)
			{
				for (int I = 0; I < (int)L.Hints.size(); ++I)
				{
					const FHintDef& H = L.Hints[I];
					if (P.X >= H.X0 && P.X <= H.X1 && (H.UntilGate < 0 || !Gates[H.UntilGate].Open)) { Hint = I; break; }
				}
			}
			if (Trap >= 0)
			{
				D.Mode = EDogMode::Point;
				D.PointTrap = Trap;
				D.TargetX = Traps[Trap].X - PF * 38.0;   // stops short of the teeth
			}
			else if (Hint >= 0)
			{
				D.Mode = EDogMode::Point;
				D.PointTrap = -1;
				D.TargetX = L.Hints[Hint].PointX;
			}
			else
			{
				D.Mode = EDogMode::Follow;
				D.PointTrap = -1;
				// It trails the child on whichever side it already is, and lets the child get a little ahead
				// before it gets up: no darting through the child's legs every time the child turns round.
				const double Side = P.X >= D.X ? 1.0 : -1.0;
				D.TargetX = std::fabs(P.X - D.X) > 64.0 ? P.X - Side * 40.0 : D.X;
			}
		}

		const double ToTarget = D.TargetX - D.X;
		const bool bArrived = std::fabs(ToTarget) < 7.0;
		double Want = 0;
		if (D.Mode != EDogMode::Stay && !bArrived)
		{
			Want = (ToTarget > 0 ? 1.0 : -1.0) * Clamp(std::fabs(ToTarget) * 2.4, 50.0, 275.0);   // a walk, a trot, then a run
		}
		D.VX = Approach(D.VX, Want, (Want != 0 ? 1100.0 : 1500.0) * kStep);
		if (Want != 0) { D.Facing = Want > 0 ? 1 : -1; }
		else if (D.Mode == EDogMode::Point && D.PointTrap >= 0) { D.Facing = Traps[D.PointTrap].X > D.X ? 1 : -1; }
		else if (D.Mode == EDogMode::Follow && std::fabs(P.X - D.X) > 12.0) { D.Facing = P.X > D.X ? 1 : -1; }   // it watches the child

		// Hop over what a dog would hop over: a step, a gap it can clear, an open trap.
		if (D.Grounded && Want != 0)
		{
			const int Dir = Want > 0 ? 1 : -1;
			const double Front = D.X + Dir * FDog::W * 0.5;
			bool bJump = D.StuckTime > 0.06;
			for (const FTrap& T : Traps)
			{
				const double Ahead = (T.X - D.X) * Dir;
				bJump = bJump || (!T.Closed && Ahead > 16.0 && Ahead < 46.0 && std::fabs(T.Y - D.Y) < 4.0);
			}
			if (!HasSupport(Front + Dir * 8.0, D.Y, 8.0, 40.0))
			{
				bool bReach = false;
				for (double DX = 12.0; DX <= 170.0 && !bReach; DX += 8.0) { bReach = HasSupport(Front + Dir * DX, D.Y, 70.0, 150.0); }
				if (bReach) { bJump = true; } else { D.VX = 0; }   // wait at the edge rather than fall
			}
			if (bJump)
			{
				D.VY = -500.0;
				D.Grounded = false;
			}
		}

		D.VY = std::min(D.VY + kGravity * kStep, kMaxFallSpeed);

		// Move, one axis at a time, against everything solid.
		const double StartX = D.X;
		{
			const double DX = D.VX * kStep;
			FRect B = D.Box();
			double Allowed = DX;
			bool bBlocked = false;
			if (DX != 0)
			{
				ForEachSolid(*this, false, true, -1, [&](const FSolid& S)
				{
					if (!(B.Y0 < S.R.Y1 - kEps && B.Y1 > S.R.Y0 + kEps)) { return; }
					const bool bEntering = DX > 0 ? (B.X1 <= S.R.X0 + kEps && B.X1 + DX > S.R.X0) : (B.X0 >= S.R.X1 - kEps && B.X0 + DX < S.R.X1);
					if (!bEntering) { return; }
					if (D.Grounded && S.R.Y0 >= B.Y1 - 6.0)   // step up a slope's stair
					{
						D.Y = S.R.Y0;
						B = D.Box();
						return;
					}
					const double Limit = DX > 0 ? S.R.X0 - B.X1 : S.R.X1 - B.X0;
					if (DX > 0 ? Limit < Allowed : Limit > Allowed) { Allowed = Limit; bBlocked = true; }
				});
				if (DX > 0) { Allowed = std::max(Allowed, 0.0); } else { Allowed = std::min(Allowed, 0.0); }
				D.X += Allowed;
				if (bBlocked) { D.VX = 0; }
			}
		}
		{
			auto MoveY = [&](double DY)
			{
				const FRect B = D.Box();
				double Allowed = DY;
				bool bLanded = false;
				ForEachSolid(*this, true, true, -1, [&](const FSolid& S)
				{
					if (!(B.X0 < S.R.X1 - kEps && B.X1 > S.R.X0 + kEps)) { return; }
					if (DY > 0 && B.Y1 <= S.R.Y0 + 0.5 && B.Y1 + DY >= S.R.Y0)
					{
						const double Dist = S.R.Y0 - B.Y1;
						if (Dist <= Allowed) { Allowed = Dist; bLanded = true; }
					}
					else if (DY < 0 && !S.OneWay && B.Y0 >= S.R.Y1 - kEps && B.Y0 + DY < S.R.Y1)
					{
						const double Dist = S.R.Y1 - B.Y0;
						if (Dist > Allowed) { Allowed = Dist; D.VY = 0; }
					}
				});
				D.Y += Allowed;
				if (bLanded) { D.VY = 0; }
				return bLanded;
			};
			const bool bWasGrounded = D.Grounded;
			const double Fall = D.VY;
			D.Grounded = MoveY(D.VY * kStep);
			if (!D.Grounded && bWasGrounded && Fall >= 0 && Fall < 60.0)
			{
				// trot down a hillside's steps without leaving the ground
				const double Y0 = D.Y;
				D.Grounded = MoveY(8.0);
				if (!D.Grounded) { D.Y = Y0; }
			}
		}
		const double Moved = std::fabs(D.X - StartX);
		D.StuckTime = (Want != 0 && Moved < 0.2 * std::fabs(Want) * kStep) ? D.StuckTime + kStep : 0.0;
		// One turn of the phase is one stride: short at a trot, long at a gallop, so the legs never whirr.
		D.RunPhase += Moved / Lerp(40.0, 90.0, SmoothStep(150.0, 230.0, std::fabs(D.VX))) * 2.0 * kPi;
		D.SitTime = (Want == 0 && D.Grounded) ? D.SitTime + kStep : 0.0;

		// Arrived somewhere it was sent.
		if (bArrived && D.Grounded)
		{
			if (D.Mode == EDogMode::GoStay)
			{
				D.Mode = EDogMode::Stay;
			}
			else if (D.Mode == EDogMode::TaskGo)
			{
				ThrowLever(D.Task, L.Levers[D.Task].X, L.Levers[D.Task].Y);
				D.Task = -1;
				D.Mode = EDogMode::Follow;
				D.BarkTimer = 0.25;
			}
			else if (D.Mode == EDogMode::Point && D.BarkTimer <= 0)
			{
				D.BarkTimer = 1.3;
				D.BarkFlash = 0.3;
				Emit(EEvent::Bark, D.X + D.Facing * 14.0, D.Y - 14.0);
			}
		}

		// Left behind, fallen in a hole, or stuck with no way round: catch the child up.
		const bool bFell = D.Y > kDeathY;
		if (D.Mode == EDogMode::Follow)
		{
			const bool bFar = std::fabs(P.X - D.X) > 520.0 || std::fabs(P.Y - D.Y) > 280.0;
			const bool bStuck = !bArrived && (D.StuckTime > 0.5 || (D.Grounded && Want != 0 && D.VX == 0));
			D.LostTime = (bFar || bStuck) ? D.LostTime + kStep : 0.0;
			if ((bFell || D.LostTime > 1.2) && P.Grounded && Phase == EPhase::Playing)
			{
				PlaceDogNearPlayer();
				Emit(EEvent::DogPoof, D.X, D.Y - 8.0);
			}
		}
		else if (bFell)
		{
			D.Mode = EDogMode::Follow;
			PlaceDogNearPlayer();
			Emit(EEvent::DogPoof, D.X, D.Y - 8.0);
		}
	}
}
