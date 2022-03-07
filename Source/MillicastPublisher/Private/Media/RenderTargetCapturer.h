#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"

class RenderTargetCapturer : public IMillicastVideoSource
{
	UTextureRenderTarget2D* RenderTarget;
	FVideoTrackInterface RtcVideoTrack;
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;

	FCriticalSection CriticalSection;

public:
	explicit RenderTargetCapturer(UTextureRenderTarget2D* InRenderTarget) noexcept;

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	FStreamTrackInterface GetTrack() override;

	void SwitchTarget(UTextureRenderTarget2D* InRenderTarget);

private:
	void OnEndFrameRenderThread();
	void OnEndFrame();
};