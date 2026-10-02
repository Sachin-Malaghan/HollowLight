// EMBERHOME: draws the world - sky, parallax forest (or warehouse / railway), fog, the play layer, rain, the lantern's light,
// vignette and film grain - entirely from procedural shapes. (CLAUDE.md: Rendering)
#pragma once

#include "CoreMinimal.h"
#include "Core/HLSim.h"

class FHLDraw;
class UTexture2D;

struct FHLRenderView
{
	const HL::FSim* Sim = nullptr;
	double CamX = 0, CamY = 0;       // world position of the screen's top-left
	double Scale = 1;                 // pixels per world unit (screen height / 400)
	double ViewW = 711;               // world units across the screen
	double Time = 0;                  // real seconds (animation)
	double PlayerX = 0, PlayerY = 0;  // interpolated player feet
	double Lightning = 0;             // 0..1 current flash
	double Shake = 0;                 // screen shake amplitude (world units)
	bool bGrain = true;
	bool bAttract = false;
	UTexture2D* GrainTexture = nullptr;
};

class FHLWorldRenderer
{
public:
	void Draw(FHLDraw& D, const FHLRenderView& V);

	// Lantern position (world) for the child standing at the view's player position.
	static FVector2D LanternPos(const HL::FSim& Sim, double PlayerX, double PlayerY, double Time);

	static UTexture2D* CreateGrainTexture();

private:
	void DrawSky(FHLDraw& D, const FHLRenderView& V);
	void DrawShafts(FHLDraw& D, const FHLRenderView& V);
	void DrawTreeLayer(FHLDraw& D, const FHLRenderView& V, int Layer);
	void DrawBuiltLayer(FHLDraw& D, const FHLRenderView& V, int Layer, const FLinearColor& Col, double OX, double OY);
	void DrawFogBand(FHLDraw& D, const FHLRenderView& V, int Layer);
	void DrawPlayLayer(FHLDraw& D, const FHLRenderView& V);
	void DrawGroundTop(FHLDraw& D, const FHLRenderView& V, double X0, double X1, double Top, uint32 Seed);
	void DrawTraps(FHLDraw& D, const FHLRenderView& V);
	void DrawLogs(FHLDraw& D, const FHLRenderView& V);
	void DrawPlatforms(FHLDraw& D, const FHLRenderView& V);
	void DrawCrumbles(FHLDraw& D, const FHLRenderView& V);
	void DrawWater(FHLDraw& D, const FHLRenderView& V);
	void DrawProps(FHLDraw& D, const FHLRenderView& V);
	void DrawSlopes(FHLDraw& D, const FHLRenderView& V);
	void DrawMechanisms(FHLDraw& D, const FHLRenderView& V);
	void DrawDog(FHLDraw& D, const FHLRenderView& V);
	void DrawGhost(FHLDraw& D, const FHLRenderView& V);
	void DrawRelics(FHLDraw& D, const FHLRenderView& V, const FLinearColor& Col, const FLinearColor& Gap, double X0, double X1, double Ground);
	void DrawPlayer(FHLDraw& D, const FHLRenderView& V);
	void DrawForeground(FHLDraw& D, const FHLRenderView& V);
	void DrawRain(FHLDraw& D, const FHLRenderView& V);
	void DrawBackLight(FHLDraw& D, const FHLRenderView& V);
	void DrawLight(FHLDraw& D, const FHLRenderView& V);
	void DrawVignetteAndGrain(FHLDraw& D, const FHLRenderView& V);

	double VisibleX0 = 0, VisibleX1 = 0;
};
