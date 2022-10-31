// Copyright Millicast 2022. All Rights Reserved.

#include "AudioDeviceModule.h"
#include "MillicastPublisherPrivate.h"

FAudioDeviceModule::FAudioDeviceModule() noexcept 
{}

rtc::scoped_refptr<FAudioDeviceModule> FAudioDeviceModule::Create()
{
	rtc::scoped_refptr<FAudioDeviceModule>
		AudioDeviceModule(new rtc::RefCountedObject<FAudioDeviceModule>());

	return AudioDeviceModule;
}

int32 FAudioDeviceModule::ActiveAudioLayer(AudioLayer* audioLayer) const
{
	*audioLayer = AudioLayer::kDummyAudio;
	return 0;
}

int32_t FAudioDeviceModule::RegisterAudioCallback(webrtc::AudioTransport* InAudioTransport)
{
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

	return 0;
}

int32_t FAudioDeviceModule::StopRecording()
{
	return 0;
}

bool FAudioDeviceModule::Recording() const
{
	return true;
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
