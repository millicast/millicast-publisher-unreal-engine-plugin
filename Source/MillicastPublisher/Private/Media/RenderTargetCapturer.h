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
		~RenderTargetCapturer() noexcept;

		FStreamTrackInterface StartCapture(UWorld* InWorld) override;
		void StopCapture() override;
		void SetSimulcast(bool InSimulcast) override { Simulcast = InSimulcast; }
		void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget) override { RenderTarget = InRenderTarget; }

		FStreamTrackInterface GetTrack() override;

		/** Switch render target object while capturing */
		void SwitchTarget(UTextureRenderTarget2D* InRenderTarget);

	private:
		/** Callback called on the rendering thread when a new frame has been rendered */
		void OnEndFrameRenderThread();

		UWorld* World = nullptr;
		UTextureRenderTarget2D* RenderTarget = nullptr;
		bool Simulcast = false;
		
		FVideoTrackInterface RtcVideoTrack;
		rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;
	};

}