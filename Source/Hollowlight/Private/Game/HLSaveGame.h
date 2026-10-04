// EMBERHOME: saved progress and settings (one slot, all platforms). (CLAUDE.md: Game flow)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HLSaveGame.generated.h"

UENUM()
enum class EHLTouchMode : uint8
{
	Auto,   // shown on touch devices, or after the first touch
	On,
	Off
};

UCLASS()
class UHLSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("Hollowlight");

	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 UnlockedLevels = 1;     // 1..number of levels
	UPROPERTY() int32 LastLevel = 0;
	UPROPERTY() TArray<float> BestTimes;       // seconds, 0 = never finished
	UPROPERTY() TArray<int32> BestDeaths;
	UPROPERTY() bool bFinishedGame = false;
	// Progress inside levels: how many checkpoints of each level have been reached (a cleared level counts
	// them all), and the checkpoint CONTINUE resumes from (-1 = the start of LastLevel).
	UPROPERTY() TArray<int32> ReachedCheckpoints;
	UPROPERTY() int32 LastCheckpoint = -1;
	UPROPERTY() TArray<int32> EmberMask;       // per level: one bit for each ember found

	UPROPERTY() bool bMusic = true;
	UPROPERTY() bool bSound = true;
	UPROPERTY() EHLTouchMode TouchMode = EHLTouchMode::Auto;
	UPROPERTY() bool bFilmGrain = true;
	UPROPERTY() bool bReduceFlashing = false;
	UPROPERTY() bool bTimer = true;            // the speedrun clock on the play screen

	// Touch calibration ("touch the light"): reported = Scale * true + Offset, in screen fractions.
	UPROPERTY() bool bTouchCalibrated = false;
	UPROPERTY() float TouchScaleX = 1.f;
	UPROPERTY() float TouchOffsetX = 0.f;
	UPROPERTY() float TouchScaleY = 1.f;
	UPROPERTY() float TouchOffsetY = 0.f;
};
