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

void AMillicastCameraActor::BeginPlay()
{
	Super::BeginPlay();
}
