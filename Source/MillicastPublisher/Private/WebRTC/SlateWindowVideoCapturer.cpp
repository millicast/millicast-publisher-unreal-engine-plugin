#include "SlateWindowVideoCapturer.h"

#include "Framework/Application/SlateApplication.h"

#include "PeerConnection.h"
#include "MillicastPublisherPrivate.h"

// Maybe return a TUniquePtr or Shared or somehting less ... raw
IMillicastVideoSource* IMillicastVideoSource::Create()
{
	return new SlateWindowVideoCapturer;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> SlateWindowVideoCapturer::StartCapture()
{
	RtcVideoSource = new rtc::RefCountedObject<FTexture2DVideoSourceAdapter>();

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(this, 
		&SlateWindowVideoCapturer::OnBackBufferReadyToPresent);

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

	return RtcVideoTrack;
}

void SlateWindowVideoCapturer::StopCapture()
{
	FScopeLock lock(&CriticalSection);

	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);

	RtcVideoSource = nullptr;
	RtcVideoTrack = nullptr;
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
