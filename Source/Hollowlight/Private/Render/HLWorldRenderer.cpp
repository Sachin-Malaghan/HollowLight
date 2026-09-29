// EMBERHOME: procedural world renderer. (CLAUDE.md: Rendering)
#include "Render/HLWorldRenderer.h"

#include "Render/HLDraw.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

using namespace HL;

namespace
{
	constexpr uint32 kSkyTop = 0xc4c5bf;
	constexpr uint32 kSkyBottom = 0x5e5f5a;
	constexpr uint32 kLayerColor[4] = { 0xa3a49e, 0x7b7c77, 0x4a4b47, 0x22231f };
	constexpr double kLayerSpeed[4] = { 0.18, 0.38, 0.62, 0.85 };
	constexpr double kLayerCell[4] = { 58, 84, 120, 170 };
	constexpr double kLayerTrunk[4] = { 5.0, 8.5, 13.0, 21.0 };
	constexpr double kLayerHorizon[4] = { 268, 286, 304, 322 };
	constexpr double kFgSpeed = 1.35;
	constexpr uint32 kAmber = 0xffc46e;

	const FLinearColor Black(0, 0, 0, 1);

	// Multiply an sRGB colour by a brightness factor, optionally warming it (dawn).
	FLinearColor Shade(uint32 RGB, double Brightness, double Warm = 0, float Alpha = 1.f)
	{
		double R = ((RGB >> 16) & 0xff) / 255.0, G = ((RGB >> 8) & 0xff) / 255.0, B = (RGB & 0xff) / 255.0;
		R = FMath::Clamp(R * Brightness * (1.0 + 0.16 * Warm), 0.0, 1.0);
		G = FMath::Clamp(G * Brightness * (1.0 + 0.05 * Warm), 0.0, 1.0);
		B = FMath::Clamp(B * Brightness * (1.0 - 0.10 * Warm), 0.0, 1.0);
		FLinearColor C = FLinearColor::FromSRGBColor(FColor((uint8)(R * 255.0 + 0.5), (uint8)(G * 255.0 + 0.5), (uint8)(B * 255.0 + 0.5), 255));
		C.A = Alpha;
		return C;
	}

	FLinearColor Amber(float Intensity)
	{
		// Additive: colour carries the intensity, alpha 1.
		FLinearColor C = HLColor(kAmber);
		C.R *= Intensity; C.G *= Intensity; C.B *= Intensity;
		C.A = 1.f;
		return C;
	}

	double DawnWarmth(const FHLRenderView& V)
	{
		const FTheme& T = V.Sim->Level->Theme;
		if (T.Dawn <= 0) { return 0; }
		const double Progress = (V.PlayerX - V.Sim->Level->StartX) / FMath::Max(1.0, V.Sim->Level->GoalX - V.Sim->Level->StartX);
		double W = T.Dawn * SmoothStep(0.45, 1.0, Progress);
		if (V.Sim->Phase == EPhase::Won) { W = FMath::Min(1.0, W + V.Sim->PhaseTime * 0.2); }
		return W;
	}

	// 0 -> 1 over the seconds after the child reaches the lamp post.
	double GoalGlow(const FSim& S)
	{
		return SmoothStep(0.0, 3.0, S.PhaseTime);
	}

	double Flicker(double T)
	{
		return 1.0 + 0.055 * FMath::Sin(T * 13.1) + 0.035 * FMath::Sin(T * 7.3 + 1.2) + 0.025 * FMath::Sin(T * 23.7 + 0.4);
	}
}

// -------------------------------------------------------------------------------------------------

UTexture2D* FHLWorldRenderer::CreateGrainTexture()
{
	constexpr int32 N = 128;
	UTexture2D* Tex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
	if (!Tex) { return nullptr; }
	Tex->SRGB = false;
	Tex->Filter = TF_Nearest;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Wrap;
	Tex->NeverStream = true;
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	uint8* Data = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 I = 0; I < N * N; ++I)
	{
		const uint8 V = (uint8)(Hash32(0x51a3u, (uint32)I) & 0xff);
		Data[I * 4 + 0] = V;
		Data[I * 4 + 1] = V;
		Data[I * 4 + 2] = V;
		Data[I * 4 + 3] = 255;
	}
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

FVector2D FHLWorldRenderer::LanternPos(const FSim& Sim, double PlayerX, double PlayerY, double Time)
{
	const FPlayer& P = Sim.P;
	const double Speed = FMath::Clamp(FMath::Abs(P.VX) / kRunSpeed, 0.0, 1.0);
	const double Bob = P.Grounded ? FMath::Sin(P.RunPhase * 2.0) * 1.2 * Speed : 0.0;
	return FVector2D(PlayerX + P.Facing * 10.5, PlayerY - 13.0 + Bob);
}

void FHLWorldRenderer::Draw(FHLDraw& D, const FHLRenderView& V)
{
	if (!V.Sim || !V.Sim->Level) { return; }
	VisibleX0 = V.CamX - 40;
	VisibleX1 = V.CamX + V.ViewW + 40;

	D.SetBlend(SE_BLEND_Translucent);
	DrawSky(D, V);
	DrawShafts(D, V);
	for (int L = 0; L < 4; ++L)
	{
		DrawTreeLayer(D, V, L);
		if (L < 3) { DrawFogBand(D, V, L); }
	}

	if (V.Lightning > 0)
	{
		D.SetBlend(SE_BLEND_Translucent);
		D.ScreenRect(0, 0, D.ScreenW, D.ScreenH, FLinearColor(0.85f, 0.87f, 0.9f, (float)(0.55 * V.Lightning)));
	}

	DrawBackLight(D, V);
	DrawPlayLayer(D, V);
	DrawForeground(D, V);
	DrawRain(D, V);
	DrawLight(D, V);
	DrawVignetteAndGrain(D, V);
	D.Flush();
}

// -------------------------------------------------------------------------------------------------

void FHLWorldRenderer::DrawSky(FHLDraw& D, const FHLRenderView& V)
{
	const FTheme& T = V.Sim->Level->Theme;
	const double Warm = DawnWarmth(V);
	FLinearColor Top = Shade(kSkyTop, T.Brightness * (1.0 + 0.10 * Warm), Warm);
	FLinearColor Bottom = Shade(kSkyBottom, T.Brightness * (1.0 + 0.18 * Warm), Warm * 1.4);
	D.SetBlend(SE_BLEND_Opaque);
	// Sky gradient is fixed to the screen; the vertical camera only nudges it slightly.
	const double Shift = FMath::Clamp(-V.CamY * 0.15 * V.Scale, 0.0, D.ScreenH * 0.2);
	D.SetTransform(1, 0, 0);
	D.RectV(0, -Shift, D.ScreenW, D.ScreenH, Top, Bottom);
	D.Rect(0, D.ScreenH - 1, D.ScreenW, D.ScreenH + 2, Bottom);

	if (Warm > 0)
	{
		// A low warm glow on the horizon as the rain lifts.
		D.SetBlend(SE_BLEND_Additive);
		const double CX = D.ScreenW * 0.78, CY = D.ScreenH * 0.62;
		D.Glow(CX, CY, D.ScreenH * 0.9, Amber((float)(0.22 * Warm)), FLinearColor(0, 0, 0, 1), 40);
	}
	D.SetBlend(SE_BLEND_Translucent);
}

void FHLWorldRenderer::DrawShafts(FHLDraw& D, const FHLRenderView& V)
{
	const FTheme& T = V.Sim->Level->Theme;
	if (T.Shafts <= 0) { return; }
	D.SetBlend(SE_BLEND_Additive);
	D.SetTransform(V.Scale, V.CamX * 0.1, 0);
	const double Cell = 420;
	const int32 C0 = FMath::FloorToInt((V.CamX * 0.1 - 300) / Cell), C1 = FMath::FloorToInt((V.CamX * 0.1 + V.ViewW + 300) / Cell);
	for (int32 C = C0; C <= C1; ++C)
	{
		const uint32 S = Hash32(T.Seed * 131u + 7u, (uint32)(C + 100000));
		if (Hash01(S, 1) < 0.35) { continue; }
		const double X = C * Cell + Hash01(S, 2) * Cell;
		const double W = HashRange(S, 3, 26, 70);
		const double Slant = HashRange(S, 4, 110, 190);
		const double Pulse = 0.65 + 0.35 * FMath::Sin(V.Time * HashRange(S, 5, 0.15, 0.35) + Hash01(S, 6) * 6.28);
		const float A = (float)(0.075 * T.Shafts * T.Brightness * Pulse);
		const FLinearColor Top(A, A, A * 0.97f, 1), Bottom(0, 0, 0, 1);
		D.TriColors(FVector2D(X, -20), FVector2D(X + W, -20), FVector2D(X + W + Slant, 330), Top, Top, Bottom);
		D.TriColors(FVector2D(X, -20), FVector2D(X + W + Slant, 330), FVector2D(X + Slant, 330), Top, Bottom, Bottom);
	}
	D.SetBlend(SE_BLEND_Translucent);
}

void FHLWorldRenderer::DrawTreeLayer(FHLDraw& D, const FHLRenderView& V, int Layer)
{
	const FTheme& T = V.Sim->Level->Theme;
	const double Speed = kLayerSpeed[Layer];
	const double Warm = DawnWarmth(V) * (1.0 - Layer * 0.25);
	const FLinearColor Col = Shade(kLayerColor[Layer], T.Brightness * (Layer == 3 ? 1.0 : 1.0), Warm * 0.6);
	const double OX = V.CamX * Speed, OY = V.CamY * Speed;
	D.SetTransform(V.Scale, OX, OY);
	const double X0 = OX - 120, X1 = OX + V.ViewW + 120;
	const double Bottom = OY + kViewHeight + 20;

	// Rolling ground of this layer.
	const double Horizon = kLayerHorizon[Layer];
	const double Step = 24;
	for (double X = FMath::FloorToDouble(X0 / Step) * Step; X < X1; X += Step)
	{
		auto H = [&](double PX)
		{
			return Horizon + 7.0 * FMath::Sin(PX * 0.011 + Layer * 1.7 + T.Seed) + 4.0 * FMath::Sin(PX * 0.029 + Layer);
		};
		D.Quad(FVector2D(X, H(X)), FVector2D(X + Step, H(X + Step)), FVector2D(X + Step, Bottom), FVector2D(X, Bottom), Col);
	}

	const double Cell = kLayerCell[Layer];
	const int32 C0 = FMath::FloorToInt(X0 / Cell), C1 = FMath::FloorToInt(X1 / Cell);
	for (int32 C = C0; C <= C1; ++C)
	{
		const uint32 S = Hash32(T.Seed * 7919u + (uint32)Layer * 104729u, (uint32)(C + 1000000));
		if (Hash01(S, 1) < 0.18) { continue; }
		const double X = C * Cell + HashRange(S, 2, 0.1, 0.9) * Cell;
		const double W = kLayerTrunk[Layer] * HashRange(S, 3, 0.7, 1.45);
		const double Lean = HashRange(S, 4, -0.05, 0.05);
		const double Base = Horizon + 10;
		const double TopY = -80 - Hash01(S, 5) * 80;
		const double TopX = X + Lean * (Base - TopY);
		D.TaperLine(X, Base, TopX, TopY, W * 1.2, W * 0.62, Col);
		// root flare
		D.Tri(X - W * 1.3, Base, X + W * 1.3, Base, X, Base - W * 2.2, Col);

		const int32 NumBranches = 2 + (int32)(Hash01(S, 6) * 3.0);
		for (int32 B = 0; B < NumBranches; ++B)
		{
			const double BY = HashRange(S, 10 + B, 30, Base - 70);
			const double TT = (Base - BY) / (Base - TopY);
			const double BX = FMath::Lerp(X, TopX, TT);
			const double Side = (Hash01(S, 20 + B) < 0.5) ? -1.0 : 1.0;
			const double Len = HashRange(S, 30 + B, 22, 70) * (0.55 + Layer * 0.22);
			const double Rise = HashRange(S, 40 + B, 0.25, 0.7);
			const double BW = W * HashRange(S, 50 + B, 0.28, 0.42);
			const FVector2D A(BX, BY), K(BX + Side * Len * 0.55, BY - Len * 0.08), E(BX + Side * Len, BY - Len * Rise);
			D.Curve(A, K, E, BW, 0.8, Col, 6);
			if (Hash01(S, 60 + B) < 0.5)
			{
				const FVector2D Mid = A * 0.25 + K * 0.5 + E * 0.25;
				D.Curve(Mid, Mid + FVector2D(Side * Len * 0.2, -Len * 0.15), Mid + FVector2D(Side * Len * 0.3, -Len * 0.4), BW * 0.5, 0.6, Col, 4);
			}
		}
	}
}

void FHLWorldRenderer::DrawFogBand(FHLDraw& D, const FHLRenderView& V, int Layer)
{
	const FTheme& T = V.Sim->Level->Theme;
	const double Strength = T.Fog * (0.33 - Layer * 0.06);
	if (Strength <= 0) { return; }
	const double Speed = (kLayerSpeed[Layer] + kLayerSpeed[Layer + 1]) * 0.5;
	const double OY = V.CamY * Speed;
	D.SetTransform(V.Scale, V.CamX * Speed, OY);
	const double X0 = V.CamX * Speed - 50, X1 = V.CamX * Speed + V.ViewW + 50;
	const FLinearColor Pale = Shade(0xd2d3cd, T.Brightness, DawnWarmth(V) * 0.5, 0.f);
	FLinearColor Mid = Pale; Mid.A = (float)Strength;
	FLinearColor Low = Pale; Low.A = (float)(Strength * 0.75);
	const double Y = kLayerHorizon[Layer + 1] - 20;
	D.RectV(X0, Y - 130, X1, Y, Pale, Mid);
	D.RectV(X0, Y, X1, OY + kViewHeight + 20, Mid, Low);

	// Slow drifting wisps.
	const double Cell = 360;
	for (int32 C = FMath::FloorToInt((X0 - 400) / Cell); C <= FMath::FloorToInt((X1 + 400) / Cell); ++C)
	{
		const uint32 S = Hash32(T.Seed * 31u + (uint32)Layer * 977u, (uint32)(C + 50000));
		const double Drift = V.Time * HashRange(S, 1, 4, 12);
		const double CX = C * Cell + Hash01(S, 2) * Cell + Drift;
		const double CY = Y - HashRange(S, 3, 10, 60);
		FLinearColor In = Pale; In.A = (float)(Strength * 0.55);
		D.SetTransform(V.Scale, V.CamX * Speed, OY);
		// ellipse glow: squash by drawing in a scaled frame
		const double RX = HashRange(S, 4, 120, 260), RY = RX * 0.18;
		const int Segs = 24;
		for (int I = 0; I < Segs; ++I)
		{
			const double A0 = 2 * PI * I / Segs, A1 = 2 * PI * (I + 1) / Segs;
			D.TriColors(FVector2D(CX, CY), FVector2D(CX + FMath::Cos(A0) * RX, CY + FMath::Sin(A0) * RY),
			            FVector2D(CX + FMath::Cos(A1) * RX, CY + FMath::Sin(A1) * RY), In, Pale, Pale);
		}
	}
}

// -------------------------------------------------------------------------------------------------

void FHLWorldRenderer::DrawGroundTop(FHLDraw& D, const FHLRenderView& V, double X0, double X1, double Top, uint32 Seed)
{
	// Jagged grass fringe: blades seeded by their world position so they never change.
	const double A = FMath::Max(X0, VisibleX0), B = FMath::Min(X1, VisibleX1);
	if (A >= B) { return; }
	const double Step = 3.2;
	const double Start = FMath::FloorToDouble(A / Step) * Step;
	for (double X = Start; X < B; X += Step)
	{
		if (X < X0 || X + Step > X1 + 0.01) { continue; }
		const uint32 S = Hash32(Seed, (uint32)(int32)FMath::FloorToInt(X * 10.0) + 7000000u);
		const double H = HashRange(S, 1, 2.5, 9.0) * (Hash01(S, 2) < 0.08 ? 1.9 : 1.0);
		const double Lean = HashRange(S, 3, -2.5, 2.5) + FMath::Sin(V.Time * 1.3 + X * 0.05) * 0.6;
		const double W = HashRange(S, 4, 2.4, 4.4);
		D.Tri(X, Top + 1.5, X + W, Top + 1.5, X + W * 0.5 + Lean, Top - H, Black);
	}
	// A few pebbles and tufts sit proud of the edge.
	for (double X = FMath::FloorToDouble(A / 40.0) * 40.0; X < B; X += 40.0)
	{
		const uint32 S = Hash32(Seed ^ 0x9e37u, (uint32)(int32)FMath::FloorToInt(X) + 9000000u);
		if (Hash01(S, 1) < 0.25 && X > X0 + 6 && X < X1 - 6)
		{
			D.Ellipse(X + Hash01(S, 2) * 30, Top, HashRange(S, 3, 3, 7), HashRange(S, 4, 2, 3.5), Black, 10);
		}
	}
}

void FHLWorldRenderer::DrawPlayLayer(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FLevelDef& L = *S.Level;
	D.SetBlend(SE_BLEND_Translucent);
	D.SetTransform(V.Scale, V.CamX, V.CamY);
	const double Bottom = V.CamY + kViewHeight + 30;

	DrawProps(D, V);
	DrawLogs(D, V);
	DrawWater(D, V);

	for (int32 I = 0; I < (int32)L.Ground.size(); ++I)
	{
		const FGroundDef& G = L.Ground[I];
		if (G.X1 < VisibleX0 || G.X0 > VisibleX1) { continue; }
		D.Rect(G.X0, G.Top, G.X1, Bottom, Black);
		DrawGroundTop(D, V, G.X0, G.X1, G.Top, L.Theme.Seed * 17u + (uint32)I);
		// Ragged pit walls: roots and stones poking out of the sides.
		for (int Side = 0; Side < 2; ++Side)
		{
			const double EX = Side == 0 ? G.X0 : G.X1;
			const double Dir = Side == 0 ? -1.0 : 1.0;
			uint32 K = 0;
			for (double Y = G.Top + 4; Y < Bottom; ++K)
			{
				const uint32 H = Hash32((uint32)I * 131u + (uint32)Side, K + 3000u);
				const double Len = HashRange(H, 1, 5, 22);
				if (Hash01(H, 5) < 0.6)
				{
					// an uneven bulge of earth or stone
					const double Out = HashRange(H, 2, 0.8, 4.5);
					D.Tri(EX, Y, EX, Y + Len, EX + Dir * Out, Y + Len * HashRange(H, 3, 0.3, 0.7), Black);
				}
				if (Hash01(H, 4) < 0.1)
				{
					D.Curve(FVector2D(EX, Y), FVector2D(EX + Dir * 9, Y + 5), FVector2D(EX + Dir * 5, Y + 19), 2.2, 0.5, Black, 4);
				}
				Y += Len * HashRange(H, 6, 0.8, 1.4);
			}
		}
	}

	for (int32 I = 0; I < (int32)L.Blocks.size(); ++I)
	{
		const FBlockDef& B = L.Blocks[I];
		if (B.X1 < VisibleX0 || B.X0 > VisibleX1) { continue; }
		if (B.Y0 < 0)
		{
			// The end wall is an enormous trunk.
			const double CX = (B.X0 + B.X1) * 0.5, W = B.X1 - B.X0;
			D.TaperLine(CX, Bottom, CX + 6, V.CamY - 40, W * 1.25, W * 1.0, Black);
			D.Tri(B.X0 - 50, 302, B.X1 + 40, 302, CX, 200, Black);
			D.Curve(FVector2D(CX, 120), FVector2D(CX - 70, 110), FVector2D(CX - 150, 60), 18, 2, Black, 8);
			D.Curve(FVector2D(CX, 60), FVector2D(CX - 40, 40), FVector2D(CX - 95, -10), 12, 2, Black, 6);
		}
		else
		{
			// A mossy stone: rounded top corners.
			const double R = FMath::Min(8.0, (B.X1 - B.X0) * 0.25);
			D.Rect(B.X0, B.Y0 + R, B.X1, FMath::Min(B.Y1, Bottom), Black);
			D.Rect(B.X0 + R, B.Y0, B.X1 - R, B.Y0 + R, Black);
			D.Circle(B.X0 + R, B.Y0 + R, R, Black, 10);
			D.Circle(B.X1 - R, B.Y0 + R, R, Black, 10);
			DrawGroundTop(D, V, B.X0 + 3, B.X1 - 3, B.Y0, 0xb10cu + (uint32)I);
		}
	}

	DrawPlatforms(D, V);
	DrawCrumbles(D, V);

	for (const FCrate& C : S.Crates)
	{
		if (C.X + kCrateSize < VisibleX0 || C.X > VisibleX1) { continue; }
		// Pure black box; small notches at the corners and a rope loop so it reads as a crate.
		D.Rect(C.X + 1.5, C.Y, C.X + kCrateSize - 1.5, C.Y + kCrateSize, Black);
		D.Rect(C.X, C.Y + 1.5, C.X + kCrateSize, C.Y + kCrateSize - 1.5, Black);
		D.Curve(FVector2D(C.X + 18, C.Y), FVector2D(C.X + 28, C.Y - 10), FVector2D(C.X + 38, C.Y), 2.0, 2.0, Black, 6);
	}

	DrawTraps(D, V);
	DrawPlayer(D, V);
}

void FHLWorldRenderer::DrawProps(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FLevelDef& L = *S.Level;

	// Checkpoint cairns.
	for (int32 I = 0; I < (int32)L.Checkpoints.size(); ++I)
	{
		const double X = L.Checkpoints[I] + 12;
		if (X < VisibleX0 || X > VisibleX1) { continue; }
		const double Y = S.SurfaceAt(X, -kWorldBottom);
		D.Ellipse(X, Y - 3, 9, 5, Black, 12);
		D.Ellipse(X + 1, Y - 9, 6.5, 4, Black, 12);
		D.Ellipse(X - 0.5, Y - 14, 4.5, 3, Black, 10);
		D.Line(X, Y - 16, X, Y - 20, 1.4, Black);  // wick
	}

	// Lamp post (goal).
	{
		const double X = L.GoalX;
		if (X > VisibleX0 - 200 && X < VisibleX1 + 200)
		{
			const double Y = S.SurfaceAt(X, -kWorldBottom);
			D.Rect(X - 7, Y - 8, X + 7, Y + 2, Black);
			D.Rect(X - 5, Y - 12, X + 5, Y - 8, Black);
			D.TaperLine(X, Y - 10, X, Y - 118, 5.5, 3.5, Black);
			D.Circle(X, Y - 60, 3.4, Black, 10);
			D.Curve(FVector2D(X, Y - 116), FVector2D(X - 2, Y - 132), FVector2D(X - 16, Y - 128), 3.2, 2.4, Black, 8);
			const double LX = X - 18, LY = Y - 118;
			D.Line(LX, LY - 10, LX, LY - 5, 1.2, Black);
			D.Quad(FVector2D(LX - 7, LY - 5), FVector2D(LX + 7, LY - 5), FVector2D(LX + 5, LY + 9), FVector2D(LX - 5, LY + 9), Black);
			D.Tri(LX - 9, LY - 4, LX + 9, LY - 4, LX, LY - 11, Black);
			D.Rect(LX - 4, LY + 9, LX + 4, LY + 12, Black);

			// The last level ends at the edge of a village.
			if (L.Theme.Dawn > 0)
			{
				const double HX[3] = { X + 90, X + 190, X + 300 };
				for (int H = 0; H < 3; ++H)
				{
					const double W = 62 + H * 10, HH = 58 + (H % 2) * 14;
					D.Rect(HX[H], Y - HH, HX[H] + W, Y + 2, Black);
					D.Tri(HX[H] - 8, Y - HH, HX[H] + W + 8, Y - HH, HX[H] + W * 0.5, Y - HH - 34, Black);
					D.Rect(HX[H] + W * 0.7, Y - HH - 30, HX[H] + W * 0.7 + 8, Y - HH - 12, Black);
				}
			}
		}
	}
}

void FHLWorldRenderer::DrawTraps(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	for (const FTrap& T : S.Traps)
	{
		if (T.X < VisibleX0 || T.X > VisibleX1) { continue; }
		const double X = T.X, Y = T.Y;
		const double Half = kTrapW * 0.5;
		// Snap animation: 0 open -> 1 shut in 70 ms.
		const double K = T.Closed ? FMath::Clamp((S.Time - T.ClosedTime) / 0.07, 0.0, 1.0) : 0.0;
		D.Rect(X - Half * 0.55, Y - 3, X + Half * 0.55, Y + 1, Black);  // base plate
		D.Circle(X, Y - 3, 3, Black, 8);                                 // spring
		for (int Side = -1; Side <= 1; Side += 2)
		{
			// Each jaw rotates from lying flat (open) to upright (closed) about the centre hinge.
			const double Angle = FMath::Lerp(0.0, PI * 0.5 * 0.92, K);
			const double Len = Half;
			const double DX = Side * FMath::Cos(Angle), DY = -FMath::Sin(Angle);
			const FVector2D Hinge(X, Y - 3);
			const FVector2D Tip = Hinge + FVector2D(DX, DY) * Len;
			D.Line(Hinge.X, Hinge.Y, Tip.X, Tip.Y, 3.0, Black);
			// Teeth along the jaw: pointing up while open, toward the other jaw once shut.
			const FVector2D Along(DX, DY);
			for (int I = 1; I <= 4; ++I)
			{
				const FVector2D P0 = Hinge + Along * (Len * (I - 0.9) / 4.0);
				const FVector2D P1 = Hinge + Along * (Len * (I - 0.1) / 4.0);
				const FVector2D Mid = (P0 + P1) * 0.5;
				const FVector2D TipT = Mid + (K < 0.5 ? FVector2D(0, -6) : FVector2D(-Side * 6.0, 0));
				D.Tri(P0.X, P0.Y, P1.X, P1.Y, TipT.X, TipT.Y, Black);
			}
		}
	}
}

void FHLWorldRenderer::DrawLogs(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FLevelDef& L = *S.Level;
	for (int32 I = 0; I < (int32)L.Logs.size(); ++I)
	{
		const FLogDef& Def = L.Logs[I];
		if (Def.X + Def.Rope + 260 < VisibleX0 || Def.X - Def.Rope - 260 > VisibleX1) { continue; }
		// The branch it hangs from, growing out of a trunk to the left.
		const double TX = Def.X - 230;
		D.TaperLine(TX, 320, TX + 4, V.CamY - 40, 30, 22, Black);
		// A heavy bough that sags a little where the ropes hang, then lifts toward its tip.
		D.Curve(FVector2D(TX, Def.PivotY - 14), FVector2D(Def.X - 70, Def.PivotY + 14), FVector2D(Def.X + 150, Def.PivotY - 34), 18, 3, Black, 12);
		D.Curve(FVector2D(Def.X + 70, Def.PivotY - 10), FVector2D(Def.X + 88, Def.PivotY - 34), FVector2D(Def.X + 80, Def.PivotY - 60), 4, 1, Black, 5);
		D.Curve(FVector2D(Def.X - 110, Def.PivotY), FVector2D(Def.X - 130, Def.PivotY + 18), FVector2D(Def.X - 124, Def.PivotY + 34), 3, 0.8, Black, 4);
		D.Curve(FVector2D(TX + 8, Def.PivotY + 70), FVector2D(TX + 40, Def.PivotY + 60), FVector2D(TX + 70, Def.PivotY + 30), 7, 1.5, Black, 6);

		const FRect B = S.LogBox(I);
		const double CX = (B.X0 + B.X1) * 0.5, CY = (B.Y0 + B.Y1) * 0.5;
		// Two parallel ropes keep the log level as it swings.
		D.Line(Def.X - 20, Def.PivotY, CX - 20, CY - 6, 1.6, Black);
		D.Line(Def.X + 20, Def.PivotY - 3, CX + 20, CY - 6, 1.6, Black);
		D.Rect(B.X0 + kLogHalfH, B.Y0, B.X1 - kLogHalfH, B.Y1, Black);
		D.Circle(B.X0 + kLogHalfH, CY, kLogHalfH, Black, 14);
		D.Circle(B.X1 - kLogHalfH, CY, kLogHalfH, Black, 14);
		D.TaperLine(CX + 8, B.Y0 + 2, CX + 16, B.Y0 - 7, 4, 1.5, Black);   // snapped twig
		D.TaperLine(CX - 14, B.Y1 - 2, CX - 22, B.Y1 + 6, 3.5, 1.2, Black);
	}
}

void FHLWorldRenderer::DrawPlatforms(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	for (int32 I = 0; I < (int32)S.Platforms.size(); ++I)
	{
		const FRect B = S.PlatformBox(I);
		if (B.X1 < VisibleX0 - 100 || B.X0 > VisibleX1 + 100) { continue; }
		const double CY = B.Y0 + kPlatformThickness * 0.5;
		// A bough slung on two vines from the canopy.
		for (int Side = 0; Side < 2; ++Side)
		{
			const double VX = Side == 0 ? B.X0 + 10 : B.X1 - 10;
			const double Sway = FMath::Sin(V.Time * 1.1 + I + Side) * 6;
			D.Curve(FVector2D(VX, B.Y0), FVector2D(VX + Sway, (B.Y0 + V.CamY - 60) * 0.5), FVector2D(VX + Sway * 0.5, V.CamY - 60), 1.8, 1.4, Black, 6);
		}
		D.TaperLine(B.X0 + 4, CY, B.X1 - 4, CY - 1, kPlatformThickness, kPlatformThickness * 0.8, Black);
		D.Circle(B.X0 + 5, CY, kPlatformThickness * 0.5, Black, 10);
		D.Circle(B.X1 - 5, CY - 1, kPlatformThickness * 0.42, Black, 10);
		D.TaperLine(B.X1 - 20, B.Y0 + 2, B.X1 - 8, B.Y0 - 9, 3, 1, Black);
		D.TaperLine(B.X0 + 24, B.Y1 - 1, B.X0 + 18, B.Y1 + 8, 2.5, 0.8, Black);
	}
}

void FHLWorldRenderer::DrawCrumbles(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FLevelDef& L = *S.Level;
	for (int32 I = 0; I < (int32)S.Crumbles.size(); ++I)
	{
		const FCrumbleDef& Def = L.Crumbles[I];
		const FCrumble& C = S.Crumbles[I];
		if (Def.X1 < VisibleX0 || Def.X0 > VisibleX1) { continue; }
		if (C.State == 2 && C.Drop > 500) { continue; }
		double JX = 0, JY = 0;
		if (C.State == 1)
		{
			JX = FMath::Sin(S.Time * 90.0) * 1.2;
			JY = FMath::Cos(S.Time * 77.0) * 0.8;
		}
		const double Drop = C.State == 2 ? C.Drop : 0;
		const double Mid = (Def.X0 + Def.X1) * 0.5;
		for (int Half = 0; Half < 2; ++Half)
		{
			// Once fallen, the branch breaks in two and each half tumbles.
			const double HX0 = Half == 0 ? Def.X0 : Mid, HX1 = Half == 0 ? Mid : Def.X1;
			const double Rot = C.State == 2 ? (Half == 0 ? -1 : 1) * FMath::Min(1.2, Drop * 0.006) : 0.0;
			const FVector2D Pivot((HX0 + HX1) * 0.5 + JX, Def.Top + kCrumbleThickness * 0.5 + JY + Drop);
			auto Tr = [&](double X, double Y)
			{
				const FVector2D P(X - (HX0 + HX1) * 0.5, Y - (Def.Top + kCrumbleThickness * 0.5));
				return Pivot + FVector2D(P.X * FMath::Cos(Rot) - P.Y * FMath::Sin(Rot), P.X * FMath::Sin(Rot) + P.Y * FMath::Cos(Rot));
			};
			const int N = 6;
			for (int K = 0; K < N; ++K)
			{
				const double XA = FMath::Lerp(HX0, HX1, double(K) / N), XB = FMath::Lerp(HX0, HX1, double(K + 1) / N);
				const uint32 H = Hash32((uint32)I * 977u + (uint32)Half, (uint32)K);
				const double Under = Def.Top + kCrumbleThickness * HashRange(H, 1, 0.55, 1.1);
				D.Quad(Tr(XA, Def.Top), Tr(XB, Def.Top), Tr(XB, Under), Tr(XA, Def.Top + kCrumbleThickness * 0.8), Black);
			}
			// moss strands
			for (int K = 0; K < 3; ++K)
			{
				const uint32 H = Hash32((uint32)I * 31u + (uint32)Half, (uint32)K + 50u);
				const double MX = FMath::Lerp(HX0, HX1, HashRange(H, 1, 0.15, 0.85));
				const double Len = HashRange(H, 2, 8, 22);
				const FVector2D A = Tr(MX, Def.Top + kCrumbleThickness * 0.8), B = Tr(MX + FMath::Sin(V.Time * 2 + K) * 2, Def.Top + kCrumbleThickness + Len);
				D.TaperLine(A.X, A.Y, B.X, B.Y, 1.6, 0.4, Black);
			}
		}
	}
}

void FHLWorldRenderer::DrawWater(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FLevelDef& L = *S.Level;
	const double Bottom = V.CamY + kViewHeight + 30;
	for (const FWaterDef& W : L.Water)
	{
		if (W.X1 < VisibleX0 || W.X0 > VisibleX1) { continue; }
		D.Rect(W.X0, W.Surface, W.X1, Bottom, Black);
		// A faint grey sheen on the surface, broken by rain rings.
		const FLinearColor Sheen = HLGrey(0.55f, 0.85f);
		for (double X = W.X0; X < W.X1; X += 5)
		{
			const double Wave = FMath::Sin(X * 0.08 + V.Time * 2.2) * 0.6;
			const float A = (float)(0.45 + 0.4 * FMath::Sin(X * 0.21 - V.Time * 1.7));
			FLinearColor C = Sheen; C.A *= A;
			D.Rect(X, W.Surface - 0.8 + Wave, FMath::Min(W.X1, X + 4.5), W.Surface + 0.9 + Wave, C);
			FLinearColor Deep = C; Deep.A *= 0.25f;
			D.Rect(X, W.Surface + 5 + Wave * 0.5, FMath::Min(W.X1, X + 3), W.Surface + 5.8 + Wave * 0.5, Deep);
		}
		for (int K = 0; K < 5; ++K)
		{
			const double Period = 0.9;
			const double Phase = FMath::Fmod(V.Time + K * 0.37, Period) / Period;
			const uint32 H = Hash32((uint32)(int32)W.X0, (uint32)(FMath::FloorToInt((V.Time + K * 0.37) / Period) * 7 + K));
			const double RX = FMath::Lerp(W.X0 + 8, W.X1 - 8, Hash01(H, 1));
			const double R = 2 + Phase * 10;
			FLinearColor C = Sheen; C.A *= (float)(1.0 - Phase);
			D.Rect(RX - R, W.Surface - 0.5, RX - R + 3, W.Surface + 0.8, C);
			D.Rect(RX + R - 3, W.Surface - 0.5, RX + R, W.Surface + 0.8, C);
		}
	}
}

void FHLWorldRenderer::DrawPlayer(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FPlayer& P = S.P;
	const double X = V.PlayerX, Y = V.PlayerY;
	const double F = P.Facing;
	const double Speed = FMath::Clamp(FMath::Abs(P.VX) / kRunSpeed, 0.0, 1.0);
	const bool bAir = !P.Grounded && S.Phase != EPhase::Dying;
	const bool bDead = S.Phase == EPhase::Dying;
	const double Ph = P.RunPhase;
	const double Wind = S.Level->Theme.Wind;

	// When caught by a trap or a log the child crumples a little.
	const double Slump = bDead && (S.LastDeath == EDeath::Trap || S.LastDeath == EDeath::Log) ? FMath::Min(1.0, S.PhaseTime * 5.0) : 0.0;
	const double Bob = P.Grounded ? FMath::Abs(FMath::Sin(Ph)) * 1.6 * Speed : 0.0;
	const double HipY = Y - 17 + Bob * 0.5 + Slump * 6;
	const double ShoulderY = Y - 33 + Bob + Slump * 8;
	const double Lean = F * (Speed * 2.5 + (P.PushTimer > 0 ? 4.0 : 0.0));
	const FVector2D Hip(X, HipY), Shoulder(X + Lean, ShoulderY);

	// Legs
	for (int Leg = 0; Leg < 2; ++Leg)
	{
		double A;
		if (bAir) { A = (Leg == 0 ? 0.55 : -0.35) * F + (P.VY < 0 ? 0.1 : -0.1) * F; }
		else { A = FMath::Sin(Ph + Leg * PI) * 0.75 * Speed * F; }
		const double Knee = bAir ? 0.6 : FMath::Max(0.0, FMath::Sin(Ph + Leg * PI + 1.2)) * 0.8 * Speed;
		const FVector2D K = Hip + FVector2D(FMath::Sin(A) * 9.0, FMath::Cos(A) * 9.0);
		const double A2 = A - Knee * F;
		const FVector2D Foot = K + FVector2D(FMath::Sin(A2) * 9.0, FMath::Cos(A2) * 9.0 - Slump * 2);
		D.Line(Hip.X, Hip.Y, K.X, K.Y, 3.4, Black);
		D.Line(K.X, K.Y, Foot.X, Foot.Y, 3.0, Black);
		D.Circle(K.X, K.Y, 1.7, Black, 6);
		D.Ellipse(Foot.X + F * 1.5, Foot.Y - 0.8, 3.0, 1.6, Black, 8);
	}

	// Back arm (swings opposite the front arm)
	{
		const double A = bAir ? -1.9 * F : -FMath::Sin(Ph) * 0.8 * Speed * F - 0.15 * F;
		const FVector2D From = Shoulder + FVector2D(-F * 2.0, 1.0);
		const FVector2D To = From + FVector2D(FMath::Sin(A) * 12.0, FMath::Cos(A) * 12.0);
		D.Line(From.X, From.Y, To.X, To.Y, 2.6, Black);
	}

	// Tunic: trapezoid from the shoulders to the hem
	D.Quad(FVector2D(Shoulder.X - 5, ShoulderY - 1), FVector2D(Shoulder.X + 5, ShoulderY - 1), FVector2D(X + 8.5, HipY + 2), FVector2D(X - 8.5, HipY + 2), Black);
	D.Circle(Shoulder.X, ShoulderY + 1, 5, Black, 10);

	// Scarf: wraps the neck and trails behind, waving more when running or jumping.
	{
		const FVector2D Neck = Shoulder + FVector2D(0, -2.5);
		D.Ellipse(Neck.X, Neck.Y, 5.5, 2.8, Black, 12);
		const double Amp = 0.6 + 1.8 * Speed + (bAir ? 1.6 : 0.0) + Wind * 1.2;
		const double Stretch = 3.0 + 1.2 * Speed + (bAir ? 0.8 : 0.0) + Wind * 0.8;
		FVector2D Prev = Neck + FVector2D(-F * 2.0, 0.5);
		const int N = 8;
		for (int I = 1; I <= N; ++I)
		{
			const double T = double(I) / N;
			const double Droop = (1.0 - Speed * 0.7 - (bAir ? 0.3 : 0.0)) * I * 1.3 + (bAir && P.VY < 0 ? I * 0.8 : 0.0);
			const FVector2D Next = Neck + FVector2D(-F * (2.0 + I * Stretch), 0.5 + Droop + FMath::Sin(V.Time * 9.0 - I * 0.9) * Amp * T * 1.6);
			D.TaperLine(Prev.X, Prev.Y, Next.X, Next.Y, 3.6 * (1.0 - T * 0.55), 3.6 * (1.0 - (T + 1.0 / N) * 0.55), Black);
			Prev = Next;
		}
		D.Tri(Prev.X, Prev.Y - 1.5, Prev.X, Prev.Y + 1.5, Prev.X - F * 3.5, Prev.Y + 2.5, Black);
	}

	// Head with a spiky tuft of hair
	{
		const FVector2D Head = Shoulder + FVector2D(F * 1.2, -8.8 + Slump * 2);
		D.Circle(Head.X, Head.Y, 6.4, Black, 16);
		D.Tri(Head.X - F * 1.0, Head.Y - 5.5, Head.X + F * 3.0, Head.Y - 5.8, Head.X + F * 0.5, Head.Y - 11.5, Black);
		D.Tri(Head.X - F * 3.5, Head.Y - 4.5, Head.X, Head.Y - 6.0, Head.X - F * 4.5, Head.Y - 10.5, Black);
		D.Tri(Head.X - F * 5.5, Head.Y - 2.5, Head.X - F * 3.0, Head.Y - 5.0, Head.X - F * 8.5, Head.Y - 6.5, Black);
		D.Tri(Head.X + F * 2.0, Head.Y - 5.5, Head.X + F * 5.0, Head.Y - 4.0, Head.X + F * 6.0, Head.Y - 8.5, Black);
	}

	// Front arm holding the lantern
	{
		const FVector2D L = LanternPos(S, X, Y, V.Time) + FVector2D(0, Slump * 6);
		const FVector2D From = Shoulder + FVector2D(F * 2.0, 1.0);
		const FVector2D Hand = L + FVector2D(0, -7.5);
		const FVector2D Elbow = (From + Hand) * 0.5 + FVector2D(-F * 1.5, 2.0);
		D.Line(From.X, From.Y, Elbow.X, Elbow.Y, 2.6, Black);
		D.Line(Elbow.X, Elbow.Y, Hand.X, Hand.Y, 2.4, Black);
		// Lantern: bail, cap, glass frame, base. Its warm core is added in the light pass.
		D.Line(Hand.X, Hand.Y, L.X, L.Y - 5, 1.0, Black);
		D.Tri(L.X - 3.6, L.Y - 4.4, L.X + 3.6, L.Y - 4.4, L.X, L.Y - 7.0, Black);
		D.Rect(L.X - 3.2, L.Y - 4.6, L.X + 3.2, L.Y - 3.6, Black);
		D.Rect(L.X - 3.2, L.Y - 3.6, L.X - 2.3, L.Y + 3.6, Black);
		D.Rect(L.X + 2.3, L.Y - 3.6, L.X + 3.2, L.Y + 3.6, Black);
		D.Rect(L.X - 3.8, L.Y + 3.4, L.X + 3.8, L.Y + 4.8, Black);
	}
}

void FHLWorldRenderer::DrawForeground(FHLDraw& D, const FHLRenderView& V)
{
	const FTheme& T = V.Sim->Level->Theme;
	const double OX = V.CamX * kFgSpeed;
	D.SetBlend(SE_BLEND_Translucent);
	D.SetTransform(V.Scale, OX, V.CamY);
	const double X0 = OX - 200, X1 = OX + V.ViewW + 200;
	const double Bottom = V.CamY + kViewHeight + 10;

	// Occasional huge trunks right in front of the camera.
	const double Cell = 640;
	for (int32 C = FMath::FloorToInt(X0 / Cell); C <= FMath::FloorToInt(X1 / Cell); ++C)
	{
		const uint32 S = Hash32(T.Seed * 613u + 3u, (uint32)(C + 200000));
		if (Hash01(S, 1) > 0.42 * T.ForegroundDensity) { continue; }
		const double X = C * Cell + Hash01(S, 2) * Cell;
		const double W = HashRange(S, 3, 44, 88);
		D.TaperLine(X, Bottom + 20, X + HashRange(S, 4, -20, 20), V.CamY - 60, W * 1.15, W * 0.9, Black);
		if (Hash01(S, 5) < 0.6)
		{
			const double BY = V.CamY + HashRange(S, 6, 40, 150);
			const double Side = Hash01(S, 7) < 0.5 ? -1 : 1;
			D.Curve(FVector2D(X, BY), FVector2D(X + Side * 70, BY - 10), FVector2D(X + Side * 150, BY - 60), W * 0.3, 3, Black, 8);
		}
	}

	// Tall grass along the bottom edge.
	const double GCell = 150;
	for (int32 C = FMath::FloorToInt(X0 / GCell); C <= FMath::FloorToInt(X1 / GCell); ++C)
	{
		const uint32 S = Hash32(T.Seed * 419u + 11u, (uint32)(C + 300000));
		if (Hash01(S, 1) > 0.5 * T.ForegroundDensity) { continue; }
		const double X = C * GCell + Hash01(S, 2) * GCell;
		const int32 Blades = 6 + (int32)(Hash01(S, 3) * 10);
		for (int32 B = 0; B < Blades; ++B)
		{
			const double BX = X + HashRange(S, 10 + B, -26, 26);
			const double H = HashRange(S, 40 + B, 26, 78);
			const double Sway = FMath::Sin(V.Time * 1.4 + BX * 0.03) * (3.0 + T.Wind * 5.0);
			const double Lean = HashRange(S, 70 + B, -12, 12) + Sway;
			D.Curve(FVector2D(BX, Bottom + 4), FVector2D(BX + Lean * 0.3, Bottom - H * 0.55), FVector2D(BX + Lean, Bottom - H), 4.0, 0.5, Black, 4);
		}
	}
}

void FHLWorldRenderer::DrawRain(FHLDraw& D, const FHLRenderView& V)
{
	const FTheme& T = V.Sim->Level->Theme;
	const int32 Count = (int32)(170 * T.Rain);
	if (Count <= 0) { return; }
	D.SetBlend(SE_BLEND_Translucent);
	D.SetTransform(1, 0, 0);
	const double W = D.ScreenW, H = D.ScreenH;
	const double Slant = T.RainSlant + T.Wind * 0.18;
	const double Fall = 1.9 * H;  // screen heights per second -> px/s
	for (int32 I = 0; I < Count; ++I)
	{
		const uint32 S = Hash32(0xa11u, (uint32)I);
		const double Speed = Fall * HashRange(S, 1, 0.85, 1.25);
		const double Len = H * HashRange(S, 2, 0.028, 0.05);
		double Y = FMath::Fmod(Hash01(S, 3) * H * 1.3 + V.Time * Speed, H * 1.3) - H * 0.15;
		// parallax: nearer streaks drift with the camera more
		double X = Hash01(S, 4) * W * 1.4 + Y * Slant - V.CamX * V.Scale * HashRange(S, 5, 0.9, 1.3);
		X = FMath::Fmod(X, W * 1.4);
		if (X < 0) { X += W * 1.4; }
		X -= W * 0.2;
		const float A = (float)HashRange(S, 6, 0.18, 0.36);
		const FLinearColor C = HLColor(0x2a2b28, A);
		const double Thick = FMath::Max(1.0, H / 520.0);
		const FVector2D From(X, Y), To(X + Len * Slant, Y + Len);
		const FVector2D N(Thick * 0.5, 0);
		D.Quad(From - N, From + N, To + N, To - N, C);
	}
}

void FHLWorldRenderer::DrawBackLight(FHLDraw& D, const FHLRenderView& V)
{
	// The lantern warms the rain and mist behind the child. Drawn before the play layer so the
	// silhouettes stay pure black.
	const FSim& S = *V.Sim;
	D.SetTransform(1, 0, 0);
	D.SetBlend(SE_BLEND_Additive);
	if (S.Lantern > 0.01)
	{
		const FVector2D LW = LanternPos(S, V.PlayerX, V.PlayerY, V.Time);
		const FVector2D L((LW.X - V.CamX) * V.Scale, (LW.Y - V.CamY) * V.Scale);
		const float I = (float)(S.Lantern * Flicker(V.Time));
		const double R = 175.0 * S.Level->Theme.LightRadius * V.Scale;
		D.Glow(L.X, L.Y, R * 1.25, Amber(0.34f * I), FLinearColor(0, 0, 0, 1), 40);
		D.Glow(L.X, L.Y, R * 0.45, Amber(0.22f * I), FLinearColor(0, 0, 0, 1), 28);
	}
	if (S.Phase == EPhase::Won)
	{
		// The lamp post catches and its halo spreads through the mist, bigger and bigger.
		const double K = GoalGlow(S);
		const double X = S.Level->GoalX - 18, Y = S.SurfaceAt(S.Level->GoalX, -kWorldBottom) - 114;
		const FVector2D SP((X - V.CamX) * V.Scale, (Y - V.CamY) * V.Scale);
		const float G = (float)(FMath::Min(1.0, S.PhaseTime * 2.5) * Flicker(V.Time * 0.7));
		D.Glow(SP.X, SP.Y, (90 + 520 * K) * V.Scale, Amber(G * (float)(0.30 + 0.30 * K)), FLinearColor(0, 0, 0, 1), 48);
		D.Glow(SP.X, SP.Y, (30 + 110 * K) * V.Scale, Amber(G * 0.45f), FLinearColor(0, 0, 0, 1), 32);
	}
	D.SetBlend(SE_BLEND_Translucent);
}

void FHLWorldRenderer::DrawLight(FHLDraw& D, const FHLRenderView& V)
{
	const FSim& S = *V.Sim;
	const FTheme& T = S.Level->Theme;
	const FVector2D LW = LanternPos(S, V.PlayerX, V.PlayerY, V.Time);
	D.SetTransform(1, 0, 0);
	const FVector2D L = FVector2D((LW.X - V.CamX) * V.Scale, (LW.Y - V.CamY) * V.Scale);
	const double Fl = Flicker(V.Time);
	const double Lantern = S.Lantern;
	const double R = 175.0 * T.LightRadius * V.Scale * (0.55 + 0.45 * Lantern) * (0.97 + 0.03 * Fl);
	const double Diag = FMath::Sqrt(D.ScreenW * D.ScreenW + D.ScreenH * D.ScreenH) * 1.5;

	// Darkness everywhere except a soft hole around the lantern; it lifts as the lamp post lights.
	const double Lift = S.Phase == EPhase::Won ? 0.75 * GoalGlow(S) : 0.0;
	const float Dark = (float)FMath::Clamp((T.Darkness + (1.0 - Lantern) * 0.35) * (1.0 - Lift), 0.0, 0.95);
	const FLinearColor DarkC(0.004f, 0.004f, 0.003f, Dark), Clear(0.004f, 0.004f, 0.003f, 0.f);
	D.SetBlend(SE_BLEND_Translucent);
	const double RIn = R * 0.18 * Lantern;
	D.Ring(L.X, L.Y, RIn, R, Clear, DarkC, 48);
	D.Ring(L.X, L.Y, R, Diag, DarkC, DarkC, 48);

	// Warm glows, added on top.
	D.SetBlend(SE_BLEND_Additive);
	const float I = (float)(Lantern * Fl);
	if (I > 0.01f)
	{
		D.Glow(L.X, L.Y, 20 * V.Scale, Amber(0.42f * I), FLinearColor(0, 0, 0, 1), 24);
		D.Glow(L.X, L.Y, 7 * V.Scale, Amber(1.2f * I), Amber(0.3f * I), 16);
		D.Ellipse(L.X, L.Y + 0.4 * V.Scale, 1.8 * V.Scale, 2.6 * V.Scale, Amber(1.6f * I), 12);

		// Embers drifting up from the lantern.
		for (int32 E = 0; E < 7; ++E)
		{
			const double Life = 1.6 + E * 0.17;
			const double Age = FMath::Fmod(V.Time + E * 0.61, Life);
			const uint32 H = Hash32(0xe3u, (uint32)(FMath::FloorToInt((V.Time + E * 0.61) / Life) * 13 + E));
			const double K = Age / Life;
			const double EX = LW.X + HashRange(H, 1, -4, 4) + FMath::Sin(Age * 3.0 + E) * 5.0 * K + T.Wind * 18 * K;
			const double EY = LW.Y - 3 - Age * HashRange(H, 2, 16, 30);
			const FVector2D SP((EX - V.CamX) * V.Scale, (EY - V.CamY) * V.Scale);
			const float A = (float)(I * (1.0 - K) * (K < 0.1 ? K * 10.0 : 1.0));
			D.Glow(SP.X, SP.Y, 2.8 * V.Scale, Amber(0.7f * A), FLinearColor(0, 0, 0, 1), 8);
		}
	}

	// Lit checkpoint wicks.
	const FLevelDef& Lv = *S.Level;
	for (int32 C = 0; C <= S.CheckpointIndex && C < (int32)Lv.Checkpoints.size(); ++C)
	{
		const double X = Lv.Checkpoints[C] + 12;
		if (X < VisibleX0 || X > VisibleX1) { continue; }
		const double Y = S.SurfaceAt(X, -kWorldBottom) - 21;
		const FVector2D SP((X - V.CamX) * V.Scale, (Y - V.CamY) * V.Scale);
		const float F = (float)(0.8 + 0.2 * Flicker(V.Time * 1.3 + C));
		D.Glow(SP.X, SP.Y, 22 * V.Scale, Amber(0.25f * F), FLinearColor(0, 0, 0, 1), 16);
		D.Glow(SP.X, SP.Y, 3 * V.Scale, Amber(0.9f * F), FLinearColor(0, 0, 0, 1), 10);
	}

	// The lamp post: it only lights when the child reaches it, then glows bigger and bigger.
	if (S.Phase == EPhase::Won)
	{
		const double X = Lv.GoalX - 18, Y = S.SurfaceAt(Lv.GoalX, -kWorldBottom) - 114;
		const FVector2D SP((X - V.CamX) * V.Scale, (Y - V.CamY) * V.Scale);
		const double K = GoalGlow(S);
		const float G = (float)(FMath::Min(1.0, S.PhaseTime * 3.0) * Fl);
		D.Glow(SP.X, SP.Y, (14 + 14 * K) * V.Scale, Amber(G * 0.28f), FLinearColor(0, 0, 0, 1), 28);
		D.Glow(SP.X, SP.Y, 7 * V.Scale, Amber(G * 1.1f), Amber(G * 0.2f), 16);
		D.Ellipse(SP.X, SP.Y, 3.2 * V.Scale, 4.4 * V.Scale, Amber(G * 1.4f), 14);

		if (Lv.Theme.Dawn > 0)
		{
			// Windows in the village come on one by one.
			const double HX[3] = { Lv.GoalX + 90, Lv.GoalX + 190, Lv.GoalX + 300 };
			for (int H = 0; H < 3; ++H)
			{
				const float On = (float)SmoothStep(0.6 + H * 0.5, 1.0 + H * 0.5, S.PhaseTime);
				if (On <= 0) { continue; }
				const double W = 62 + H * 10, HH = 58 + (H % 2) * 14;
				const double WX = HX[H] + W * 0.3, WY = S.SurfaceAt(Lv.GoalX, -kWorldBottom) - HH * 0.6;
				const FVector2D P0((WX - V.CamX) * V.Scale, (WY - V.CamY) * V.Scale);
				D.ScreenRect(P0.X, P0.Y, P0.X + 12 * V.Scale, P0.Y + 14 * V.Scale, Amber(0.9f * On));
				D.Glow(P0.X + 6 * V.Scale, P0.Y + 7 * V.Scale, 24 * V.Scale, Amber(0.16f * On), FLinearColor(0, 0, 0, 1), 20);
			}
		}
	}

	// Death / respawn fade.
	D.SetBlend(SE_BLEND_Translucent);
	if (S.Fade > 0.001)
	{
		D.ScreenRect(0, 0, D.ScreenW, D.ScreenH, FLinearColor(0, 0, 0, (float)S.Fade));
	}
}

void FHLWorldRenderer::DrawVignetteAndGrain(FHLDraw& D, const FHLRenderView& V)
{
	D.SetTransform(1, 0, 0);
	D.SetBlend(SE_BLEND_Translucent);
	const double CX = D.ScreenW * 0.5, CY = D.ScreenH * 0.5;
	const double Diag = FMath::Sqrt(CX * CX + CY * CY);
	const FLinearColor Clear(0, 0, 0, 0), Edge(0, 0, 0, 0.62f);
	// An elliptical vignette: draw the ring in a frame squashed to the screen's aspect.
	const double AX = CX / Diag, AY = CY / Diag;
	const int Segs = 48;
	for (int I = 0; I < Segs; ++I)
	{
		const double A0 = 2 * PI * I / Segs, A1 = 2 * PI * (I + 1) / Segs;
		auto P = [&](double A, double R) { return FVector2D(CX + FMath::Cos(A) * R * AX * 1.42, CY + FMath::Sin(A) * R * AY * 1.42); };
		const double R0 = Diag * 0.55, R1 = Diag * 1.0, R2 = Diag * 2.0;
		D.TriColors(P(A0, R0), P(A0, R1), P(A1, R1), Clear, Edge, Edge);
		D.TriColors(P(A0, R0), P(A1, R1), P(A1, R0), Clear, Edge, Clear);
		D.TriColors(P(A0, R1), P(A0, R2), P(A1, R2), Edge, Edge, Edge);
		D.TriColors(P(A0, R1), P(A1, R2), P(A1, R1), Edge, Edge, Edge);
	}

	if (V.bGrain && V.GrainTexture && V.GrainTexture->GetResource())
	{
		// Tiled noise, re-rolled 24 times a second. Low-alpha translucent approximates overlay.
		const double Tile = 128.0 * FMath::Max(1.0, D.ScreenH / 720.0) * 1.5;
		const int32 Frame = FMath::FloorToInt(V.Time * 24.0);
		const double U0 = Hash01(0x9a1u, (uint32)Frame), V0 = Hash01(0x9a2u, (uint32)Frame);
		D.ScreenTexture(V.GrainTexture->GetResource(), 0, 0, D.ScreenW, D.ScreenH, U0, V0, U0 + D.ScreenW / Tile, V0 + D.ScreenH / Tile,
			FLinearColor(1, 1, 1, 0.075f), SE_BLEND_Translucent);
	}
}
