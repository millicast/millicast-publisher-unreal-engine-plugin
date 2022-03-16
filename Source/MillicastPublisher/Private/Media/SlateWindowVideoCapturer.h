// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"

/**
* This class is a video source capturer and captures video from the Slate Window renderer
*/
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
	/** Callback from the SlateWindowRenderer when a new frame buffer is ready */
	void OnBackBufferReadyToPresent(SWindow& SlateWindow, const FTexture2DRHIRef& Buffer);
};