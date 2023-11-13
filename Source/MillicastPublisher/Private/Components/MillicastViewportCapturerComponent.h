// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "Components/SceneCaptureComponent2D.h"

#include "MillicastViewportCapturerComponent.generated.h"

/** A component used to capture viewport from a Millicast Camera Actor */
UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Viewport Capturer Component", BlueprintSpawnableComponent))
class MILLICASTPUBLISHER_API UMillicastViewportCapturerComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

private:
	/** Width and height of the viewport. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings", META = (DisplayName = "Capture Size", AllowPrivateAccess = true))
	FIntPoint TargetSize = FIntPoint(1280, 720);
};
