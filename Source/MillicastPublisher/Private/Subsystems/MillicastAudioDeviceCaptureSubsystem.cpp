// Copyright Dolby.io 2023. All Rights Reserved.

#include "MillicastAudioDeviceCaptureSubsystem.h"

#include <Containers/StringConv.h>

#if PLATFORM_WINDOWS
#include "Media/WasapiDeviceCapturer.h"
#endif

void UMillicastAudioDeviceCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
}

void UMillicastAudioDeviceCaptureSubsystem::Deinitialize()
{
}

void UMillicastAudioDeviceCaptureSubsystem::Refresh()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Fetch available audio devices"));

	// input 
	TArray<Audio::FCaptureDeviceInfo> RawDevices;
	Audio::FAudioCapture AudioCapture;
	AudioCapture.GetCaptureDevicesAvailable(RawDevices);

	Devices.Empty();
	for (const auto& Device : RawDevices)
	{
		Devices.Emplace(Device.DeviceName, Device.DeviceId, EAudioCaptureDirection::Input);
	}

	// output
#if PLATFORM_WINDOWS
	std::vector<Millicast::Publisher::DeviceDesc> RawOutputDevices;
	Millicast::Publisher::WasapiDeviceCapturer WasapiCapturer(10, true);

	WasapiCapturer.EnumerateDevice(RawOutputDevices);

	for (const auto& Device : RawOutputDevices)
	{
		Devices.Emplace(FString(Device.DeviceName.c_str()), 
			FString(Device.DeviceID.c_str()),
			EAudioCaptureDirection::Ouptut);
	}
#endif
}
