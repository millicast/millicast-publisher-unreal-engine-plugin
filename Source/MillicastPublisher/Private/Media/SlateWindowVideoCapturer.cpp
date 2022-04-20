#include "SlateWindowVideoCapturer.h"

#include "Framework/Application/SlateApplication.h"

#include "WebRTC/PeerConnection.h"
#include "MillicastPublisherPrivate.h"

#include "Util.h"

// Maybe return a TUniquePtr or Shared or somehting less ... raw
IMillicastVideoSource* IMillicastVideoSource::Create()
{
	return new SlateWindowVideoCapturer;
}

IMillicastSource::FStreamTrackInterface SlateWindowVideoCapturer::StartCapture()
{
	// Create WebRTC video source
	RtcVideoSource = new rtc::RefCountedObject<FTexture2DVideoSourceAdapter>();

	// Attach the callback to the Slate window renderer
	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, 
		&SlateWindowVideoCapturer::OnBackBufferReadyToPresent);

	// Get the peerconnection factory to create the video track
	auto PeerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	RtcVideoTrack = PeerConnectionFactory->CreateVideoTrack(to_string(TrackId.Get("slate-window-track")), RtcVideoSource);

	// Check
	if (RtcVideoTrack) 
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Created video track"));
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not create video track"));
	}

	return RtcVideoTrack;
}

void SlateWindowVideoCapturer::StopCapture()
{
	FScopeLock lock(&CriticalSection);

	// Remove the callback to stop receiving new buffers
	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);

	// Destroy track and source
	RtcVideoSource = nullptr;
	RtcVideoTrack = nullptr;
}

SlateWindowVideoCapturer::FStreamTrackInterface SlateWindowVideoCapturer::GetTrack()
{
	return RtcVideoTrack;
}

void SlateWindowVideoCapturer::OnBackBufferReadyToPresent(SWindow& SlateWindow, const FTexture2DRHIRef& Buffer)
{
	FScopeLock lock(&CriticalSection);

	if (RtcVideoSource)
	{
		check(IsInRenderingThread());

		// Create and send webrtc video frame
		RtcVideoSource->OnFrameReady(Buffer, true);
	}
}

