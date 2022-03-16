// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "IMillicastSource.h"

/** Class to capturer audio from the game */
class AudioGameCapturer : public IMillicastAudioSource
{
	rtc::scoped_refptr<webrtc::AudioSourceInterface> RtcAudioSource;
	FStreamTrackInterface                            RtcAudioTrack;

public:

	AudioGameCapturer() noexcept;

	/** Just create audio source and audio track. The audio capture is started by WebRTC in the audio device module */
	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;
	FStreamTrackInterface GetTrack() override;
};