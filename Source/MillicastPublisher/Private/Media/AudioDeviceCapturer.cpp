// Copyright Millicast 2022. All Rights Reserved.
#include "AudioDeviceCapturer.h"

#include "common_audio/include/audio_util.h"
#include "MillicastPublisherPrivate.h"

namespace Millicast::Publisher
{
AudioDeviceCapturer::FStreamTrackInterface AudioDeviceCapturer::StartCapture(UWorld* InWorld)
{
	CreateRtcSourceTrack();

	Audio::FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 InNumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
	{
		if (SamplePerSecond != SampleRate)
		{
			return;
		}

		if (GetNumChannel() != InNumChannels)
		{
			SetNumChannel(InNumChannels);
		}

		int32 NumSamples = NumFrames * InNumChannels;

		for (int i = 0; i < NumSamples; ++i)
		{
			const float factor = pow(10.0f, VolumeMultiplier / 20.f); // Volume amplifier factor
			float value = FMath::Clamp(AudioData[i] * factor, -1.f, 1.f); // Amplify the volume by the factor and clamp it

			AudioBuffer.Add(webrtc::FloatToS16(value)); // Convert the float value to S16 and add it to the Buffer
		}

		SendAudio();
	};

	Audio::FAudioCaptureDeviceParams Params = Audio::FAudioCaptureDeviceParams();
	Params.DeviceIndex = DeviceIndex;

	// Start the stream here to avoid hitching the audio render thread. 
	if (AudioCapture.OpenCaptureStream(Params, MoveTemp(OnCapture), 1024))
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Starting audio capture"));
		AudioCapture.StartStream();
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not start audio capture"));
	}

	return RtcAudioTrack;
}

void AudioDeviceCapturer::StopCapture()
{
	if (RtcAudioTrack)
	{
		AudioCapture.StopStream();
		RtcAudioTrack = nullptr;
	}
}

void AudioDeviceCapturer::SetAudioCaptureDevice(int32 InDeviceIndex)
{
	DeviceIndex = InDeviceIndex;
}

}