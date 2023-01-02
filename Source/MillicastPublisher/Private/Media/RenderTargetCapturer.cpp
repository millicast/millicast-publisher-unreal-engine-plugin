#include "RenderTargetCapturer.h"
#include "MillicastPublisherPrivate.h"
#include "WebRTC/PeerConnection.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Util.h"

#include <condition_variable>

IMillicastVideoSource* IMillicastVideoSource::Create(UTextureRenderTarget2D* RenderTarget)
{
	return new RenderTargetCapturer(RenderTarget);
}

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
	RtcVideoSource = new rtc::RefCountedObject<FTexture2DVideoSourceAdapter>();

	// Get PCF to create video track
	auto PeerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

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
	std::unique_lock<std::mutex> Lock(CriticalSection);

	if (RtcVideoSource)
	{
		RtcVideoTrack = nullptr;
		RtcVideoSource = nullptr;
		// Remove callback to stop receiveng end frame rendering event
		FCoreDelegates::OnEndFrameRT.RemoveAll(this);

		// Wait for OnEndFrameRT callback to finish its last call to avoid segfault 
		// because this object is released while the thread is still stuck on the mutex
		UE_LOG(LogMillicastPublisher, Log, TEXT("Waiting for thread to finish"));
		auto status = ConditionVariable.wait_for(Lock, std::chrono::milliseconds(100));

		if (status == std::cv_status::no_timeout)
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Thread finished: no timeout"));
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Thread finished: timeout"));
		}
	}
}

RenderTargetCapturer::FStreamTrackInterface RenderTargetCapturer::GetTrack()
{
	return RtcVideoTrack;
}

void RenderTargetCapturer::SwitchTarget(UTextureRenderTarget2D* InRenderTarget)
{
	std::unique_lock<std::mutex> Lock(CriticalSection);

	RenderTarget = InRenderTarget;
}

void RenderTargetCapturer::OnEndFrameRenderThread()
{
	std::unique_lock<std::mutex> Lock(CriticalSection);

	if (RtcVideoSource)
	{
		// Read the render target resource texture 2D
		auto texture = RenderTarget->GetResource()->GetTexture2DRHI();

		// Convert it to WebRTC video frame
		if (texture)
		{
			RtcVideoSource->OnFrameReady(texture);
		}
	}

	Lock.unlock();
	ConditionVariable.notify_all();
}