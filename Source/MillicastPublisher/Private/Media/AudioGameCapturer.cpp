// Copyright Millicast 2022. All Rights Reserved.

#include "AudioGameCapturer.h"
#include "WebRTC/PeerConnection.h"

#include "Util.h"

TArray<Audio::FCaptureDeviceInfo> AudioDeviceCapture::CaptureDevices;

IMillicastAudioSource* IMillicastAudioSource::Create(AudioCapturerType CapturerType)
{
	switch (CapturerType)
	{
	case AudioCapturerType::SUBMIX: return new AudioGameCapturer;
	case AudioCapturerType::DEVICE: return new AudioDeviceCapture;
	}

	return nullptr;
}

AudioCapturerBase::AudioCapturerBase() noexcept : RtcAudioSource(nullptr), RtcAudioTrack(nullptr)
{}

void AudioCapturerBase::CreateRtcSourceTrack()
{
	// Get PCF to create audio source and audio track
	auto peerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	// Disable WebRTC processing
	cricket::AudioOptions options;
	options.echo_cancellation.emplace(false);
	options.auto_gain_control.emplace(false);
	options.noise_suppression.emplace(false);
	options.highpass_filter.emplace(false);
	options.stereo_swapping.emplace(false);
	options.typing_detection.emplace(false);
	options.experimental_agc.emplace(false);
	options.experimental_ns.emplace(false);
	options.residual_echo_detector.emplace(false);

	options.audio_jitter_buffer_max_packets = 1000;
	options.audio_jitter_buffer_fast_accelerate = false;
	options.audio_jitter_buffer_min_delay_ms = 0;
	options.audio_jitter_buffer_enable_rtx_handling = false;

	RtcAudioSource = peerConnectionFactory->CreateAudioSource(options);
	RtcAudioTrack = peerConnectionFactory->CreateAudioTrack(to_string(TrackId.Get("audio")), RtcAudioSource);
}

IMillicastSource::FStreamTrackInterface AudioCapturerBase::GetTrack()
{
	return RtcAudioTrack;
}

AudioGameCapturer::AudioGameCapturer() noexcept : Submix(nullptr)
{}

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

	UE_LOG(LogMillicastPublisher, Warning, TEXT("CHEVAL"));
	auto AudioDevice = GEngine->GetAudioDeviceManager()->GetAudioDevice(id);
	UE_LOG(LogMillicastPublisher, Warning, TEXT("PONEY"));
	if (!AudioDevice)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not get main audio device"));
		return nullptr;
	}

	return AudioDevice.GetAudioDevice();
}

IMillicastSource::FStreamTrackInterface AudioGameCapturer::StartCapture()
{
	auto MainAudioDevice = GetUEAudioDevice();
	if (AudioDevice == nullptr) return nullptr;

	MainAudioDevice->RegisterSubmixBufferListener(this, Submix);
	AudioDevice = MainAudioDevice;

	CreateRtcSourceTrack();

	return RtcAudioTrack;
}

void AudioGameCapturer::StopCapture()
{
	if(RtcAudioTrack) 
	{
		RtcAudioTrack = nullptr;
		RtcAudioSource = nullptr;

		AudioDevice->UnregisterSubmixBufferListener(this, Submix);
	}
}

AudioGameCapturer::~AudioGameCapturer()
{
	if (RtcAudioTrack)
	{
		StopCapture();
	}
}

void AudioGameCapturer::SetAudioSubmix(USoundSubmix* InSubmix)
{
	Submix = InSubmix;
}

void AudioGameCapturer::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	auto Adm = FWebRTCPeerConnection::GetAudioDeviceModule();
	Adm->SendAudioData(AudioData, NumSamples, NumChannels, SampleRate);
}

AudioDeviceCapture::AudioDeviceCapture() noexcept
{

}

AudioDeviceCapture::FStreamTrackInterface AudioDeviceCapture::StartCapture()
{
	CreateRtcSourceTrack();

	Audio::FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
	{
		int32 NumSamples = NumChannels * NumFrames;

		auto Adm = FWebRTCPeerConnection::GetAudioDeviceModule();
		Adm->SendAudioData(AudioData, NumSamples, NumChannels, SampleRate);
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

void AudioDeviceCapture::StopCapture()
{
	if (RtcAudioTrack)
	{
		RtcAudioSource = nullptr;
		RtcAudioTrack  = nullptr;
		AudioCapture.StopStream();
	}
}

void AudioDeviceCapture::SetAudioCaptureinfo(int32 InDeviceIndex)
{
	DeviceIndex = InDeviceIndex;
}

TArray<Audio::FCaptureDeviceInfo>& AudioDeviceCapture::GetCaptureDevicesAvailable()
{
	Audio::FAudioCapture AudioCapture;

	CaptureDevices.Empty();
	AudioCapture.GetCaptureDevicesAvailable(CaptureDevices);

	return CaptureDevices;
}
