// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastViewportCapturerComponent.h"
#include "MillicastPublisherPrivate.h"
#include "CanvasTypes.h"
#include <EngineModule.h>
#include <LegacyScreenPercentageDriver.h>

UMillicastViewportCapturerComponent::UMillicastViewportCapturerComponent(const FObjectInitializer& ObjectInitializer) :
	bIsInitialized(false)
{
	bWantsInitializeComponent = true;
	CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	PostProcessSettings.DepthOfFieldFocalDistance = 10000.f;
}

void UMillicastViewportCapturerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UMillicastViewportCapturerComponent::UninitializeComponent()
{
}

void UMillicastViewportCapturerComponent::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	// ensure we have some thread-safety
	FScopeLock Lock(&UpdateRenderContext);

	if (TextureTarget == nullptr)
	{
		return;
	}

	// Do the actual capturing
	Super::UpdateSceneCaptureContents(Scene);
}

void UMillicastViewportCapturerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
}

void UMillicastViewportCapturerComponent::Deactivate()
{
	Super::Deactivate();
}

void UMillicastViewportCapturerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}