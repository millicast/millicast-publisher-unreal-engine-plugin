// Copyright Millicast 2022. All Rights Reserved.
#include "AudioGameCapturer.h"

#include "MillicastPublisherPrivate.h"
#include "WebRTC/PeerConnection.h"

#include "Util.h"

TArray<Audio::FCaptureDeviceInfo> AudioDeviceCapture::CaptureDevices;

IMillicastAudioSource* IMillicastAudioSource::Create(AudioCapturerType CapturerType)
{
	switch (CapturerType)
	{
	case AudioCapturerType::SUBMIX: return new AudioGameCapturer;
	case AudioCapturerType::DEVICE: return new AudioDeviceCapture;
#if PLATFORM_WINDOWS
	case AudioCapturerType::LOOPBACK: return new WasapiDeviceCapture(10, true);
#else
	case AudioCapturerType::LOOPBACK: return nullptr;
#endif
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
	options.auto_gain_control.emplace(true);
	options.noise_suppression.emplace(false);
	options.highpass_filter.emplace(true);
	options.stereo_swapping.emplace(false);
	options.typing_detection.emplace(false);
	options.experimental_agc.emplace(false);
	options.experimental_ns.emplace(false);
	options.residual_echo_detector.emplace(false);

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

#if PLATFORM_WINDOWS

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
/** @file WasapiDeviceCapture.cpp
 *  @copyright Copyright (C) 2015-2021  RedpillVR
 */
 //-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "mmeapi.h"
#include <codecvt>
#include <locale>

#include <timeapi.h>

#ifndef WIN32
#error Windows only
#endif

#include <avrt.h>

#define _WIN32_DCOM

#undef SAFE_RELEASE

#define REFTIMES_PER_SEC			10000000
#define REFTIMES_PER_MILLISEC		10000
#define MAX_STR_LEN					512
#define EXIT_ON_ERROR(hres)			if (FAILED(hres)) { UE_LOG(LogMillicastPublisher, Error, TEXT("%s"), getError(hres)); goto Exit; }
#define SAFE_RELEASE(punk)			if ((punk) != nullptr) { (punk)->Release(); (punk) = nullptr; }

#define WASAPI_EXCLUSIVE_LINE_IN	1

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")

extern const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
extern const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
extern const IID IID_IAudioClient = __uuidof(IAudioClient);
extern const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
extern const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
extern const IID IID_IAudioClock = __uuidof(IAudioClock);

enum LogicalEP
{
	InputVoice,
	InputMusic,
	NumLogicalEP
};

template<typename T>
T nextPow2(T value)
{
	static_assert(std::is_integral<T>::value, "Integral required.");
	T ret = 0b1;

	while (ret < value)
	{
		ret <<= 1;
	}

	return ret;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static const char* getError(HRESULT hr)
{
	const char* text = nullptr;

	switch (hr)
	{
	case S_OK:    text = "";
	case E_POINTER:    text = "E_POINTER"; break;
	case E_INVALIDARG:    text = "E_INVALIDARG"; break;

	case AUDCLNT_E_NOT_INITIALIZED:    text = "AUDCLNT_E_NOT_INITIALIZED"; break;
	case AUDCLNT_E_ALREADY_INITIALIZED:    text = "AUDCLNT_E_ALREADY_INITIALIZED"; break;
	case AUDCLNT_E_WRONG_ENDPOINT_TYPE:    text = "AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
	case AUDCLNT_E_DEVICE_INVALIDATED:    text = "AUDCLNT_E_DEVICE_INVALIDATED"; break;
	case AUDCLNT_E_NOT_STOPPED:    text = "AUDCLNT_E_NOT_STOPPED"; break;
	case AUDCLNT_E_BUFFER_TOO_LARGE:    text = "AUDCLNT_E_BUFFER_TOO_LARGE"; break;
	case AUDCLNT_E_OUT_OF_ORDER:    text = "AUDCLNT_E_OUT_OF_ORDER"; break;
	case AUDCLNT_E_UNSUPPORTED_FORMAT:    text = "AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
	case AUDCLNT_E_INVALID_SIZE:    text = "AUDCLNT_E_INVALID_SIZE"; break;
	case AUDCLNT_E_DEVICE_IN_USE:    text = "AUDCLNT_E_DEVICE_IN_USE"; break;
	case AUDCLNT_E_BUFFER_OPERATION_PENDING:    text = "AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
	case AUDCLNT_E_THREAD_NOT_REGISTERED:    text = "AUDCLNT_E_THREAD_NOT_REGISTERED"; break;
	case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:    text = "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED"; break;
	case AUDCLNT_E_ENDPOINT_CREATE_FAILED:    text = "AUDCLNT_E_ENDPOINT_CREATE_FAILED"; break;
	case AUDCLNT_E_SERVICE_NOT_RUNNING:    text = "AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
	case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED:    text = "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED"; break;
	case AUDCLNT_E_EXCLUSIVE_MODE_ONLY:    text = "AUDCLNT_E_EXCLUSIVE_MODE_ONLY"; break;
	case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:    text = "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"; break;
	case AUDCLNT_E_EVENTHANDLE_NOT_SET:    text = "AUDCLNT_E_EVENTHANDLE_NOT_SET"; break;
	case AUDCLNT_E_INCORRECT_BUFFER_SIZE:    text = "AUDCLNT_E_INCORRECT_BUFFER_SIZE"; break;
	case AUDCLNT_E_BUFFER_SIZE_ERROR:    text = "AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
	case AUDCLNT_E_CPUUSAGE_EXCEEDED:    text = "AUDCLNT_E_CPUUSAGE_EXCEEDED"; break;

#ifdef AUDCLNT_E_BUFFER_ERROR
	case AUDCLNT_E_BUFFER_ERROR:    text = "AUDCLNT_E_BUFFER_ERROR"; break;
#endif

#ifdef AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED
	case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:    text = "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED"; break;
#endif

#ifdef AUDCLNT_E_INVALID_DEVICE_PERIOD
	case AUDCLNT_E_INVALID_DEVICE_PERIOD:    text = "AUDCLNT_E_INVALID_DEVICE_PERIOD"; break;
#endif

	case AUDCLNT_S_BUFFER_EMPTY:    text = "AUDCLNT_S_BUFFER_EMPTY"; break;
	case AUDCLNT_S_THREAD_ALREADY_REGISTERED:    text = "AUDCLNT_S_THREAD_ALREADY_REGISTERED"; break;
	case AUDCLNT_S_POSITION_STALLED:    text = "AUDCLNT_S_POSITION_STALLED"; break;

		// other windows common errors:
	case CO_E_NOTINITIALIZED:    text = "CO_E_NOTINITIALIZED"; break;

	default:
		text = "UNKNOWN ERROR";
	}

	return text;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct DeviceDesc
{
		DeviceDesc() = default;

		std::wstring DeviceID;
		std::wstring DeviceName;

		REFERENCE_TIME DefaultDevicePeriod;
		REFERENCE_TIME MinimumDevicePeriod;

		WAVEFORMATEXTENSIBLE MixFormat;
		EndpointFormFactor FormFactor;

		bool isSystemDefault = false;
		bool isCommunicationDefault = false;
};

class WasapiCore
{
public:

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------
	WasapiCore() = default;
	~WasapiCore() = default;

	IMMDeviceEnumerator* deviceEnumerator_ = nullptr;

	std::vector<DeviceDesc> inputDeviceList_;

	HANDLE hEvent_ = nullptr;
	HANDLE hTask_ = nullptr;
	HANDLE timer_ = nullptr;

	bool initialized_ = false;

	WasapiDeviceCapture* activStreams_[LogicalEP::NumLogicalEP] = { nullptr };
	std::wstring	devId_[LogicalEP::NumLogicalEP];
	std::wstring	defId_[LogicalEP::NumLogicalEP];

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	const DeviceDesc* findDeviceByID(const std::wstring& devID)
	{
		if (devID.empty())
		{
			return nullptr;
		}

		const DeviceDesc* devices = inputDeviceList_.data();

		for (size_t i = 0; i < inputDeviceList_.size(); i++)
		{
			if (devices[i].DeviceID == devID) {
				return &devices[i];
			}
		}

		return nullptr;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	const DeviceDesc* findDeviceByFriendlyName(const std::wstring& devName)
	{
		if (devName.empty())
		{
			return nullptr;
		}

		const DeviceDesc* devices = inputDeviceList_.data();

		for (size_t i = 0; i < inputDeviceList_.size(); i++)
		{
			if (devices[i].DeviceName == devName) {
				return &devices[i];
			}
		}

		return nullptr;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	void prepareWaveFormat(WAVEFORMATEXTENSIBLE& wf, size_t channels, size_t sampleRate, size_t bits, size_t validBits)
	{
		// Set up wave format structure. 
		ZeroMemory(&wf, sizeof(WAVEFORMATEXTENSIBLE));

		wf.Format.nChannels = WORD(channels);
		wf.Format.wBitsPerSample = WORD(bits);
		wf.Format.nSamplesPerSec = DWORD(sampleRate);
		wf.Format.nBlockAlign = wf.Format.nChannels * wf.Format.wBitsPerSample / 8;
		wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;

		if (bits <= 16)
		{
			wf.Format.wFormatTag = WAVE_FORMAT_PCM;
			wf.Format.cbSize = 0;
		}
		else
		{
			wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			wf.Format.cbSize = 0x16;
			wf.Samples.wValidBitsPerSample = WORD(validBits);

			if (channels == 2) {
				wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
			}
			else if (channels == 4) {
				wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
			}
			else if (channels == 6) {
				wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
			}
			else if (channels == 8) {
				wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER;
			}

			if (validBits == 32) {
				wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
			}
			else {
				wf.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	void EnumerateDevices(IMMDeviceEnumerator* pEnumerator, std::vector<DeviceDesc>& portList, EDataFlow endpointType=eCapture)
	{
		IMMDeviceCollection* pCollection = nullptr;
		IMMDevice* pEndpoint = nullptr;
		IPropertyStore* pProps = nullptr;
		IAudioClient* client = nullptr;
		WCHAR* pszDeviceId = nullptr;
		LPWSTR defaultSystemID = nullptr;
		LPWSTR defaultCommunicationsID = nullptr;
		LPWSTR defaultLineInID = nullptr;

		HRESULT hr = pEnumerator->EnumAudioEndpoints(endpointType, DEVICE_STATE_ACTIVE, &pCollection);
		EXIT_ON_ERROR(hr);

		hr = pEnumerator->GetDefaultAudioEndpoint(endpointType, ERole::eConsole, &pEndpoint);
		EXIT_ON_ERROR(hr);

		hr = pEndpoint->GetId(&defaultSystemID);
		EXIT_ON_ERROR(hr);
		SAFE_RELEASE(pEndpoint);

		hr = pEnumerator->GetDefaultAudioEndpoint(endpointType, ERole::eCommunications, &pEndpoint);
		EXIT_ON_ERROR(hr);

		hr = pEndpoint->GetId(&defaultCommunicationsID);
		EXIT_ON_ERROR(hr);
		SAFE_RELEASE(pEndpoint);

		// LiveInput line in
		if (endpointType == EDataFlow::eCapture)
		{
			hr = pEnumerator->GetDefaultAudioEndpoint(EDataFlow::eCapture, ERole::eMultimedia, &pEndpoint);
			EXIT_ON_ERROR(hr);

			hr = pEndpoint->GetId(&defaultLineInID);
			EXIT_ON_ERROR(hr);
			SAFE_RELEASE(pEndpoint);
		}

		portList.resize(0);

		UINT count;
		hr = pCollection->GetCount(&count);
		EXIT_ON_ERROR(hr)

			// Each loop prints the name of an endpoint device.
			for (ULONG i = 0; i < count; i++)
			{
				bool isLineInDefault = false;

				// Get pointer to endpoint number i.
				hr = pCollection->Item(i, &pEndpoint);
				EXIT_ON_ERROR(hr)

					// Get the endpoint ID string.
					hr = pEndpoint->GetId(&pszDeviceId);
				EXIT_ON_ERROR(hr)

					// make sure the device is active
					DWORD state = 0;
				hr = pEndpoint->GetState(&state);
				EXIT_ON_ERROR(hr)

					if (state == DEVICE_STATE_ACTIVE)
					{
						hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
						EXIT_ON_ERROR(hr)

							DeviceDesc devDesc;

						{// "Friendly" Name
							PROPVARIANT vars;
							PropVariantInit(&vars);

							hr = pProps->GetValue(PKEY_Device_FriendlyName, &vars);
							EXIT_ON_ERROR(hr)

								devDesc.DeviceID = pszDeviceId;
							devDesc.DeviceName = vars.pwszVal;
							devDesc.isSystemDefault = defaultSystemID != nullptr && devDesc.DeviceID == defaultSystemID;
							devDesc.isCommunicationDefault = defaultCommunicationsID != nullptr && devDesc.DeviceID == defaultCommunicationsID;
						}

						{// EndpointFormFactor
							PROPVARIANT vars;
							PropVariantInit(&vars);

							hr = pProps->GetValue(PKEY_AudioEndpoint_FormFactor, &vars);
							EXIT_ON_ERROR(hr);

							devDesc.FormFactor = (EndpointFormFactor)vars.uiVal;
							PropVariantClear(&vars);
						}

						{// Default format
							PROPVARIANT vars;
							PropVariantInit(&vars);

							hr = pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &vars);
							EXIT_ON_ERROR(hr);

							ZeroMemory(&devDesc.MixFormat, sizeof(WAVEFORMATEXTENSIBLE));
							memcpy(&devDesc.MixFormat, vars.blob.pBlobData, std::min<ULONG>(sizeof(WAVEFORMATEXTENSIBLE), vars.blob.cbSize));
							PropVariantClear(&vars);
						}

						{//lantency
							hr = pEndpoint->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&client);
							EXIT_ON_ERROR(hr)

								hr = client->GetDevicePeriod(&devDesc.DefaultDevicePeriod, &devDesc.MinimumDevicePeriod);
							EXIT_ON_ERROR(hr)
						}

						{// Format
							WAVEFORMATEX* pwft;
							hr = client->GetMixFormat(&pwft);
							EXIT_ON_ERROR(hr);

							memcpy(&devDesc.MixFormat, pwft, std::min<ULONG>(sizeof(WAVEFORMATEX) + pwft->cbSize, sizeof(WAVEFORMATEXTENSIBLE)));

							if (pwft->cbSize == 0) {
								devDesc.MixFormat.Samples.wValidBitsPerSample = pwft->wBitsPerSample;
							}

							::CoTaskMemFree(pwft);
						}

						if (devDesc.isCommunicationDefault)
						{
							defId_[LogicalEP::InputVoice] = devDesc.DeviceID;
							UE_LOG(LogMillicastPublisher, Log, TEXT("System default communications recording device: %S - %S"),
								devDesc.DeviceName.c_str(), devDesc.DeviceID.c_str());
						}
						

						if (isLineInDefault && !devDesc.isCommunicationDefault)
						{
							defId_[LogicalEP::InputMusic] = devDesc.DeviceID;
							UE_LOG(LogMillicastPublisher, Log, TEXT("System default recording device: %S - %S"),
								devDesc.DeviceName.c_str(), devDesc.DeviceID.c_str());
						}

						portList.push_back(devDesc);


						SAFE_RELEASE(client);
						SAFE_RELEASE(pProps);

					}

				SAFE_RELEASE(pEndpoint);

				CoTaskMemFree(pszDeviceId);
				pszDeviceId = nullptr;
			}

	Exit:

		CoTaskMemFree(pszDeviceId);

		SAFE_RELEASE(client);
		SAFE_RELEASE(pProps);
		SAFE_RELEASE(pEndpoint);
		SAFE_RELEASE(pCollection);
	}


	//-------------------------------------------------------------------------------------------------------------------------------------------------------------

	bool ColdInit();
	//-------------------------------------------------------------------------------------------------------------------------------------------------------------
	void ColdExit()
	{
		// since we used TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS for timeSetEvent(), we're guarantied the timer will stop right away

		if (hTask_ != NULL)
		{
			AvRevertMmThreadCharacteristics(hTask_);
			hTask_ = NULL;
		}

		SAFE_RELEASE(deviceEnumerator_);

		CoUninitialize();

		initialized_ = false;
	}


	//-------------------------------------------------------------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------
	void StartTickingStream(WasapiDeviceCapture* pstream, LogicalEP lep, UINT tickPeriod)
	{
		if (pstream == nullptr || lep >= LogicalEP::NumLogicalEP || tickPeriod == 0)
		{
			return;
		}

		// will from now on be ticked
		activStreams_[lep] = pstream;
	}

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------
	void Tick()
	{
		if (!timer_)
		{
			return;
		}

		for (size_t i = 0; i < LogicalEP::NumLogicalEP; i++)
		{
			if (activStreams_[i] != nullptr)
			{
				if (!activStreams_[i]->IsReleased())
				{
					activStreams_[i]->OnTick();
				}
				else
				{
					delete activStreams_[i];
					activStreams_[i] = nullptr;
				}
			}
		}
	}
}
static sWcore;


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void CALLBACK coreCallback(PVOID /*lpParameter*/, BOOLEAN /*TimerOrWaitFired*/)
{
	sWcore.Tick();
}

bool WasapiCore::ColdInit()
{
	if (initialized_)
	{
		ColdExit();
	}

	// check OS version: we need Microsoft Windows Vista or later
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 1;

	DWORD dwTypeMask = VER_MAJORVERSION | VER_MINORVERSION;
	DWORDLONG dwlConditionMask = 0;

	dwlConditionMask = VerSetConditionMask(dwlConditionMask, VER_MAJORVERSION, VER_GREATER);
	dwlConditionMask = VerSetConditionMask(dwlConditionMask, VER_MINORVERSION, VER_GREATER);

	bool isMinimumRequired = VerifyVersionInfo(&osvi, dwTypeMask, dwlConditionMask);
	if (!isMinimumRequired)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Windows Vista or later is required!"));
		return false;
	}

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// Initialize device enumerator
	if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&deviceEnumerator_)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't create IMMDeviceEnumerator interface!"));
		return false;
	}

	// Ask MMCSS to temporarily boost the thread priority
	// to reduce glitches while the low-latency stream plays.
	DWORD taskIndex = 0;
	hTask_ = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (hTask_ == NULL)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't increase audio thread priority!"));
	}

	initialized_ = true;
	return true;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
bool WasapiDeviceCapture::Initialize(size_t tickRate , bool loopback)
{
	if (!sWcore.initialized_)
	{
		return false;
	}

	Finalize();

	exclusive_ = false;
	tickRate_ = tickRate;

	// Get audio endpoint
	HRESULT hr;
	if (FAILED(hr = sWcore.deviceEnumerator_->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eConsole, &device_)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::Initialize: Couldn't get default endpoint! (%s)"), getError(hr));
		return false;
	}

	// Obtain endpoint's IAudioClient interface
	if (FAILED(hr = device_->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&client_)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::Initialize: Couldn't obtain IAudioClient interface! (%s)"), getError(hr));
		return false;
	}

	// samplerate: force our internal rate to match WASAPI's -> let user know if different from desired setup
	// we stick to the default device so that the user can select which one they want to use in the control panel
	if (FAILED(hr = client_->GetMixFormat(&format_)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::Initialize: Couldn't obtain mixer format! (%s)"), getError(hr));
		return false;
	}

	devSampleRate_ = format_->nSamplesPerSec;

	WAVEFORMATEX* pfmt = nullptr;

	pfmt = format_;
	devBitsPerSample_ = pfmt->wBitsPerSample;

	ResamplingParameters.ResamplerMethod = Audio::EResamplingMethod::FastSinc;
	ResamplingParameters.NumChannels = format_->nChannels;
	ResamplingParameters.SourceSampleRate = format_->nSamplesPerSec;
	ResamplingParameters.DestinationSampleRate = kOpusSampleRate;

	if (pfmt->wBitsPerSample == 32 && ((WAVEFORMATEXTENSIBLE*)pfmt)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT || pfmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		devBitsPerSample_ = kSampleBitsFloat;
	}

	size_t frameLenPerChan = nextPow2(static_cast<size_t>(format_->nSamplesPerSec / float(tickRate)));
	REFERENCE_TIME hnsRequestedDuration = (REFERENCE_TIME)REFTIMES_PER_SEC * frameLenPerChan / format_->nSamplesPerSec;

	// allocate the audio endpoint buffer
	if (FAILED(hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pfmt, NULL)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::Initialize: Couldn't initialize IAudioClient! (%s)"), getError(hr));
		return false;
	}

	// Get the actual size in samples of the allocated endpoint buffer.
	client_->GetBufferSize(&bufferLen_);

	// get recording service
	if (FAILED(hr = client_->GetService(IID_IAudioCaptureClient, (void**)&capture_)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::Initialize: Couldn't initialize IAudioCaptureClient! (%s)"), getError(hr));
		return false;
	}

	UE_LOG(LogMillicastPublisher, Log, TEXT("AudioDriver::Initialize: SUCCESS"));

	return true;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
WasapiDeviceCapture::FStreamTrackInterface WasapiDeviceCapture::StartCapture()
{
	if (!sWcore.initialized_ || !IsInitialized())
	{
		return nullptr;
	}

	runStatus_ = true;

	REFERENCE_TIME Tdef, Tmin;

	if (FAILED(client_->GetDevicePeriod(&Tdef, &Tmin)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::StartStream( %s ): Failed to get device period!"), numChans_ == 2 ? "music input" : "voice input");
		return nullptr;
	}

	LogicalEP lep = numChans_ == 2 ? LogicalEP::InputMusic : LogicalEP::InputVoice;

	auto period = UINT(Tmin / 10000);
	BOOL success = CreateTimerQueueTimer(&sWcore.timer_, NULL, (WAITORTIMERCALLBACK)coreCallback, this, period / 2, period / 2, NULL);

	sWcore.StartTickingStream(this, lep, UINT(Tmin / 10000));

	if (FAILED(client_->Start())) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("AudioDriver::StartStream( %s ): could not start!"),numChans_ == 2 ? "music input" : "voice input");
	}
	else {
		UE_LOG(LogMillicastPublisher, Log, TEXT("AudioDriver::StartStream( %s ): SUCCESS"), numChans_ == 2 ? "music input" : "voice input");
	}

	CreateRtcSourceTrack();

	return RtcAudioTrack;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void WasapiDeviceCapture::StopCapture()
{
	if (!runStatus_)
	{
		return;
	}

	if (sWcore.timer_)
	{
		DeleteTimerQueueTimer(NULL, sWcore.timer_, NULL);
		sWcore.timer_ = nullptr;
	}

	runStatus_ = false;

	// Stop playing.
	if (client_ != nullptr)
	{
		client_->Stop();
	}

	RtcAudioTrack = nullptr;
	RtcAudioSource = nullptr;
	// we keep ticking the stream until it is released in the callback but tick will do nothing
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void WasapiDeviceCapture::Finalize()
{
	StopCapture();

	if (format_ != nullptr)
	{
		CoTaskMemFree(format_);
		format_ = nullptr;
	}

	SAFE_RELEASE(capture_)
	SAFE_RELEASE(render_)
	SAFE_RELEASE(client_)
	SAFE_RELEASE(device_)

	deviceID_.clear();

	bufferLen_ = 0;
	runStatus_ = false;
	interleaved_ = false;
	exclusive_ = false;
	defaultDevice_ = false;
	intSampleRate_ = 0;
	devSampleRate_ = 0;
	devBitsPerSample_ = 0;
}

float* WasapiDeviceCapture::ConvertToFloatSample(void* pData, size_t numFramesAvailable, size_t numCaptureChannels)
{
	float* pinf = reinterpret_cast<float*>(pData);

	// sample type conversion
	if (devBitsPerSample_ == 32)
	{
		int32_t* pin32 = reinterpret_cast<int32_t*>(pData);

		for (size_t i = 0; i < size_t(numFramesAvailable) * numCaptureChannels; i++)
		{
			pinf[i] = float(pin32[i]) / 2147483648.f;
		}
	}
	else if (devBitsPerSample_ == 24)
	{
		int32_t* pin24 = reinterpret_cast<int32_t*>(pData);

		for (size_t i = 0; i < size_t(numFramesAvailable) * numCaptureChannels; i++)
		{
			pinf[i] = float(pin24[i] >> 8) / 8388608.f;
		}
	}
	else if (devBitsPerSample_ == 16)
	{
		int16_t* pin16 = reinterpret_cast<int16_t*>(pData);

		for (size_t i = 0; i < size_t(numFramesAvailable) * numCaptureChannels; i++)
		{
			pinf[i] = float(pin16[i]) / 32768.f;
		}
	}

	return pinf;
}

void WasapiDeviceCapture::SendAudioData(const float* pinf, size_t numFramesAvailable, size_t numCaptureChannels)
{
	auto adm = FWebRTCPeerConnection::GetAudioDeviceModule();

	if (kOpusSampleRate != format_->nSamplesPerSec)
	{
		ResamplingParameters.InputBuffer.Append(pinf, numFramesAvailable * numCaptureChannels);

		Audio::FResamplerResults results;
		Audio::AlignedFloatBuffer OutBuffer;

		results.OutBuffer = &OutBuffer;
		results.OutBuffer->AddUninitialized(numFramesAvailable * numCaptureChannels * kOpusSampleRate / format_->nSamplesPerSec);

		auto r = Audio::Resample(ResamplingParameters, results);

		if (results.OutputFramesGenerated > 0)
		{
			adm->SendAudioData(results.OutBuffer->GetData(), results.OutputFramesGenerated * numCaptureChannels, numCaptureChannels, kOpusSampleRate);
		}
	}
	else
	{
		adm->SendAudioData(pinf, numFramesAvailable * numCaptureChannels, numCaptureChannels, kOpusSampleRate);
	}
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void WasapiDeviceCapture::OnTick()
{
	size_t numCaptureChannels = format_->nChannels;
	UINT32 packetLength = 0;

	if (FAILED(capture_->GetNextPacketSize(&packetLength)))
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't get packet size!"));
		return;
	}

	while (packetLength > 0)
	{
		// Get the available data in the shared buffer.
		UINT32 numFramesAvailable;
		BYTE* pData;
		DWORD flags;

		if (FAILED(capture_->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr)))
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't get capture buffer!"));
			break;
		}

		float* pinf = ConvertToFloatSample(pData, numFramesAvailable, numCaptureChannels);

		SendAudioData(pinf, numFramesAvailable, numCaptureChannels);

		AudioBuffer.Empty();
		capture_->ReleaseBuffer(numFramesAvailable);
		capture_->GetNextPacketSize(&packetLength);
	}
}

bool WasapiDeviceCapture::ColdInit()
{
	return sWcore.ColdInit();
}

void WasapiDeviceCapture::ColdExit()
{
	sWcore.ColdExit();
}

#endif
