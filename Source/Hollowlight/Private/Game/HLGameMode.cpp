// HOLLOWLIGHT: game mode. (CLAUDE.md: Architecture)
#include "Game/HLGameMode.h"

#include "Game/HLHUD.h"
#include "Game/HLPlayerController.h"

AHLGameMode::AHLGameMode()
{
	PlayerControllerClass = AHLPlayerController::StaticClass();
	HUDClass = AHLHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
