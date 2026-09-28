// HOLLOWLIGHT: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Render/HLWorldRenderer.h"
#include "HLHUD.generated.h"

class UFont;
class UTexture2D;

UCLASS()
class AHLHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHLHUD();
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	UPROPERTY() TObjectPtr<UFont> Font;
	UPROPERTY() TObjectPtr<UTexture2D> Grain;

	FHLWorldRenderer Renderer;
};
