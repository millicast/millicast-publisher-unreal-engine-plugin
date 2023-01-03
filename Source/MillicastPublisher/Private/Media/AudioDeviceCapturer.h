// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "AudioCapturerBase.h"

#include <AudioCaptureCore.h>
#include <AudioDevice.h>

namespace Millicast::Publisher
{
	/** Class to capture audio from a system audio device */
	class AudioDeviceCapturer : public AudioCapturerBase
	{
		static TArray<Audio::FCaptureDeviceInfo> CaptureDevices;

		int32                     DeviceIndex;
		Audio::FAudioCapture      AudioCapture;

		float VolumeMultiplier; // dB

	public:
		AudioDeviceCapturer() noexcept;
		virtual ~AudioDeviceCapturer() override = default;

		FStreamTrackInterface StartCapture() override;
		void StopCapture() override;

		/**
		* Set the audio device to use.
		* The device index is the index in the CaptureDevice info list return by GetCaptureDevicesAvailable.
		*/
		void SetAudioCaptureDevice(int32 InDeviceIndex);

		/**
		* Set the audio device by its id
		*/
		void SetAudioCaptureDeviceById(FStringView Id);

		/**
		* Set the audio device by its name
		*/
		void SetAudioCaptureDeviceByName(FStringView name);

		void SetVolumeMultiplier(float f) noexcept { VolumeMultiplier = f; }

		static TArray<Audio::FCaptureDeviceInfo>& GetCaptureDevicesAvailable();
	};

}