#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "MillicastPublisherSourceRegistrySubsystem.generated.h"

class UMillicastPublisherSource;

UCLASS()
class UMillicastPublisherSourceRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void Register(UMillicastPublisherSource* Instance);

protected:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

private:
	TArray<UMillicastPublisherSource*> Instances;
};
