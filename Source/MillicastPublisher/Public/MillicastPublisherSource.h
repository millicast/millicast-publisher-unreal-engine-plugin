// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "AudioCaptureDeviceInterface.h"
#include "IMillicastSource.h"
#include "Media/MillicastRenderTargetCanvas.h"
#include "StreamMediaSource.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundSubmix.h"

#include "MillicastPublisherSource.generated.h"

USTRUCT(BlueprintType)
struct FMillicastLayeredTexture
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	const UTexture* Texture = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	FVector2D Size = FVector2D::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFrameRendered, UCanvas*, Canvas);

/**
 * Media source description for Millicast Publisher.
 */
UCLASS(BlueprintType, hideCategories=(Platforms,Object),
       META = (DisplayName = "Millicast Publisher Source"))
class MILLICASTPUBLISHER_API UMillicastPublisherSource : public UStreamMediaSource
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnFrameRendered OnFrameRendered;

	/* Can be set if no layered textures are provided to expose a canvas anyway */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	bool bSupportCustomDrawCanvas = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	TArray<FMillicastLayeredTexture> LayeredTextures;

public:
	UMillicastPublisherSource(const FObjectInitializer& ObjectInitializer);

	bool IsCapturing() const { return World != nullptr; }
	
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Initialize"))
	void Initialize(const FString& InPublishingToken, const FString& InStreamName, const FString& InSourceId, const FString& InStreamUrl = "https://director.millicast.com/api/director/publish");


	/** The Millicast Stream name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Stream)
	FString StreamName;

	/** Publishing token. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream)
	FString PublishingToken;

	/** Source id to use Millicast multisource feature */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream)
	FString SourceId;

	/** Whether we should capture video or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	bool CaptureVideo = true;

	/** Publish video from this render target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Video)
	UTextureRenderTarget2D* RenderTarget = nullptr;
	
	/** Whether we should capture game audio or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	bool CaptureAudio = true;

	/** Which audio capturer to use */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	EAudioCapturerType AudioCaptureType = EAudioCapturerType::Submix;
	
	/** Audio submix */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	USoundSubmix* Submix;

	/** Capture device index  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	int32 CaptureDeviceIndex; // UMETA(ArrayClamp = "CaptureDevicesName");

	/** Apply a volume multiplier for the recorded data in dB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	float VolumeMultiplier = 20.f;

public:
	/** Mute the video stream */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "MuteVideo"))
	void MuteVideo(bool Muted);

	/** Set a new render target while publishing */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "ChangeRenderTarget"))
	void ChangeRenderTarget(UTextureRenderTarget2D * InRenderTarget);
	
public:
	/** Mute the audio stream */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "MuteAudio"))
	void MuteAudio(bool Muted);

	/** Set the audio capture device by its id */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetAudioDeviceById"))
	void SetAudioDeviceById(const FString& Id);

	/** Set the audio capture device by its name */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetAudioDeviceByName"))
	void SetAudioDeviceByName(const FString& Name);

	/** Apply a volume multiplier for the recorded data in dB */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetVolumeMultiplier"))
	void SetVolumeMultiplier(float Multiplier);

public:
	// IMediaOptions interface
	FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	bool HasMediaOption(const FName& Key) const override;
	//~ IMediaOptions interface

public:
	// UMediaSource interface
	FString GetUrl() const override;
	bool Validate() const override;
	//~ UMediaSource interface

public:
	//~ UObject interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif //WITH_EDITOR
	//~ End UObject interface

public:
	/** 
	* Create a capturer from the configuration set for video and audio and start the capture
	* You can set a callback to get the track returns by the capturer when starting the capture
	*/
	void StartCapture(UWorld* InWorld, bool InSimulcast, TFunction<void(IMillicastSource::FStreamTrackInterface)> Callback = nullptr);

	/**
	* Stop the capture and destroy all capturers
	* @param bDestroyLayeredTexturesCanvas If true will destroy the canvas created for layered textures.
	*										If you plan on quickly switching between sources, it is advised to have it be false.
	*/
	void StopCapture(bool bDestroyLayeredTexturesCanvas = false);

private:
	void HandleFrameRendered();
	void HandleRenderTargetCanvasInitialized();
	void TryInitRenderTargetCanvas();

private:
	TSharedPtr<IMillicastVideoSource> VideoSource;
	TSharedPtr<IMillicastAudioSource> AudioSource;

	UPROPERTY()
	UMillicastRenderTargetCanvas* RenderTargetCanvas;

	UPROPERTY()
	UWorld* World = nullptr;

	bool Simulcast = false;
};
