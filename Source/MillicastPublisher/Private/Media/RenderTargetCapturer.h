// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"
#include <condition_variable>


/** Video source capturer to capture video frame from a RenderTarget2D */
class RenderTargetCapturer : public IMillicastVideoSource
{
	UTextureRenderTarget2D* RenderTarget;
	FVideoTrackInterface RtcVideoTrack;
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;

	std::mutex CriticalSection;
	std::condition_variable ConditionVariable;

public:
	explicit RenderTargetCapturer(UTextureRenderTarget2D* InRenderTarget) noexcept;
	~RenderTargetCapturer() noexcept;

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	FStreamTrackInterface GetTrack() override;

	/** Switch render target object while capturing */
	void SwitchTarget(UTextureRenderTarget2D* InRenderTarget);

private:
	/** Callback called on the rendering thread when a new frame has been rendered */
	void OnEndFrameRenderThread();
};