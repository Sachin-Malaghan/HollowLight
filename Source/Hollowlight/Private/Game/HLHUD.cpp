// EMBERHOME: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#include "Game/HLHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Game/HLPlayerController.h"
#include "Game/HLSaveGame.h"
#include "Render/HLDraw.h"
#include "UI/HLUI.h"
#include "UObject/ConstructorHelpers.h"

AHLHUD::AHLHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> Roboto(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	Font = Roboto.Object;
}

void AHLHUD::BeginPlay()
{
	Super::BeginPlay();
	Grain = FHLWorldRenderer::CreateGrainTexture();
}

void AHLHUD::DrawHUD()
{
	Super::DrawHUD();
	AHLPlayerController* PC = Cast<AHLPlayerController>(PlayerOwner);
	if (!PC || !Canvas || !PC->Save || !PC->Game.Sim.Level) { return; }
	FHLGame& Game = PC->Game;

	FHLDraw D(Canvas);

	FHLRenderView V;
	V.Sim = &Game.Sim;
	const double ShakeX = FMath::Sin(Game.RealTime * 61.0) * Game.Shake * 0.6;
	const double ShakeY = FMath::Cos(Game.RealTime * 47.0) * Game.Shake * 0.4;
	V.CamX = Game.CamX + ShakeX;
	V.CamY = Game.CamY + ShakeY;
	V.Scale = D.ScreenH / HL::kViewHeight;
	V.ViewW = D.ScreenW / V.Scale;
	V.Time = Game.RealTime;
	V.PlayerX = Game.RenderX;
	V.PlayerY = Game.RenderY;
	V.Lightning = Game.Lightning;
	V.bGrain = PC->Save->bFilmGrain;
	V.bAttract = Game.bAttract;
	V.GrainTexture = Grain;
	Renderer.Draw(D, V);

	if (!Game.bHideUI)
	{
		FHLUiContext Ctx;
		Ctx.Font = Font;
		Ctx.bShowTouch = PC->ShouldShowTouch();
		Ctx.bTouchDevice = PC->IsTouchDevice();
		Ctx.bLeftDown = PC->bTouchLeft;
		Ctx.bRightDown = PC->bTouchRight;
		Ctx.bJumpDown = PC->bTouchJump;
		Ctx.bSlideDown = PC->bTouchSlide;
		Ctx.bMenuTouchDown = PC->bMenuTouchDown;
		Ctx.MenuTouch = PC->MenuTouch;
		Ctx.Safe = PC->SafeArea;
		FHLUI::Draw(D, Canvas, Game, Ctx);
	}
	else
	{
		Game.Buttons.Reset();
	}
	D.Flush();
}
