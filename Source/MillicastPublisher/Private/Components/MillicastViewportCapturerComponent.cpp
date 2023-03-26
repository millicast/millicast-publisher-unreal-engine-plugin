// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastViewportCapturerComponent.h"

UMillicastViewportCapturerComponent::UMillicastViewportCapturerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	PostProcessSettings.DepthOfFieldFocalDistance = 10000.f;
}
