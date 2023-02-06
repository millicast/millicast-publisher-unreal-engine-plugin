#pragma once

#include <CoreMinimal.h>

UENUM()
enum class EMillicastVideoCodecs : uint8
{
	Vp8  UMETA(DisplayName = "VP8"),
	Vp9  UMETA(DisplayName = "VP9"),
	H264 UMETA(DisplayName = "H264"),
	Av1  UMETA(DisplayName = "AV1")
};

UENUM()
enum class EMillicastAudioCodecs : uint8
{
	Opus      UMETA(DisplayName = "opus"),
};