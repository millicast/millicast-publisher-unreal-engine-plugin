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
	RtcAudioTrack  = peerConnectionFactory->CreateAudioTrack(to_string(TrackId.Get("audio")), RtcAudioSource);
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

	auto AudioDevice = GEngine->GetAudioDeviceManager()->GetAudioDevice(id);
	if (!AudioDevice)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not get main audio device"));
		return nullptr;
	}

	return AudioDevice.GetAudioDevice();
}

IMillicastSource::FStreamTrackInterface AudioGameCapturer::StartCapture()
{
	if (DeviceId.IsSet())
	{
		AudioDevice = GetUEAudioDevice(DeviceId.GetValue());
	}
	else
	{
		AudioDevice = GetUEAudioDevice();
	}
	
	if (AudioDevice == nullptr) return nullptr;

	AudioDevice->RegisterSubmixBufferListener(this, Submix);

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

void AudioGameCapturer::SetAudioDeviceId(Audio::FDeviceId Id)
{
	DeviceId = Id;
}

void AudioGameCapturer::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	auto Adm = FWebRTCPeerConnection::GetAudioDeviceModule();
	Adm->SendAudioData(AudioData, NumSamples, NumChannels, SampleRate);
}

AudioDeviceCapture::AudioDeviceCapture() noexcept : VolumeMultiplier(0.f)
{}

AudioDeviceCapture::FStreamTrackInterface AudioDeviceCapture::StartCapture()
{
	CreateRtcSourceTrack();

	Audio::FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
	{
		int32 NumSamples = NumFrames * NumChannels;

		// UE_LOG(LogMillicastPublisher, Display, 
		// 	TEXT("%d %d %d %d %f"), NumFrames, NumChannels, SampleRate, NumSamples, StreamTime);

		float* MutableAudioData = new float[NumSamples];

		for (int i = 0; i < NumSamples; ++i) {
			const float factor = pow(10.0f, VolumeMultiplier / 20.f);
			MutableAudioData[i] = FMath::Clamp(AudioData[i] * factor, -1.f, 1.f);
		}

		auto Adm = FWebRTCPeerConnection::GetAudioDeviceModule();
		Adm->SendAudioData(MutableAudioData, NumSamples, NumChannels, SampleRate);

		delete[] MutableAudioData;
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

void AudioDeviceCapture::SetAudioCaptureDevice(int32 InDeviceIndex)
{
	DeviceIndex = InDeviceIndex;
}

void AudioDeviceCapture::SetAudioCaptureDeviceById(FStringView Id)
{
	GetCaptureDevicesAvailable();

	// Find index the given device id
	auto it = CaptureDevices.IndexOfByPredicate([&Id](const auto& e) { return Id == e.DeviceId; });

	// if it exists
	if (it != INDEX_NONE)
	{
		SetAudioCaptureDevice(it);
	}
}

void AudioDeviceCapture::SetAudioCaptureDeviceByName(FStringView Name)
{
	GetCaptureDevicesAvailable();

	// Find index the given device id
	auto it = CaptureDevices.IndexOfByPredicate([&Name](const auto& e) { return Name == e.DeviceName; });

	// if it exists
	if (it != INDEX_NONE)
	{
		SetAudioCaptureDevice(it);
	}
}

TArray<Audio::FCaptureDeviceInfo>& AudioDeviceCapture::GetCaptureDevicesAvailable()
{
	Audio::FAudioCapture AudioCapture;

	CaptureDevices.Empty();
	AudioCapture.GetCaptureDevicesAvailable(CaptureDevices);

	return CaptureDevices;
}
