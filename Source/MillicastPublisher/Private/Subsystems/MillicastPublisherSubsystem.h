#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "MillicastPublisherSubsystem.generated.h"

UCLASS()
class UMillicastPublisherSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
};
