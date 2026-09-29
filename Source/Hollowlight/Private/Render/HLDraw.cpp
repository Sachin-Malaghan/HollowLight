// EMBERHOME: batched 2D triangle drawing on a UCanvas. (CLAUDE.md: Rendering)
#include "Render/HLDraw.h"

#include "CanvasItem.h"
#include "RenderUtils.h"
#include "TextureResource.h"

FLinearColor HLColor(uint32 RGB, float Alpha)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xff, (RGB >> 8) & 0xff, RGB & 0xff, 255));
	C.A = Alpha;
	return C;
}

FLinearColor HLGrey(float SRGB01, float Alpha)
{
	const uint8 V = (uint8)FMath::Clamp(FMath::RoundToInt(SRGB01 * 255.f), 0, 255);
	FLinearColor C = FLinearColor::FromSRGBColor(FColor(V, V, V, 255));
	C.A = Alpha;
	return C;
}

FHLDraw::FHLDraw(UCanvas* InCanvas)
	: Canvas(InCanvas)
{
	ScreenW = Canvas->ClipX;
	ScreenH = Canvas->ClipY;
	Batch.Reserve(4096);
}

FHLDraw::~FHLDraw()
{
	Flush();
}

void FHLDraw::SetTransform(double InScale, double InOriginX, double InOriginY)
{
	Scale = InScale;
	OriginX = InOriginX;
	OriginY = InOriginY;
}

void FHLDraw::SetBlend(ESimpleElementBlendMode Mode)
{
	if (Mode != Blend)
	{
		Flush();
		Blend = Mode;
	}
}

void FHLDraw::Flush()
{
	if (Batch.Num() == 0 || !Canvas || !Canvas->Canvas) { Batch.Reset(); return; }
	FCanvasTriangleItem Item(Batch, GWhiteTexture);
	Item.BlendMode = Blend;
	Canvas->DrawItem(Item);
	Batch.Reset();
}

void FHLDraw::Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	FCanvasUVTri& T = Batch.AddDefaulted_GetRef();
	T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
	T.V0_UV = T.V1_UV = T.V2_UV = FVector2D(0.5, 0.5);
	T.V0_Color = CA; T.V1_Color = CB; T.V2_Color = CC;
	if (Batch.Num() >= 8000) { Flush(); }
}

void FHLDraw::Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color)
{
	Push(ToScreen(AX, AY), ToScreen(BX, BY), ToScreen(CX, CY), Color, Color, Color);
}

void FHLDraw::TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	Push(ToScreen(A.X, A.Y), ToScreen(B.X, B.Y), ToScreen(C.X, C.Y), CA, CB, CC);
}

void FHLDraw::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color)
{
	const FVector2D SA = ToScreen(A.X, A.Y), SB = ToScreen(B.X, B.Y), SC = ToScreen(C.X, C.Y), SD = ToScreen(D.X, D.Y);
	Push(SA, SB, SC, Color, Color, Color);
	Push(SA, SC, SD, Color, Color, Color);
}

void FHLDraw::Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color)
{
	RectV(X0, Y0, X1, Y1, Color, Color);
}

void FHLDraw::RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& Top, const FLinearColor& Bottom)
{
	const FVector2D A = ToScreen(X0, Y0), B = ToScreen(X1, Y0), C = ToScreen(X1, Y1), D = ToScreen(X0, Y1);
	Push(A, B, C, Top, Top, Bottom);
	Push(A, C, D, Top, Bottom, Bottom);
}

void FHLDraw::Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments)
{
	Ellipse(CX, CY, R, R, Color, Segments, 0);
}

void FHLDraw::Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments, double Rotation)
{
	const FVector2D C = ToScreen(CX, CY);
	const double CR = FMath::Cos(Rotation), SR = FMath::Sin(Rotation);
	auto P = [&](int I)
	{
		const double A = 2.0 * PI * I / Segments;
		const double LX = FMath::Cos(A) * RX, LY = FMath::Sin(A) * RY;
		return ToScreen(CX + LX * CR - LY * SR, CY + LX * SR + LY * CR);
	};
	FVector2D Prev = P(0);
	for (int I = 1; I <= Segments; ++I)
	{
		const FVector2D Next = P(I);
		Push(C, Prev, Next, Color, Color, Color);
		Prev = Next;
	}
}

void FHLDraw::Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments)
{
	// Concentric rings with a (1 - r)^2 falloff, so the edge fades out with no visible rim.
	static const double Radii[] = { 0.0, 0.1, 0.24, 0.42, 0.62, 0.82, 1.0 };
	constexpr int NumRings = UE_ARRAY_COUNT(Radii);
	FLinearColor Cols[NumRings];
	for (int K = 0; K < NumRings; ++K)
	{
		const float F = (float)FMath::Square(1.0 - Radii[K]);
		Cols[K] = Center * F + Edge * (1.f - F);
	}
	for (int I = 0; I < Segments; ++I)
	{
		const double A0 = 2.0 * PI * I / Segments, A1 = 2.0 * PI * (I + 1) / Segments;
		const double C0 = FMath::Cos(A0), S0 = FMath::Sin(A0), C1 = FMath::Cos(A1), S1 = FMath::Sin(A1);
		for (int K = 0; K + 1 < NumRings; ++K)
		{
			const double RA = R * Radii[K], RB = R * Radii[K + 1];
			const FVector2D In0 = ToScreen(CX + C0 * RA, CY + S0 * RA), In1 = ToScreen(CX + C1 * RA, CY + S1 * RA);
			const FVector2D Out0 = ToScreen(CX + C0 * RB, CY + S0 * RB), Out1 = ToScreen(CX + C1 * RB, CY + S1 * RB);
			if (K == 0) { Push(In0, Out0, Out1, Cols[0], Cols[1], Cols[1]); continue; }
			Push(In0, Out0, Out1, Cols[K], Cols[K + 1], Cols[K + 1]);
			Push(In0, Out1, In1, Cols[K], Cols[K + 1], Cols[K]);
		}
	}
}

void FHLDraw::Ring(double CX, double CY, double R0, double R1, const FLinearColor& C0, const FLinearColor& C1, int Segments)
{
	for (int I = 0; I < Segments; ++I)
	{
		const double A0 = 2.0 * PI * I / Segments, A1 = 2.0 * PI * (I + 1) / Segments;
		const FVector2D In0 = ToScreen(CX + FMath::Cos(A0) * R0, CY + FMath::Sin(A0) * R0);
		const FVector2D In1 = ToScreen(CX + FMath::Cos(A1) * R0, CY + FMath::Sin(A1) * R0);
		const FVector2D Out0 = ToScreen(CX + FMath::Cos(A0) * R1, CY + FMath::Sin(A0) * R1);
		const FVector2D Out1 = ToScreen(CX + FMath::Cos(A1) * R1, CY + FMath::Sin(A1) * R1);
		Push(In0, Out0, Out1, C0, C1, C1);
		Push(In0, Out1, In1, C0, C1, C0);
	}
}

void FHLDraw::Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color)
{
	TaperLine(AX, AY, BX, BY, Width, Width, Color);
}

void FHLDraw::TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color)
{
	FVector2D D(BX - AX, BY - AY);
	const double Len = D.Size();
	if (Len < 1e-6) { return; }
	const FVector2D N(-D.Y / Len, D.X / Len);
	Quad(FVector2D(AX, AY) + N * (WA * 0.5), FVector2D(BX, BY) + N * (WB * 0.5),
	     FVector2D(BX, BY) - N * (WB * 0.5), FVector2D(AX, AY) - N * (WA * 0.5), Color);
}

void FHLDraw::Curve(const FVector2D& A, const FVector2D& K, const FVector2D& B, double WA, double WB, const FLinearColor& Color, int Segments)
{
	auto P = [&](double T) { const double U = 1.0 - T; return A * (U * U) + K * (2.0 * U * T) + B * (T * T); };
	FVector2D Prev = P(0);
	for (int I = 1; I <= Segments; ++I)
	{
		const double T0 = double(I - 1) / Segments, T1 = double(I) / Segments;
		const FVector2D Next = P(T1);
		TaperLine(Prev.X, Prev.Y, Next.X, Next.Y, FMath::Lerp(WA, WB, T0), FMath::Lerp(WA, WB, T1), Color);
		// round the joint so thick curves don't crack
		if (I < Segments) { Circle(Next.X, Next.Y, FMath::Lerp(WA, WB, T1) * 0.5, Color, 8); }
		Prev = Next;
	}
}

void FHLDraw::ConvexPoly(const TArray<FVector2D>& Points, const FLinearColor& Color)
{
	if (Points.Num() < 3) { return; }
	const FVector2D P0 = ToScreen(Points[0].X, Points[0].Y);
	for (int I = 1; I + 1 < Points.Num(); ++I)
	{
		Push(P0, ToScreen(Points[I].X, Points[I].Y), ToScreen(Points[I + 1].X, Points[I + 1].Y), Color, Color, Color);
	}
}

void FHLDraw::ScreenRect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color)
{
	const FVector2D A(X0, Y0), B(X1, Y0), C(X1, Y1), D(X0, Y1);
	Push(A, B, C, Color, Color, Color);
	Push(A, C, D, Color, Color, Color);
}

void FHLDraw::ScreenTexture(const FTexture* Texture, double X0, double Y0, double X1, double Y1, double U0, double V0, double U1, double V1, const FLinearColor& Color, ESimpleElementBlendMode Mode)
{
	Flush();
	if (!Texture || !Canvas) { return; }
	FCanvasTileItem Tile(FVector2D(X0, Y0), Texture, FVector2D(X1 - X0, Y1 - Y0), FVector2D(U0, V0), FVector2D(U1, V1), Color);
	Tile.BlendMode = Mode;
	Canvas->DrawItem(Tile);
}
