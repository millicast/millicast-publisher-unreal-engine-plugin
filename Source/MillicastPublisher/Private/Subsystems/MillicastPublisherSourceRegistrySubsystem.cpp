// Copyright Dolby.io 2023. All Rights Reserved.

#include "MillicastPublisherSourceRegistrySubsystem.h"

#include "MillicastPublisherSource.h"

void UMillicastPublisherSourceRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
}

void UMillicastPublisherSourceRegistrySubsystem::Deinitialize()
{
	for( auto* Instance : Instances )
	{
		if( Instance->IsCapturing() )
		{
			Instance->StopCapture(true);
		}
	}
}

void UMillicastPublisherSourceRegistrySubsystem::Register(UMillicastPublisherSource* Instance)
{
	if( Instances.Contains(Instance) )
	{
		UE_LOG(LogTemp,Warning,TEXT("UMillicastPublisherSourceRegistrySubsystem::Register called twice for the same Instance"));
		return;
	}

	Instances.Add(Instance);
}
