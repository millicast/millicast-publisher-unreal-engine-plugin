// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "AudioCapturerBase.h"
#include "Sound/SoundSubmix.h"

namespace Millicast::Publisher
{
	/** Class to capturer audio from the main audio device */
	class AudioSubmixCapturer : public AudioCapturerBase, public ISubmixBufferListener
	{
	public:
		virtual ~AudioSubmixCapturer() override;

		/** Just create audio source and audio track. The audio capture is started by WebRTC in the audio device module */
		FStreamTrackInterface StartCapture(UWorld* InWorld) override;
		void StopCapture() override;

		/** Set the submix to attach a callback to. nullptr means master submix */
		void SetAudioSubmix(USoundSubmix* InSubmix = nullptr);

		/** Set the device id to use */
		void SetAudioDeviceId(Audio::FDeviceId Id);

		/** Called by the main audio device when a new audio data buffer is ready */
		void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
			int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;

	private:
		FAudioDevice* AudioDevice = nullptr;
		USoundSubmix* Submix = nullptr;
		TOptional<Audio::FDeviceId> DeviceId;
	};

}