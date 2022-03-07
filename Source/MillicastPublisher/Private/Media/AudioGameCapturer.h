// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "IMillicastSource.h"

class AudioGameCapturer : public IMillicastAudioSource
{
	rtc::scoped_refptr<webrtc::AudioSourceInterface> RtcAudioSource;
	FStreamTrackInterface                            RtcAudioTrack;

public:

	AudioGameCapturer() noexcept;

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;
	FStreamTrackInterface GetTrack() override;
};