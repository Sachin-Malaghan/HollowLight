// EMBERHOME: batched 2D triangle drawing on a UCanvas. (CLAUDE.md: Rendering)
// Everything in the game is drawn from these primitives - no textures except the grain tile.
#pragma once

#include "CoreMinimal.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"

class FTexture;

class FHLDraw
{
public:
	explicit FHLDraw(UCanvas* InCanvas);
	~FHLDraw();

	// World -> screen: Screen = (World - Origin) * Scale. Parallax layers set their own origin.
	void SetTransform(double InScale, double InOriginX, double InOriginY);
	FVector2D ToScreen(double X, double Y) const { return FVector2D((X - OriginX) * Scale, (Y - OriginY) * Scale); }
	double GetScale() const { return Scale; }

	void SetBlend(ESimpleElementBlendMode Mode);
	void Flush();

	// Primitives in the current transform's units.
	void Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color);
	void TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);
	void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color);
	void Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color);
	void RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& Top, const FLinearColor& Bottom);
	void Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments = 20);
	void Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments = 20, double Rotation = 0);
	void Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments = 28);
	void Ring(double CX, double CY, double R0, double R1, const FLinearColor& C0, const FLinearColor& C1, int Segments = 40);
	void Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color);
	void TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color);
	// Quadratic curve A -> (control K) -> B with width tapering WA -> WB.
	void Curve(const FVector2D& A, const FVector2D& K, const FVector2D& B, double WA, double WB, const FLinearColor& Color, int Segments = 8);
	void ConvexPoly(const TArray<FVector2D>& Points, const FLinearColor& Color);

	// Screen-space (pixels) helpers for UI and overlays.
	void ScreenRect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color);
	void ScreenTexture(const FTexture* Texture, double X0, double Y0, double X1, double Y1, double U0, double V0, double U1, double V1, const FLinearColor& Color, ESimpleElementBlendMode Mode);

	UCanvas* Canvas;
	double ScreenW = 0, ScreenH = 0;

private:
	void Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);

	TArray<FCanvasUVTri> Batch;
	ESimpleElementBlendMode Blend = SE_BLEND_Translucent;
	double Scale = 1, OriginX = 0, OriginY = 0;
};

// sRGB hex -> linear colour with alpha.
FLinearColor HLColor(uint32 RGB, float Alpha = 1.f);
FLinearColor HLGrey(float SRGB01, float Alpha = 1.f);
