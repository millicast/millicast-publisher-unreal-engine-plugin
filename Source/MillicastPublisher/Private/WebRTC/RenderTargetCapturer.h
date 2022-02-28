#pragma once

#include "IMillicastVideoSource.h"
#include "Texture2DVideoSourceAdapter.h"

class RenderTargetCapturer : public IMillicastVideoSource
{
	UTextureRenderTarget2D* RenderTarget;
	FVideoTrackInterface RtcVideoTrack;
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;

	FCriticalSection CriticalSection;

public:

	explicit RenderTargetCapturer(UTextureRenderTarget2D* InRenderTarget) noexcept;

	FVideoTrackInterface StartCapture() override;
	void StopCapture() override;

	FVideoTrackInterface GetTrack() override;

	void SwitchTarget(UTextureRenderTarget2D* InRenderTarget);

private:

	void OnEndFrameRenderThread();
	void OnEndFrame();
};