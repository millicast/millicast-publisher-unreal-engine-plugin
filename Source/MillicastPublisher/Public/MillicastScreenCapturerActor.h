// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

class UMillicastScreenCapturerComponent;

#include "MillicastScreenCapturerActor.generated.h"

/**
* Capture a screen or an app
*/

UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Screen Capturer Actor"))
class MILLICASTPUBLISHER_API AMillicastScreenCapturerActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ScreenCapturer)
	UMillicastScreenCapturerComponent* ScreenCapturer;
};