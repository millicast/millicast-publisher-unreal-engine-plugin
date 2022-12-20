// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "AudioCapturerBase.h"
#include <Sound/SoundSubmix.h>

/** Class to capturer audio from the main audio device */
class AudioSubmixCapturer : public AudioCapturerBase, public ISubmixBufferListener
{	
	FAudioDevice* AudioDevice;
	USoundSubmix* Submix;
	TOptional<Audio::FDeviceId> DeviceId;

public:
	AudioSubmixCapturer() noexcept;
	virtual ~AudioSubmixCapturer() override;

	/** Just create audio source and audio track. The audio capture is started by WebRTC in the audio device module */
	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	/** Set the submix to attach a callback to. nullptr means master submix */
	void SetAudioSubmix(USoundSubmix* InSubmix = nullptr);

	/** Set the device id to use */
	void SetAudioDeviceId(Audio::FDeviceId Id);

	/** Called by the main audio device when a new audio data buffer is ready */
	void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
		int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
};