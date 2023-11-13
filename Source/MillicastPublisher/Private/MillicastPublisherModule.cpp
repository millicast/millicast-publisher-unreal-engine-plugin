// Copyright Dolby.io 2023. All Rights Reserved.

#include "IMillicastPublisherModule.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"

DEFINE_LOG_CATEGORY(LogMillicastPublisher);

#define LOCTEXT_NAMESPACE "MillicastPublisherModule"

IMillicastPublisherModule& IMillicastPublisherModule::Get()
{
	static const FName ModuleName = "MillicastPublisher";
	return FModuleManager::LoadModuleChecked<IMillicastPublisherModule>(ModuleName);
}

/**
 * Implements the Millicast Publisher module.
 */
class FMillicastPublisherModule : public IMillicastPublisherModule
{
public:
	TSharedPtr<FSlateStyleSet> GetStyle() override
	{
		return StyleSet;
	}

	void StartupModule() override
	{
		CreateStyle();
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
