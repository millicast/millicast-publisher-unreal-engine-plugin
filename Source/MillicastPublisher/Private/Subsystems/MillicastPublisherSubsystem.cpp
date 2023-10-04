#include "MillicastPublisherSubsystem.h"

#include "WebRTC/WebRTCInc.h"
#include "WebRTC/WebRTCLog.h"

#include "MillicastAudioDeviceCaptureSubsystem.h"

#if PLATFORM_WINDOWS
#include "Media/WasapiDeviceCapturer.h"
#endif

void UMillicastPublisherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	rtc::InitializeSSL();
	
	Millicast::Publisher::RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity::LS_VERBOSE);

#if PLATFORM_WINDOWS
	Millicast::Publisher::WasapiDeviceCapturer::ColdInit();
#endif

	auto* Subsystem = GEngine->GetEngineSubsystem<UMillicastAudioDeviceCaptureSubsystem>();

	if (!Subsystem)
	{
		return;
	}

	Subsystem->Refresh();
}

void UMillicastPublisherSubsystem::Deinitialize()
{
#if PLATFORM_WINDOWS
	Millicast::Publisher::WasapiDeviceCapturer::ColdExit();
#endif	
	rtc::CleanupSSL();
}
