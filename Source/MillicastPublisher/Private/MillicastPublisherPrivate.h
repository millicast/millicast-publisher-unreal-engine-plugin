// Copyright Millicast 2022. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogMillicastPublisher, Log, All);

namespace MillicastPublisherOption
{
    static const FName StreamName("StreamName");
    static const FName StreamUrl("StreamUrl");
	static const FName PublishingToken("PublishingToken");
	static const FName SourceId("SourceId");
	static const FName CaptureAudio("CaptureAudio");
	static const FName CaptureVideo("CaptureVideo");
	static const FName RenderTarget("RenderTarget");
	static const FName Submix("Submix");
	static const FName CaptureDeviceIndex("CaptureDeviceIndex");
	static const FName AudioCaptureType("AudioCaptureType");
	static const FName VolumeMultiplier("VolumeMultiplier");
}
