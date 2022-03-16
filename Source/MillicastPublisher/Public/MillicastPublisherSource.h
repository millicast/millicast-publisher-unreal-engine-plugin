// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StreamMediaSource.h"
#include "IMillicastSource.h"

#include "MillicastPublisherSource.generated.h"


/**
 * Media source description for Millicast Publisher.
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

	/** Source id to use Millicast multisource feature */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AssetRegistrySearchable)
	FString SourceId;

	/** Whether we should capture video or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video, AssetRegistrySearchable)
	bool CaptureVideo = true;

	/** Publish video from this render target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video, AssetRegistrySearchable)
	UTextureRenderTarget2D* RenderTarget = nullptr;

	/** Whether we should capture game audio or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio, AssetRegistrySearchable)
	bool CaptureAudio = true;

	/** Mute the video stream */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "MuteVideo"))
	void MuteVideo(bool Muted);

	/** Set a new render target while publishing */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "ChangeRenderTarget"))
	void ChangeRenderTarget(UTextureRenderTarget2D * InRenderTarget);
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
	/** 
	* Create a capturer from the configuration set for video and audio and start the capture
	* You can set a callback to get the track returns by the capturer when starting the capture
	*/
	void StartCapture(TFunction<void(IMillicastSource::FStreamTrackInterface)> Callback = nullptr);

	/** Stop the capture and destroy all capturers */
	void StopCapture();

private:
	TUniquePtr<IMillicastVideoSource> VideoSource;
	TUniquePtr<IMillicastAudioSource> AudioSource;
};
