// EMBERHOME: owns the game flow; gathers keyboard, gamepad, mouse and multi-touch input. (CLAUDE.md: Game flow / Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/HLGame.h"
#include "HLPlayerController.generated.h"

class UHLAudioSynth;
class UHLSaveGame;

UCLASS()
class AHLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHLPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PlayerTick(float DeltaTime) override;
	// The game draws its own touch pads; never create Unreal's default virtual joysticks (they would
	// sit on top and swallow the touches meant for JUMP and pause).
	virtual void CreateTouchInterface() override {}

	bool ShouldShowTouch() const;
	bool IsTouchDevice() const;

	FHLGame Game;

	UPROPERTY() TObjectPtr<UHLSaveGame> Save;
	UPROPERTY() TObjectPtr<UHLAudioSynth> Audio;

	// Read by the HUD.
	bool bTouchLeft = false, bTouchRight = false, bTouchJump = false, bTouchSlide = false;
	bool bMenuTouchDown = false;
	FVector2D MenuTouch = FVector2D::ZeroVector;
	FVector4 SafeArea = FVector4(0, 0, 0, 0);
	bool bForceTouch = false;

private:
	void GatherInput(float DeltaTime, FHLControls& Controls, FHLMenuInput& Menu);
	void UpdateSafeArea(double W, double H);
	void TickCapture(float DeltaTime);
	void HandleBackground();

	static constexpr int32 MaxTouches = 10;
	bool TouchDown[MaxTouches] = {};
	enum class ETouchRole : uint8 { None, Move, Jump, Slide };
	ETouchRole TouchRole[MaxTouches] = {};
	bool TouchSwiped[MaxTouches] = {};
	FVector2D TouchLastRaw[MaxTouches];
	float SwipeJumpTimer = 0, SwipeSlideTimer = 0;
	FVector2D TouchStart[MaxTouches];
	FVector2D TouchLast[MaxTouches];
	bool bTouchSeen = false;
	FVector2D LastMouse = FVector2D(-1, -1);
	float StickPrevY = 0, StickPrevX = 0;
	double SafeAreaTimer = 0;
	FDelegateHandle BackgroundHandle, DeactivateHandle;

	// Capture script (-HLCapture): screenshots of every screen and level, then quit.
	bool bCapture = false;
	int32 CaptureStep = -1;
	bool bCaptureShotTaken = false;
	double CaptureClock = 0;
	FString CaptureTag;
};
