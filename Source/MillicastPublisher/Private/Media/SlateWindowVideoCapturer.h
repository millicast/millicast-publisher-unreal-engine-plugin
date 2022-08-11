// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "IMillicastSource.h"
#include "WebRTC/Texture2DVideoSourceAdapter.h"
#include "Templates/SharedPointer.h"

/**
* This class is a video source capturer and captures video from the Slate Window renderer
*/
class SlateWindowVideoCapturer : public IMillicastVideoSource, public TSharedFromThis<SlateWindowVideoCapturer>
{
	rtc::scoped_refptr<FTexture2DVideoSourceAdapter> RtcVideoSource;
	FVideoTrackInterface RtcVideoTrack;
	FCriticalSection CriticalSection;
	TSharedPtr<SWindow> TargetWindow;
	FDelegateHandle OnBackBufferHandle;

public:
	static TSharedPtr<SlateWindowVideoCapturer> CreateCapturer();

	/* Begin IMillicastVideoSource */
	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;
	FStreamTrackInterface GetTrack() override;
	/* End IMillicastVideoSource */

	/**
	 * Window capturer can only capture one window at a time. 
	 * Use this to specify which window to capture.
	 * @param InTargetWindow The window to capture.
	 **/
	void SetTargetWindow(TSharedPtr<SWindow> InTargetWindow);

private:
	// Constructor explicitly private because we only want user to be create TSharedPtr of this type.
	SlateWindowVideoCapturer() noexcept : RtcVideoSource(nullptr), RtcVideoTrack(nullptr) {}

	/** Callback from the SlateWindowRenderer when a new frame buffer is ready */
	void OnBackBufferReadyToPresent(SWindow& SlateWindow, const FTexture2DRHIRef& Buffer);
};