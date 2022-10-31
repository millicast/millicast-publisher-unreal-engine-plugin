#pragma once

#include <CoreMinimal.h>

UENUM(BlueprintType)
enum EMillicastVideoCodecs
{
	Vp8  UMETA(DisplayName = "VP8"),
	Vp9  UMETA(DisplayName = "VP9"),
	H264 UMETA(DisplayName = "H264"),
	Av1  UMETA(DisplayName = "AV1")
};

UENUM(BlueprintType)
enum EMillicastAudioCodecs
{
	Opus      UMETA(DisplayName = "opus"),
	Multiopus UMETA(DisplayName = "multiopus")
};