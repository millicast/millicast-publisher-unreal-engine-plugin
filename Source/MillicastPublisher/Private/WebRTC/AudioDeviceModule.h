// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"
#include "AudioDevice.h"

#include "Sound/SoundWaveProcedural.h"

/** A custom audio device module for WebRTC. */
class FAudioDeviceModule : public webrtc::AudioDeviceModule
{
	/** Audio Sample type */
	typedef int16_t Sample;

	/** Audio device module constants */
	static constexpr int kTimePerFrameMs = 10;
	static constexpr uint8_t kNumberOfChannels = 2;
	static constexpr int kSamplesPerSecond = 48000;
	static constexpr int kTotalDelayMs = 0;
	static constexpr int kClockDriftMs = 0;
	static constexpr uint32_t kMaxVolume = 14392;
	static constexpr size_t kNumberSamples = kTimePerFrameMs * kSamplesPerSecond / 1000;
	static constexpr size_t kNumberBytesPerSample = sizeof(Sample) * kNumberOfChannels;
	static constexpr size_t kBytesPerBuffer = kNumberBytesPerSample * kNumberSamples;

	static const char kTimerQueueName[];

public:
	explicit FAudioDeviceModule(webrtc::TaskQueueFactory * QueueFactory) noexcept;

	~FAudioDeviceModule() = default;

	static rtc::scoped_refptr<FAudioDeviceModule> Create(webrtc::TaskQueueFactory * queue_factory);

public:
	void SendAudioData(const float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate);

public:
	// webrtc::AudioDeviceModule interface
	int32 ActiveAudioLayer(AudioLayer* audioLayer) const override;
	int32 RegisterAudioCallback(webrtc::AudioTransport* audioCallback) override;

	// Main initialization and termination
	int32 Init() override { return 0; }
	int32 Terminate() override { return 0; }
	bool Initialized() const override { return true; }

	// Device enumeration
	int16 PlayoutDevices() override;
	int16 RecordingDevices() override;
	int32 PlayoutDeviceName(uint16 index, char name[webrtc::kAdmMaxDeviceNameSize], char guid[webrtc::kAdmMaxGuidSize]) override;
	int32 RecordingDeviceName(uint16 index, char name[webrtc::kAdmMaxDeviceNameSize], char guid[webrtc::kAdmMaxGuidSize]) override;

	// Device selection
	int32 SetPlayoutDevice(uint16 index) override;
	int32 SetPlayoutDevice(WindowsDeviceType device) override;
	int32 SetRecordingDevice(uint16 index) override;
	int32 SetRecordingDevice(WindowsDeviceType device) override;

	// Audio transport initialization
	int32 PlayoutIsAvailable(bool* available) override;
	int32 InitPlayout() override;
	bool PlayoutIsInitialized() const override { return true; }
	int32 RecordingIsAvailable(bool* available) override;
	int32 InitRecording() override;
	bool RecordingIsInitialized() const override { return true; }

	// Audio transport control
	int32 StartPlayout() override;
	int32 StopPlayout() override;

	// True when audio is being pulled by the instance.
	bool Playing() const override;

	int32 StartRecording() override;
	int32 StopRecording() override;
	bool Recording() const override;

	// Audio mixer initialization
	int32 InitSpeaker() override { return 0; }
	bool SpeakerIsInitialized() const override { return true; }
	int32 InitMicrophone() override { return 0; }
	bool MicrophoneIsInitialized() const override { return true; }

	// Speaker volume controls
	int32 SpeakerVolumeIsAvailable(bool* available) override
	{
		return -1;
	}
	int32 SetSpeakerVolume(uint32 volume) override
	{
		return -1;
	}
	int32 SpeakerVolume(uint32* volume) const override
	{
		return -1;
	}
	int32 MaxSpeakerVolume(uint32* maxVolume) const override
	{
		return -1;
	}
	int32 MinSpeakerVolume(uint32* minVolume) const override
	{
		return -1;
	}

	// Microphone volume controls
	int32 MicrophoneVolumeIsAvailable(bool* available) override
	{
		return 0;
	}
	int32 SetMicrophoneVolume(uint32 volume) override;
	int32 MicrophoneVolume(uint32* volume) const override;
	int32 MaxMicrophoneVolume(uint32* maxVolume) const override;
	int32 MinMicrophoneVolume(uint32* minVolume) const override
	{
		return 0;
	}

	// Speaker mute control
	int32 SpeakerMuteIsAvailable(bool* available) override
	{
		return -1;
	}
	int32 SetSpeakerMute(bool enable) override
	{
		return -1;
	}
	int32 SpeakerMute(bool* enabled) const override
	{
		return -1;
	}

	// Microphone mute control
	int32 MicrophoneMuteIsAvailable(bool* available) override
	{
		*available = false;
		return -1;
	}
	int32 SetMicrophoneMute(bool enable) override
	{
		return -1;
	}
	int32 MicrophoneMute(bool* enabled) const override
	{
		return -1;
	}

	// Stereo support
	int32 StereoPlayoutIsAvailable(bool* available) const override;
	int32 SetStereoPlayout(bool enable) override { return 0; }
	int32 StereoPlayout(bool* enabled) const override;
	int32 StereoRecordingIsAvailable(bool* available) const override;
	int32 SetStereoRecording(bool enable) override;
	int32 StereoRecording(bool* enabled) const override;

	// Playout delay
	int32 PlayoutDelay(uint16* delayMS) const override;

	bool BuiltInAECIsAvailable() const override
	{
		return false;
	}
	bool BuiltInAGCIsAvailable() const override
	{
		return false;
	}
	bool BuiltInNSIsAvailable() const override
	{
		return false;
	}

	// Enables the built-in audio effects. Only supported on Android.
	int32 EnableBuiltInAEC(bool enable) override
	{
		return -1;
	}
	int32 EnableBuiltInAGC(bool enable) override
	{
		return -1;
	}
	int32 EnableBuiltInNS(bool enable) override
	{
		return -1;
	}

private:
	void Send();

private:
	bool bIsRecording;    // True when audio is being pulled by the instance.
	bool bIsRecordingInitialized;  // True when the instance is ready to pull audio.

	bool bIsStarted;
	int64_t NextFrameTime;

	rtc::TaskQueue TaskQueue;

	TArray<Sample> AudioBuffer;
	webrtc::AudioTransport * AudioTransport;

	FCriticalSection CriticalSection;
};
