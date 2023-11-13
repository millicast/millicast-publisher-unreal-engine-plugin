// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

namespace Millicast::Publisher
{
	void RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity Verbosity);
}
