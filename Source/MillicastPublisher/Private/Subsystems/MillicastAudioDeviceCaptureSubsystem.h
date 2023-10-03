#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "MillicastAudioDeviceCaptureSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAudioCaptureDirection : uint8
{
	Input    UMETA(DisplayName = "Input"),
	Ouptut   UMETA(DisplayName = "Output"),
};

USTRUCT(BlueprintType)
struct FAudioCaptureInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	FString DeviceName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	FString DeviceId;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	EAudioCaptureDirection Direction;

	FAudioCaptureInfo() noexcept = default;

	FAudioCaptureInfo(FString InDeviceName, FString InDeviceId, EAudioCaptureDirection InDirection) noexcept 
		: DeviceName(MoveTemp(InDeviceName)), DeviceId(MoveTemp(InDeviceId)), Direction(MoveTemp(InDirection)) {}
};

UCLASS()
class UMillicastAudioDeviceCaptureSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	TArray<FAudioCaptureInfo> Devices;

public:
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher")
	void Refresh();

protected:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
};
