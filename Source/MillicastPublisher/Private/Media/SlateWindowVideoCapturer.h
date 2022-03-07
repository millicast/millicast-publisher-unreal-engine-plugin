// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"

class SlateWindowVideoCapturer : public IMillicastVideoSource
{
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;
	FVideoTrackInterface RtcVideoTrack;
	FCriticalSection CriticalSection;

public:
	SlateWindowVideoCapturer() noexcept : RtcVideoSource(nullptr), RtcVideoTrack(nullptr) {}

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;
	FStreamTrackInterface GetTrack() override;

private:
	void OnBackBufferReadyToPresent(SWindow& SlateWindow, const FTexture2DRHIRef& Buffer);
};