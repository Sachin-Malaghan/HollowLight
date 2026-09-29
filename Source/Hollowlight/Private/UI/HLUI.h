// EMBERHOME: menus, HUD overlay, touch controls and the procedural serif title. (CLAUDE.md: UI)
#pragma once

#include "CoreMinimal.h"

class FHLDraw;
class FHLGame;
class UCanvas;
class UFont;

// On-screen touch buttons. Shared by the controller (hit testing) and the UI (drawing).
struct FHLTouchLayout
{
	FVector2D Left, Right, Jump;   // centres (pixels)
	double Radius = 0, JumpRadius = 0;

	static FHLTouchLayout Compute(double W, double H, const FVector4& Safe);
	bool HitLeft(const FVector2D& P) const;
	bool HitRight(const FVector2D& P) const;
	bool HitJump(const FVector2D& P) const;
};

struct FHLUiContext
{
	UFont* Font = nullptr;
	bool bShowTouch = false;
	bool bTouchDevice = false;
	bool bLeftDown = false, bRightDown = false, bJumpDown = false;
	bool bMenuTouchDown = false;               // a finger is on the screen outside the control pads
	FVector2D MenuTouch = FVector2D::ZeroVector;
	FVector4 Safe = FVector4(0, 0, 0, 0);   // left, top, right, bottom insets (pixels)
};

class FHLUI
{
public:
	static void Draw(FHLDraw& D, UCanvas* Canvas, FHLGame& Game, const FHLUiContext& Ctx);

	// Draws a word in the procedural serif used for the title. Letters: A-Z subset used by the game.
	static void DrawSerifWord(FHLDraw& D, const FString& Word, double CenterX, double BaselineY, double Height, double Tracking, const FLinearColor& Color);
	static double SerifWordWidth(const FString& Word, double Height, double Tracking);
};
