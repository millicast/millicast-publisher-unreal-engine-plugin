// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastVideoSource.h"
#include "Texture2DVideoSourceAdapter.h"

class SlateWindowVideoCapturer : public IMillicastVideoSource
{
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;
	FVideoTrackInterface RtcVideoTrack;
	FCriticalSection CriticalSection;

public:
	SlateWindowVideoCapturer() noexcept : RtcVideoSource(nullptr), RtcVideoTrack(nullptr) {}

	FVideoTrackInterface StartCapture() override;
	void StopCapture() override;
	FVideoTrackInterface GetTrack() override;

private:
	void OnBackBufferReadyToPresent(SWindow& SlateWindow, const FTexture2DRHIRef& Buffer);
};