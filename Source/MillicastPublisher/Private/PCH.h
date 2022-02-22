// Copyright CoSMoSoftware, 2021. All Rights Reserved.

#pragma once

#include "HAL/Thread.h"
#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/CriticalSection.h"

#include "WebRTC/WebRTCInc.h"

// Engine

#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"

#include "Async/Async.h"

#include "Modules/ModuleManager.h"

#include "IWebSocket.h"
#include "WebSocketsModule.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"

#include "Templates/UniquePtr.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Templates/Atomic.h"

#include "AudioMixerDevice.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateUser.h"

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
#include "Windows/WindowsPlatformMisc.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformMisc.h"
#endif