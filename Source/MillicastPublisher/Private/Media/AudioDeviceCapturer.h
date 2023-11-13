// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "AudioCapturerBase.h"
#include "Subsystems/MillicastAudioDeviceCaptureSubsystem.h"

#include <AudioCaptureCore.h>
#include <AudioDevice.h>


namespace Millicast::Publisher
{
	/** Class to capture audio from a system audio device */
	class AudioDeviceCapturer : public AudioCapturerBase
	{
		FAudioCaptureInfo              DeviceInfo;
		TUniquePtr<AudioCapturerBase>  AudioCapture;

		float VolumeMultiplier = 0.0f; // dB

	public:
		FStreamTrackInterface StartCapture(UWorld* InWorld) override;
		void StopCapture() override;

		IMillicastSource::FStreamTrackInterface GetTrack() override;

		void SetAudioCaptureDevice(const FAudioCaptureInfo& InDeviceIndex);

		void SetVolumeMultiplier(float f) noexcept { VolumeMultiplier = f; }
	};

}