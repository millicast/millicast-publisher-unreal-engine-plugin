// Copyright Millicast 2022. All Rights Reserved.

#include "IMillicastPublisherModule.h"

#include "MillicastPublisherPrivate.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Media/AudioGameCapturer.h"

DEFINE_LOG_CATEGORY(LogMillicastPublisher);

#define LOCTEXT_NAMESPACE "MillicastPublisherModule"

/**
 * Implements the Millicast Publisher module.
 */
class FMillicastPublisherModule : public IMillicastPublisherModule
{
public:

	virtual TSharedPtr<FSlateStyleSet> GetStyle() override
	{
		return StyleSet;
	}

public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
#if PLATFORM_WINDOWS
		WasapiDeviceCapture::ColdInit();
#endif
		CreateStyle();
	}

	virtual void ShutdownModule() override 
	{
#if PLATFORM_WINDOWS
		WasapiDeviceCapture::ColdExit();
#endif
	}

private:
	void CreateStyle()
	{
		static FName StyleName(TEXT("MillicastPublisherStyle"));
		StyleSet = MakeShared<FSlateStyleSet>(StyleName);

		const FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("MillicastPublisher"))->GetContentDir();
		const FVector2D Icon16x16(16.0f, 16.0f);
		StyleSet->Set("MillicastPublisherIcon", new FSlateImageBrush((ContentDir / TEXT("Editor/Icons/MillicastMediaSource_64x")) + TEXT(".png"), Icon16x16));
	}

private:
	TSharedPtr<FSlateStyleSet> StyleSet;
};

IMPLEMENT_MODULE(FMillicastPublisherModule, MillicastPublisher);

#undef LOCTEXT_NAMESPACE
