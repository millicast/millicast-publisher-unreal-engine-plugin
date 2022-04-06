// Copyright Millicast 2022. All Rights Reserved.

#include "AudioDeviceModule.h"
#include "MillicastPublisherPrivate.h"
#include "common_audio/include/audio_util.h"

const char FAudioDeviceModule::kTimerQueueName[] = "FAudioDeviceModuleTimer";

FAudioDeviceModule::FAudioDeviceModule(webrtc::TaskQueueFactory* TaskQueueFactory) noexcept
	: bIsRecording(false),
	bIsRecordingInitialized(false),
	bIsStarted(false),
	NextFrameTime(0),
	TaskQueue(TaskQueueFactory->CreateTaskQueue(kTimerQueueName, webrtc::TaskQueueFactory::Priority::NORMAL))
{
	AudioTransport = nullptr;
}

rtc::scoped_refptr<FAudioDeviceModule>
FAudioDeviceModule::Create(webrtc::TaskQueueFactory* TaskQueueFactory)
{
	rtc::scoped_refptr<FAudioDeviceModule>
		AudioDeviceModule(new rtc::RefCountedObject<FAudioDeviceModule>(TaskQueueFactory));

	return AudioDeviceModule;
}

int32 FAudioDeviceModule::ActiveAudioLayer(AudioLayer* audioLayer) const
{
	*audioLayer = AudioLayer::kDummyAudio;
	return 0;
}

int32_t FAudioDeviceModule::RegisterAudioCallback(webrtc::AudioTransport* InAudioTransport)
{
	FScopeLock Lock(&CriticalSection);
	UE_LOG(LogMillicastPublisher, Display, TEXT("RegisterAudioCallback"));
	AudioTransport = InAudioTransport;
	return 0;
}

int16_t FAudioDeviceModule::PlayoutDevices()
{
	return 0;
}

int16_t FAudioDeviceModule::RecordingDevices()
{
	return 0;
}

int32_t FAudioDeviceModule::PlayoutDeviceName(uint16_t,
	char[webrtc::kAdmMaxDeviceNameSize],
	char[webrtc::kAdmMaxGuidSize])
{
	return 0;
}

int32_t FAudioDeviceModule::RecordingDeviceName(uint16_t,
	char[webrtc::kAdmMaxDeviceNameSize],
	char[webrtc::kAdmMaxGuidSize])
{
	return 0;
}

int32_t FAudioDeviceModule::SetPlayoutDevice(uint16_t)
{
	return 0;
}

int32_t FAudioDeviceModule::SetPlayoutDevice(WindowsDeviceType)
{
	return 0;
}

int32_t FAudioDeviceModule::SetRecordingDevice(uint16_t)
{
	return 0;
}

int32_t FAudioDeviceModule::SetRecordingDevice(WindowsDeviceType)
{
	return 0;
}

int32_t FAudioDeviceModule::InitPlayout()
{
	return 0;
}

int32_t FAudioDeviceModule::InitRecording()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Init recording"));

	bIsRecordingInitialized = true;

	return 0;
}

int32_t FAudioDeviceModule::StartPlayout()
{
	return 0;
}

int32_t FAudioDeviceModule::StopPlayout()
{
	return 0;
}

bool FAudioDeviceModule::Playing() const
{
	return false;
}

int32_t FAudioDeviceModule::StartRecording()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Start Recording"));

	if (bIsRecording)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("AudioDeviceModule is already recording"));
		return 0;
	}

	bIsRecording = true;

	TaskQueue.PostTask([this]() { Send(); });

	return 0;
}

int32_t FAudioDeviceModule::StopRecording()
{
	if (!bIsRecording)
	{
		return 0;
	}

	bIsRecording = false;
	bIsStarted = false;

	return 0;
}

bool FAudioDeviceModule::Recording() const
{
	return bIsRecording;
}

int32 FAudioDeviceModule::RecordingIsAvailable(bool* available)
{
	*available = true;
	return 0;
}

int32_t FAudioDeviceModule::SetMicrophoneVolume(uint32_t volume)
{
	return 0;
}

int32_t FAudioDeviceModule::MicrophoneVolume(uint32_t* volume) const
{
	*volume = 0;
	return 0;
}

int32_t FAudioDeviceModule::MaxMicrophoneVolume(
	uint32_t* max_volume) const
{
	*max_volume = kMaxVolume;
	return 0;
}

int32 RecordingIsAvailable(bool* available)
{
	*available = true;
	return 0;
}

int32 FAudioDeviceModule::PlayoutIsAvailable(bool* available)
{
	*available = false;
	return 0;
}

int32 FAudioDeviceModule::StereoPlayoutIsAvailable(bool* available) const
{
	*available = false;
	return 0;
}

int32 FAudioDeviceModule::StereoPlayout(bool* available) const
{
	*available = false;
	return 0;
}

int32 FAudioDeviceModule::StereoRecordingIsAvailable(bool* available) const
{
	*available = true;
	return 0;
}

int32 FAudioDeviceModule::SetStereoRecording(bool enable)
{
	return 0;
}

int32 FAudioDeviceModule::StereoRecording(bool* enabled) const
{
	*enabled = true;
	return 0;
}

int32_t FAudioDeviceModule::PlayoutDelay(uint16_t* delay_ms) const
{
	*delay_ms = 0;
	return 0;
}

void FAudioDeviceModule::SendAudioData(const float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate)
{
	if (!bIsRecordingInitialized)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("AudioDeviceModule has not been iniatilized"));
		return;
	}

	if (kSamplesPerSecond != SampleRate)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("AudioDeviceModule supports only %d Hz"), kSamplesPerSecond);
		return;
	}

	Audio::TSampleBuffer<float> Buffer(AudioData, NumSamples, NumChannels, SampleRate);
	if (Buffer.GetNumChannels() != kNumberOfChannels)
	{
		Buffer.MixBufferToChannels(kNumberOfChannels);
	}
	
	const float* Data = Buffer.GetData();

	{
		FScopeLock Lock(&CriticalSection);
		const auto num = AudioBuffer.Num();
		AudioBuffer.AddZeroed(Buffer.GetNumSamples());

		webrtc::FloatToS16(AudioData, Buffer.GetNumSamples(), AudioBuffer.GetData() + num);
	}
}

void FAudioDeviceModule::Send()
{
	RTC_DCHECK_RUN_ON(&TaskQueue);
	{
		FScopeLock Lock(&CriticalSection);

		if (bIsRecording)
		{
			if (!bIsStarted)
			{
				NextFrameTime = rtc::TimeMillis();
				bIsStarted = true;
			}

			if (AudioBuffer.Num() >= kNumberSamples * kNumberOfChannels)
			{
				uint32_t micLevel = 0;
				AudioTransport->RecordedDataIsAvailable(AudioBuffer.GetData(), kNumberSamples, sizeof(Sample),
					kNumberOfChannels, kSamplesPerSecond, 0, 0, micLevel, false, micLevel);
				AudioBuffer.RemoveAt(0, kNumberSamples * kNumberOfChannels, true);
			}
			NextFrameTime += kTimePerFrameMs;
			const int64_t current_time = rtc::TimeMillis();
			const int64_t wait_time =
				(NextFrameTime > current_time) ? NextFrameTime - current_time : 0;
			TaskQueue.PostDelayedTask([this]() { Send(); }, int32_t(wait_time));
		}
	}
}
