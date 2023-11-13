// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "MillicastCameraActor.generated.h"

class UMillicastViewportCapturerComponent;

/**
	Capture the scene from the perspective of a camera that starts broadcasting the viewport.
	Solve the problem of gamma correction observed when using the SceneCapture2D actor in game.
*/
UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Camera Actor"))
class MILLICASTPUBLISHER_API AMillicastCameraActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Viewport)
	UMillicastViewportCapturerComponent* ViewportCapturer;
};
