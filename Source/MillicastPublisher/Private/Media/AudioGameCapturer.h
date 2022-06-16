// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "IMillicastSource.h"

#include <AudioCaptureCore.h>
#include <AudioResampler.h>

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
	TOptional<Audio::FDeviceId> DeviceId;

public:
	AudioGameCapturer() noexcept;
	~AudioGameCapturer();

	/** Just create audio source and audio track. The audio capture is started by WebRTC in the audio device module */
	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	/** Set the submix to attach a callback to. nullptr means master submix */
	void SetAudioSubmix(USoundSubmix* InSubmix = nullptr);

	/** Set the device id to use */
	void SetAudioDeviceId(Audio::FDeviceId Id);

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

	float VolumeMultiplier; // dB

public:
	AudioDeviceCapture() noexcept;

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

	void SetVolumeMultiplier(float f) noexcept { VolumeMultiplier = f;  }

	static TArray<Audio::FCaptureDeviceInfo>& GetCaptureDevicesAvailable();
};

#if PLATFORM_WINDOWS

#include <windows.h>
#include <mmreg.h>

#define INITGUID //<< avoid additional linkage of static libs, uneeded code will be optimized out
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#undef INITGUID

#include <Audioclient.h>
#include <Audiopolicy.h>

/** Class to capture audio from a windows system audio device */
class WasapiDeviceCapture : public AudioCapturerBase
{
	FCriticalSection CriticalSection;

	static constexpr size_t kNumChansMin = 1;		//!< mono
	static constexpr size_t kNumChansDef = 2;		//!< stereo
	static constexpr size_t kNumChansMax = 8;		//!< multichannel later

	static const size_t kSampleRateMin = 22050;
	static const size_t kSampleRateMax = 192000;
	static const size_t kOpusSampleRate = 48000;

	static const size_t kSampleBitsFloat = 31;		//!< hack for float bit depth

	IMMDevice* device_ = nullptr;
	IAudioClient* client_ = nullptr;
	IAudioRenderClient* render_ = nullptr;
	IAudioCaptureClient* capture_ = nullptr;
	WAVEFORMATEX* format_ = nullptr;						//!< mix format (i.e. format *after* LFX)

	uint32_t				bufferLen_ = 0;							//!< samples frames in the endpoint buffer

	std::wstring			deviceID_;
	std::wstring			deviceName_;

	volatile bool			runStatus_ = false;
	volatile bool			released_ = false;						//!< async delete pending
	bool					interleaved_ = false;
	bool					exclusive_ = false;
	bool					defaultDevice_ = false;
	size_t					intSampleRate_ = 0;						//!< Client sample rate in Hz
	size_t					devSampleRate_ = 0;						//!< Device sample rate in Hz
	size_t					devBitsPerSample_ = 0;					//!< Device sample bit depth
	size_t                  numChans_ = 1;
	size_t                  tickRate_ = 100;

	float resamplingFactor_ = 0.f;				//!< if needed
	Audio::AlignedFloatBuffer    AudioBuffer;
	Audio::FResamplingParameters ResamplingParameters;

	float* ConvertToFloatSample(void* pData, size_t numFramesAvailable, size_t numCaptureChannels);
	void SendAudioData(const float* pinf, size_t numFramesAvailable, size_t numCaptureChannels);

public:
	WasapiDeviceCapture(size_t tickRate, bool loopback) noexcept 
		: ResamplingParameters{ Audio::EResamplingMethod::FastSinc, 0, 0, 0, AudioBuffer }
	{
		Initialize(tickRate, loopback);
	}

	~WasapiDeviceCapture() noexcept
	{
		Finalize();
	}

	FStreamTrackInterface StartCapture() override;
	void StopCapture() override;

	bool Initialize(size_t tickRate, bool loopback);
	bool IsInterleaved() const { return interleaved_; }
	bool IsExclusive() const { return exclusive_; }
	bool IsReleased() const { return released_; }
	void Finalize();

	void OnTick();

	bool IsInitialized() const { return device_ != nullptr && client_ != nullptr && format_ != nullptr /*&& frameQ_ != nullptr*/; }

	const std::wstring& GetDeviceID() const { return deviceID_; }

	static bool ColdInit();
	static void ColdExit();
};

#endif
