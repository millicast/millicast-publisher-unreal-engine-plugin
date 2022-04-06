// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

class UMillicastViewportCapturerComponent;

#include "MillicastCameraActor.generated.h"

/**
	Capture the scene from the perspective of a camera that starts broadcasting the viewport.
	Solve the problem of gamma correction observed when using the SceneCapture2D actor in game.
*/
UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Camera Actor"))
class MILLICASTPUBLISHER_API AMillicastCameraActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Viewport, AssetRegistrySearchable)
	UMillicastViewportCapturerComponent* ViewportCapturer;

public:
	virtual void BeginPlay() override;
};