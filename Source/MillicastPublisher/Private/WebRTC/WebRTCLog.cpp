// Copyright Millicast 2022. All Rights Reserved.

#include "WebRTCLog.h"
#include "MillicastPublisherPrivate.h"

/**
 * Receives logging from WebRTC internals, and writes it to a log file
 * and VS's Output window
 */
class FWebRtcLogRedirector : public rtc::LogSink
{
public:
	explicit FWebRtcLogRedirector(rtc::LoggingSeverity Verbosity)
	{
		UE_LOG(LogMillicastPublisher, Verbose, TEXT("Starting WebRTC logging"));
		rtc::LogMessage::AddLogToStream(this, Verbosity);
		// Disable WebRTC's internal calls to VS's OutputDebugString, because we are calling here,
		// so we can add timestamps. Do this after `rtc::LogMessage::AddLogToStream`
		rtc::LogMessage::LogToDebug(rtc::LS_NONE);
		rtc::LogMessage::SetLogToStderr(false);
	}

	virtual ~FWebRtcLogRedirector()
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Stopping WebRTC logging"));
		rtc::LogMessage::RemoveLogToStream(this);
	}

private:
	virtual void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity, const char* tag) override
	{
#if !NO_LOGGING
		static const ELogVerbosity::Type RtcToUnrealLogCategoryMap[] = {
			ELogVerbosity::VeryVerbose,
			ELogVerbosity::Verbose,
			ELogVerbosity::Log,
			ELogVerbosity::Warning,
			ELogVerbosity::Error
		};

		if (LogMillicastPublisher.IsSuppressed(RtcToUnrealLogCategoryMap[severity]))
		{
			return;
		}

		switch (severity)
		{
			case rtc::LS_VERBOSE:
				UE_LOG(LogMillicastPublisher, Verbose, TEXT("%s"), message.c_str());
				break;
			case rtc::LS_INFO:
				UE_LOG(LogMillicastPublisher, Log, TEXT("%s"), message.c_str());
				break;
			case rtc::LS_WARNING:
				UE_LOG(LogMillicastPublisher, Warning, TEXT("%s"), message.c_str());
				break;
			case rtc::LS_ERROR:
				UE_LOG(LogMillicastPublisher, Error, TEXT("%s"), message.c_str());
				break;
		}
#endif
	}

	void OnLogMessage(const std::string& message) override
	{
		UE_LOG(LogMillicastPublisher, Verbose, TEXT("%s"), *FString(message.c_str()));
	}
};

void RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity Verbosity)
{
#if !UE_BUILD_SHIPPING
	static FWebRtcLogRedirector Redirector{ Verbosity };
#endif
}
