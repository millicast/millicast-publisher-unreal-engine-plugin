// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherSourceFactoryNew.h"

#include "AssetTypeCategories.h"
#include "MillicastPublisherSource.h"

#define LOCTEXT_NAMESPACE "MillicastPublisherEditorMediaSenderFactory"

/* UMillicastPublisherSourceFactoryNew structors
 *****************************************************************************/

UMillicastPublisherSourceFactoryNew::UMillicastPublisherSourceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMillicastPublisherSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

FText UMillicastPublisherSourceFactoryNew::GetDisplayName() const
{
	return LOCTEXT("MillicastPublisherSourceFactory", "Millicast Publisher Source");
}

UObject* UMillicastPublisherSourceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMillicastPublisherSource>(InParent, InClass, InName, Flags);
}


uint32 UMillicastPublisherSourceFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UMillicastPublisherSourceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
