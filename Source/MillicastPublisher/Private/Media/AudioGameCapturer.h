// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "IMillicastSource.h"
#include <AudioCaptureCore.h>

class AudioCapturerBase : public IMillicastAudioSource
{
protected:
	rtc::scoped_refptr<webrtc::AudioSourceInterface> RtcAudioSource;
	FStreamTrackInterface                            RtcAudioTrack;

	void CreateRtcSourceTrack();
public:
	AudioCapturerBase() noexcept;
	FStreamTrackInterface GetTrack() override;

};

/** Class to capturer audio from the main audio device */
class AudioGameCapturer : public AudioCapturerBase, public ISubmixBufferListener
{	
	FAudioDevice* AudioDevice;
	USoundSubmix* Submix;

public:
	AudioGameCapturer() noexcept;
	~AudioGameCapturer();

	/** Just create audio source and audio track. The audio capture is started by WebRTC in the audio device module */
	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	/** Set the submix to attach a callback to. nullptr means master submix */
	void SetAudioSubmix(USoundSubmix* InSubmix = nullptr);

	/** Called by the main audio device when a new audio data buffer is ready */
	void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
		int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
};

/** Class to capture audio from a system audio device */
class AudioDeviceCapture : public AudioCapturerBase
{
	static TArray<Audio::FCaptureDeviceInfo> CaptureDevices;

	int32                     DeviceIndex;
	Audio::FAudioCapture      AudioCapture;

public:
	AudioDeviceCapture() noexcept;

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	/**
	* Set the audio device to use. 
	* The device index is the index in the CaptureDevice info list return by GetCaptureDevicesAvailable.
	*/
	void SetAudioCaptureinfo(int32 InDeviceIndex);

	static TArray<Audio::FCaptureDeviceInfo>& GetCaptureDevicesAvailable();
};