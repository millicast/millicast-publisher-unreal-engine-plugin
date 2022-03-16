// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FSlateStyleSet;
class IMediaEventSink;
class IMediaPlayer;

/**
 * Interface Millicast publisher module
 */
class IMillicastPublisherModule : public IModuleInterface
{
public:

	static inline IMillicastPublisherModule& Get()
	{
		static const FName ModuleName = "MillicastPublisher";
		return FModuleManager::LoadModuleChecked<IMillicastPublisherModule>(ModuleName);
	}

	/** @return SlateStyleSet to be used across the MillicastPublisher module */
	virtual TSharedPtr<FSlateStyleSet> GetStyle() = 0;
};

