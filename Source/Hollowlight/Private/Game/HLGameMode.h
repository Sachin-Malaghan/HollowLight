// EMBERHOME: wires the controller and HUD; there is no pawn - the child lives in the 2D sim. (CLAUDE.md: Architecture)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HLGameMode.generated.h"

UCLASS()
class AHLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHLGameMode();
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
