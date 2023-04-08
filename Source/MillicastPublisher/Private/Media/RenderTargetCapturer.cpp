#include "RenderTargetCapturer.h"

#include "MillicastPublisherPrivate.h"
#include "Util.h"

#include "Engine/TextureRenderTarget2D.h"
#include "WebRTC/PeerConnection.h"

IMillicastVideoSource* IMillicastVideoSource::Create(UTextureRenderTarget2D* RenderTarget)
{
	return new Millicast::Publisher::RenderTargetCapturer(RenderTarget);
}

namespace Millicast::Publisher
{
	RenderTargetCapturer::RenderTargetCapturer(UTextureRenderTarget2D* InRenderTarget) noexcept
		: RenderTarget(InRenderTarget)
	{}

	RenderTargetCapturer::~RenderTargetCapturer() noexcept
	{
		StopCapture();
	}

	RenderTargetCapturer::FStreamTrackInterface RenderTargetCapturer::StartCapture()
	{
		// Check if a render target has been set in order to start the capture
		if (RenderTarget == nullptr)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not start capture, no render target has been provided"));
			return nullptr;
		}

		// Create WebRTC Video source
		RtcVideoSource = new rtc::RefCountedObject<Millicast::Publisher::FTexture2DVideoSourceAdapter>();
		RtcVideoSource->SetSimulcast(Simulcast);

		// Get PCF to create video track
		auto PeerConnectionFactory = Millicast::Publisher::FWebRTCPeerConnection::GetPeerConnectionFactory();

		RtcVideoTrack = PeerConnectionFactory->CreateVideoTrack(to_string(TrackId.Get("render-target-track")), RtcVideoSource);

		if (RtcVideoTrack)
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Created video track"));
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not create video track"));
		}

		// Attach a callback to be notified when a new frame is ready
		FCoreDelegates::OnEndFrameRT.AddRaw(this, &RenderTargetCapturer::OnEndFrameRenderThread);

		return RtcVideoTrack;
	}

	void RenderTargetCapturer::StopCapture()
	{
		if (RtcVideoSource)
		{
			FRenderCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();

			RtcVideoTrack = nullptr;
			RtcVideoSource = nullptr;
			// Remove callback to stop receiveng end frame rendering event
			FCoreDelegates::OnEndFrameRT.RemoveAll(this);
		}
	}

	RenderTargetCapturer::FStreamTrackInterface RenderTargetCapturer::GetTrack()
	{
		return RtcVideoTrack;
	}

	void RenderTargetCapturer::SwitchTarget(UTextureRenderTarget2D* InRenderTarget)
	{
		FRenderCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		RenderTarget = InRenderTarget;
	}

	void RenderTargetCapturer::OnEndFrameRenderThread()
	{
		if (RtcVideoSource)
		{
			// Read the render target resource texture 2D
			auto texture = RenderTarget->GetResource()->GetTexture2DRHI();

			// Convert it to WebRTC video frame
			if (texture)
			{
				OnFrameRendered.Broadcast();
				RtcVideoSource->OnFrameReady(texture);
			}
		}
	}
}