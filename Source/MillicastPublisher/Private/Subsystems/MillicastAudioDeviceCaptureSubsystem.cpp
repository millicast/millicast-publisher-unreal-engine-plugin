#include "MillicastAudioDeviceCaptureSubsystem.h"

void UMillicastAudioDeviceCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Refresh();
}

void UMillicastAudioDeviceCaptureSubsystem::Deinitialize()
{
	
}

void UMillicastAudioDeviceCaptureSubsystem::Refresh()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Fetch available audio devices"));

	TArray<Audio::FCaptureDeviceInfo> RawDevices;
	Audio::FAudioCapture AudioCapture;
	AudioCapture.GetCaptureDevicesAvailable(RawDevices);

	Devices.Empty();
	for (const auto& Device : RawDevices)
	{
		Devices.Emplace(Device.DeviceName, Device.DeviceId);
	}
}
