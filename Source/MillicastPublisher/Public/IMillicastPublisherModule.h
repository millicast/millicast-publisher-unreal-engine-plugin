// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class FSlateStyleSet;
class IMediaEventSink;
class IMediaPlayer;

/**
 * Interface Millicast publisher module
 */
class MILLICASTPUBLISHER_API IMillicastPublisherModule : public IModuleInterface
{
public:
	static IMillicastPublisherModule& Get();

	/** @return SlateStyleSet to be used across the MillicastPublisher module */
	virtual TSharedPtr<FSlateStyleSet> GetStyle() = 0;
};
