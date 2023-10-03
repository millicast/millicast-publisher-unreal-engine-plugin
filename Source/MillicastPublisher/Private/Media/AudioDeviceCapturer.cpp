// Copyright Millicast 2022. All Rights Reserved.
#include "AudioDeviceCapturer.h"

#include "common_audio/include/audio_util.h"
#include "MillicastPublisherPrivate.h"

#if PLATFORM_WINDOWS
#include "WasapiDeviceCapturer.h"
#endif

namespace Millicast::Publisher
{

/** Class to capture audio from a system audio device */

class InputCapturer : public AudioCapturerBase
{
	int32                     DeviceIndex;
	Audio::FAudioCapture      AudioCapture;

	float VolumeMultiplier = 0.0f; // dB

public:
	FStreamTrackInterface StartCapture(UWorld* InWorld) override;
	void StopCapture() override;

	void SetAudioCaptureDevice(int32 InDeviceIndex);

	void SetVolumeMultiplier(float f) noexcept { VolumeMultiplier = f; }
};

AudioDeviceCapturer::FStreamTrackInterface InputCapturer::StartCapture(UWorld* InWorld)
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

void InputCapturer::StopCapture()
{
	if (RtcAudioTrack)
	{
		AudioCapture.StopStream();
		RtcAudioTrack = nullptr;
	}
}

void InputCapturer::SetAudioCaptureDevice(int32 InDeviceIndex)
{
	DeviceIndex = InDeviceIndex;
}

/** Class to capture audio from a system audio device */

AudioDeviceCapturer::FStreamTrackInterface AudioDeviceCapturer::StartCapture(UWorld* InWorld)
{
	if (DeviceInfo.Direction == EAudioCaptureDirection::Input)
	{
		TUniquePtr<InputCapturer> CapturerImpl = MakeUnique<InputCapturer>();
		CapturerImpl->SetVolumeMultiplier(VolumeMultiplier);

		auto* Subsystem = GEngine->GetEngineSubsystem<UMillicastAudioDeviceCaptureSubsystem>();

		if (!Subsystem)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceById] UMillicastAudioDeviceCaptureSubsystem not found"));
			return nullptr;
		}

		const auto Index = Subsystem->Devices.IndexOfByPredicate([this](const auto& e) { 
			return DeviceInfo.DeviceId == e.DeviceId; 
			});

		if (Index == INDEX_NONE)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceById] Device not found: %s %s"), *DeviceInfo.DeviceName, *DeviceInfo.DeviceId);
			return nullptr;
		}

		CapturerImpl->SetAudioCaptureDevice(Index);
		AudioCapture = MoveTemp(CapturerImpl);
	}
	else
	{
#if PLATFORM_WINDOWS
		TUniquePtr<WasapiDeviceCapturer> WasapiCapturer = MakeUnique<WasapiDeviceCapturer>(10, true);
		WasapiCapturer->SetAudioDeviceById(DeviceInfo.DeviceId);

		AudioCapture = MoveTemp(WasapiCapturer);
#else
		return nullptr;
#endif
	}

	return AudioCapture->StartCapture(InWorld);
}
void AudioDeviceCapturer::StopCapture()
{
	AudioCapture->StopCapture();
	AudioCapture = nullptr;
}

void AudioDeviceCapturer::SetAudioCaptureDevice(const FAudioCaptureInfo& InDeviceIndex)
{
	DeviceInfo = InDeviceIndex;
}

}