// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StreamMediaSource.h"
#include "IMillicastVideoSource.h"

#include "MillicastPublisherSource.generated.h"


/**
 * Media source description for Millicast Player.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object),
       META = (DisplayName = "Millicast Publisher Source"))
class MILLICASTPUBLISHER_API UMillicastPublisherSource : public UStreamMediaSource
{
	GENERATED_BODY()
public:

	UMillicastPublisherSource();

	/** The Millicast Stream name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Stream, AssetRegistrySearchable)
	FString StreamName;

	/** Publishing token. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AssetRegistrySearchable)
	FString PublishingToken;

public:
	//~ IMediaOptions interface

	FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	bool HasMediaOption(const FName& Key) const override;

public:
	//~ UMediaSource interface

	FString GetUrl() const override;
	bool Validate() const override;

public:
	/**
	   Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	   to allow the object to begin an asynchronous cleanup process.
	 */
	void BeginDestroy() override;

public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

public:
	IMillicastVideoSource::FVideoTrackInterface StartCapture();
	void StopCapture();

private:
	IMillicastVideoSource * VideoSource;
};
