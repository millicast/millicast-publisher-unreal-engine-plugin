#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "MillicastAudioDeviceCaptureSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FAudioCaptureInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	FString DeviceName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MillicastPublisher")
	FString DeviceId;

	FAudioCaptureInfo() noexcept = default;

	FAudioCaptureInfo(FString InDeviceName, FString InDeviceId) noexcept :
		DeviceName(MoveTemp(InDeviceName)), DeviceId(MoveTemp(InDeviceId)) {}
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
