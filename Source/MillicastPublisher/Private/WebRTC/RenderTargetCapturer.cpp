#include "RenderTargetCapturer.h"
#include "MillicastPublisherPrivate.h"
#include "PeerConnection.h"

#include "Engine/TextureRenderTarget2D.h"

IMillicastVideoSource* IMillicastVideoSource::Create(UTextureRenderTarget2D* RenderTarget)
{
	return new RenderTargetCapturer(RenderTarget);
}

RenderTargetCapturer::RenderTargetCapturer(UTextureRenderTarget2D* InRenderTarget) noexcept
	: RenderTarget(InRenderTarget)
{}

RenderTargetCapturer::FVideoTrackInterface RenderTargetCapturer::StartCapture()
{
	if (RenderTarget == nullptr) 
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not start capture, no render target has been provided"));
		return nullptr;
	}

	RtcVideoSource = new rtc::RefCountedObject<FTexture2DVideoSourceAdapter>();

	auto PeerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	RtcVideoTrack = PeerConnectionFactory->CreateVideoTrack("slate-window", RtcVideoSource);

	if (RtcVideoTrack)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Created video track"));
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not create video track"));
	}

	FCoreDelegates::OnEndFrameRT.AddRaw(this, &RenderTargetCapturer::OnEndFrameRenderThread);

	return RtcVideoTrack;
}

void RenderTargetCapturer::StopCapture()
{
	FScopeLock Lock(&CriticalSection);

	RtcVideoTrack = nullptr;
	RtcVideoSource = nullptr;

	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
}

RenderTargetCapturer::FVideoTrackInterface RenderTargetCapturer::GetTrack()
{
	return RtcVideoTrack;
}

void RenderTargetCapturer::SwitchTarget(UTextureRenderTarget2D* InRenderTarget)
{
	FScopeLock Lock(&CriticalSection);

	RenderTarget = InRenderTarget;
}

void RenderTargetCapturer::OnEndFrameRenderThread()
{
	FScopeLock Lock(&CriticalSection);

	if (RtcVideoSource)
	{
		FTexture2DRHIRef texture = RenderTarget->Resource->GetTexture2DRHI();

		RtcVideoSource->OnFrameReady(texture);
	}
}

void RenderTargetCapturer::OnEndFrame()
{
	FScopeLock Lock(&CriticalSection);

	if (RtcVideoSource)
	{
		FTexture2DRHIRef texture = RenderTarget->Resource->GetTexture2DRHI();

		RtcVideoSource->OnFrameReady(texture);
	}
}
