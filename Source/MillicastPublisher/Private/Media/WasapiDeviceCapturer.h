// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "AudioCapturerBase.h"

#if PLATFORM_WINDOWS

// When including Windows headers with Unreal Engine code the PreWindowsApi.h and PostWindowsApi.h headers should wrap the includes.
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#include <windows.h>
#include <mmreg.h>

#define INITGUID //<< avoid additional linkage of static libs, uneeded code will be optimized out
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#undef INITGUID

#include <Audioclient.h>
#include <Audiopolicy.h>
#include <AudioResampler.h>

namespace Millicast::Publisher
{
	/** Class to capture audio from a windows system audio device */
	class WasapiDeviceCapturer : public AudioCapturerBase
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
		WasapiDeviceCapturer(size_t tickRate, bool loopback) noexcept
			: ResamplingParameters{ Audio::EResamplingMethod::FastSinc, 0, 0, 0, AudioBuffer }
		{
			Initialize(tickRate, loopback);
		}

		virtual ~WasapiDeviceCapturer() noexcept override
		{
			Finalize();
		}

		FStreamTrackInterface StartCapture(UWorld* InWorld) override;
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

}

// This undefines some Windows code so UE code that clashes can compile without issue.
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"

#endif // PLATFORM_WINDOWS
