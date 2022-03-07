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
	RtcVideoSource = new rtc::RefCountedObject<FTexture2DVideoSourceAdapter>();

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, 
		&SlateWindowVideoCapturer::OnBackBufferReadyToPresent);

	auto PeerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	RtcVideoTrack = PeerConnectionFactory->CreateVideoTrack(to_string(TrackId.Get("slate-window-track")), RtcVideoSource);

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

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);

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

		RtcVideoSource->OnFrameReady(Buffer);
	}
}

