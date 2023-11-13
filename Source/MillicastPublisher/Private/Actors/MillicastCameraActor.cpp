// Copyright Dolby.io 2023. All Rights Reserved.

#include "MillicastCameraActor.h"

#include "Components/MillicastViewportCapturerComponent.h"

AMillicastCameraActor::AMillicastCameraActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ViewportCapturer = ObjectInitializer.CreateDefaultSubobject<UMillicastViewportCapturerComponent>(
		this,
		TEXT("MillicastViewportCapturerComponent")
		);
	ViewportCapturer->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}
