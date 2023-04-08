#include "SlateWindowVideoCapturer.h"

#include "MillicastPublisherPrivate.h"
#include "Util.h"

#include "Async/Async.h"
#include "WebRTC/PeerConnection.h"
#include "Widgets/SViewport.h"

TSharedPtr<IMillicastVideoSource> IMillicastVideoSource::Create()
{
	return Millicast::Publisher::SlateWindowVideoCapturer::CreateCapturer();
}

namespace Millicast::Publisher
{

TSharedPtr<SlateWindowVideoCapturer> SlateWindowVideoCapturer::CreateCapturer()
{
	TSharedPtr<SlateWindowVideoCapturer> Capturer( new SlateWindowVideoCapturer() );

	// TODO [RW] Simulcast support for this?
	Capturer->SetSimulcast(false);
	
	// We create TWeakPtr here because we don't want gamethread keeping this alive if it is deleted before this async task happens.
	TWeakPtr<SlateWindowVideoCapturer> WeakSelf = TWeakPtr<SlateWindowVideoCapturer>(Capturer);
	AsyncTask(ENamedThreads::GameThread, [WeakSelf](){
		TSharedPtr<SlateWindowVideoCapturer> Capturer = WeakSelf.Pin();
		if(Capturer)
		{
			// We set the target window to the window with game viewport in it.
			// Note: Calls to `FSlateApplication::Get()` have to on the GameThread, hence the AsyncTask.
			FSlateApplication& SlateApp = FSlateApplication::Get();
			TSharedPtr<SViewport> GameViewport = SlateApp.GetGameViewport();
			Capturer->SetTargetWindow(SlateApp.FindWidgetWindow(GameViewport.ToSharedRef()));
		}
	});

	return Capturer;
}

void SlateWindowVideoCapturer::SetTargetWindow(TSharedPtr<SWindow> InTargetWindow)
{
	TargetWindow = InTargetWindow;
}

IMillicastSource::FStreamTrackInterface SlateWindowVideoCapturer::StartCapture()
{
	// Create WebRTC video source
	RtcVideoSource = new rtc::RefCountedObject<Millicast::Publisher::FTexture2DVideoSourceAdapter>();
	RtcVideoSource->SetSimulcast(Simulcast);

	// Attach the callback to the Slate window renderer
	OnBackBufferHandle = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddSP(this, 
		&SlateWindowVideoCapturer::OnBackBufferReadyToPresent);

	// Get the peerconnection factory to create the video track
	auto PeerConnectionFactory = Millicast::Publisher::FWebRTCPeerConnection::GetPeerConnectionFactory();

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

	// We don't bother remove this callback on engine exit because renderer is already destroyed 
	if (!IsEngineExitRequested())
	{
		// Remove the callback to stop receiving new buffers
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(OnBackBufferHandle);
	}

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
	checkf(IsInRenderingThread(), TEXT("Window capture must happen on the render thread."));

	FScopeLock Lock(&CriticalSection);

	if(!RtcVideoSource)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("FSlateWindowCapturer will skip capturing when video source has not been set."));
		return;
	}

	if(!TargetWindow)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("FSlateWindowCapturer cannot capture a window when no target window has been set."));
		return;
	}

	if(&SlateWindow != TargetWindow.Get())
	{
		// This is not our target window so skip it.
		return;
	}

	// Create and send webrtc video frame
	RtcVideoSource->OnFrameReady(Buffer);
}

}