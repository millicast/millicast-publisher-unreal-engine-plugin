// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"

namespace Millicast::Publisher
{
	/** Video source capturer to capture video frame from a RenderTarget2D */
	class RenderTargetCapturer : public IMillicastVideoSource
	{
	public:
		DECLARE_EVENT(RenderTargetCapturer, FMillicastSourceOnFrameRendered)
		FMillicastSourceOnFrameRendered OnFrameRendered;

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

		UTextureRenderTarget2D* RenderTarget = nullptr;
		FVideoTrackInterface RtcVideoTrack;
		rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;
	};

}