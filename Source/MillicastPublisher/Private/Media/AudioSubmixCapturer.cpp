// Copyright Millicast 2022. All Rights Reserved.
#include "AudioSubmixCapturer.h"

#include <AudioDevice.h>

#include "MillicastPublisherPrivate.h"

namespace Millicast::Publisher
{
	inline FAudioDevice* GetUEAudioDevice()
	{
		if (!GEngine)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("GEngine is NULL"));
			return nullptr;
		}

		auto AudioDevice = GEngine->GetMainAudioDevice();
		if (!AudioDevice)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not get main audio device"));
			return nullptr;
		}

		return AudioDevice.GetAudioDevice();
	}

	inline FAudioDevice* GetUEAudioDevice(Audio::FDeviceId id)
	{
		if (!GEngine)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("GEngine is NULL"));
			return nullptr;
		}

		auto AudioDevice = GEngine->GetAudioDeviceManager()->GetAudioDevice(id);
		if (!AudioDevice)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not get main audio device"));
			return nullptr;
		}

		return AudioDevice.GetAudioDevice();
	}

	IMillicastSource::FStreamTrackInterface AudioSubmixCapturer::StartCapture(UWorld* InWorld)
	{
		if (DeviceId.IsSet())
		{
			AudioDevice = GetUEAudioDevice(DeviceId.GetValue());
		}
		else
		{
			AudioDevice = GetUEAudioDevice();
		}

		if (!AudioDevice)
		{
			return nullptr;
		}

		AudioDevice->RegisterSubmixBufferListener(this, Submix);

		CreateRtcSourceTrack();

		return RtcAudioTrack;
	}

	void AudioSubmixCapturer::StopCapture()
	{
		if (!RtcAudioTrack)
		{
			return;
		}

		// If engine exit requested then audio device is already destroyed.
		if (!IsEngineExitRequested())
		{
			AudioDevice->UnregisterSubmixBufferListener(this, Submix);
		}

		RtcAudioTrack = nullptr;
	}

	AudioSubmixCapturer::~AudioSubmixCapturer()
	{
		StopCapture();
	}

	void AudioSubmixCapturer::SetAudioSubmix(USoundSubmix* InSubmix)
	{
		Submix = InSubmix;
	}

	void AudioSubmixCapturer::SetAudioDeviceId(Audio::FDeviceId Id)
	{
		DeviceId = Id;
	}

	void AudioSubmixCapturer::OnNewSubmixBuffer(const USoundSubmix* /*OwningSubmix*/, float* InAudioData, int32 InNumSamples, int32 InNumChannels, const int32 SampleRate, double AudioClock)
	{
		if (SamplePerSecond == SampleRate)
		{
			SendAudio(InAudioData, InNumSamples, InNumChannels);
		}
	}

}