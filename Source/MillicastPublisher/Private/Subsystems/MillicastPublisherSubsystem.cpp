#include "MillicastPublisherSubsystem.h"

#include "WebRTC/WebRTCInc.h"
#include "WebRTC/WebRTCLog.h"

#if PLATFORM_WINDOWS
#include "Media/WasapiDeviceCapturer.h"
#endif

void UMillicastPublisherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	rtc::InitializeSSL();

#if PLATFORM_WINDOWS
	Millicast::Publisher::WasapiDeviceCapturer::ColdInit();
#endif
	
	Millicast::Publisher::RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity::LS_VERBOSE);
}

void UMillicastPublisherSubsystem::Deinitialize()
{
	rtc::CleanupSSL();

#if PLATFORM_WINDOWS
	Millicast::Publisher::WasapiDeviceCapturer::ColdExit();
#endif
}
