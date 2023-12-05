// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"

#include "MillicastScreenCapturerComponent.generated.h"

class UTextureRenderTarget2D;

UENUM()
enum EMillicastScreenCapturerType
{
	App,
	Monitor
};

USTRUCT(BlueprintType)
struct MILLICASTPUBLISHER_API FMillicastScreenCapturerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString Name;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TEnumAsByte<EMillicastScreenCapturerType> Type = EMillicastScreenCapturerType::Monitor;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Id = 0;
};

UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Screen Recorder Component", BlueprintSpawnableComponent))
class MILLICASTPUBLISHER_API UMillicastScreenCapturerComponent : public UActorComponent,
	public webrtc::DesktopCapturer::Callback
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CaptureFrameRate = 30; // Capture every 30ms

public:
#if !PLATFORM_ANDROID && !PLATFORM_IOS
	void OnCaptureResult(webrtc::DesktopCapturer::Result result,
		std::unique_ptr<webrtc::DesktopFrame> frame) override;

	void InitializeComponent() override;
	void UninitializeComponent() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#else 
	void OnCaptureResult(webrtc::DesktopCapturer::Result result,
		std::unique_ptr<webrtc::DesktopFrame> frame) override {}
	void InitializeComponent() override {}
	void UninitializeComponent() override {}
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override {}
#endif
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "GetMillicastScreenCapturerInfo"))
	TArray<FMillicastScreenCapturerInfo> GetMillicastScreenCapturerInfo();

	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META  =(DisplayName = "ChangeMillicastScreenCapturer"))
	void ChangeMillicastScreenCapturer(FMillicastScreenCapturerInfo Info);

private:
	TUniquePtr<webrtc::DesktopCapturer> DesktopCapturer;
	FPooledRenderTargetDesc RenderTargetDescriptor;
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
	int32 Id;

	FCriticalSection DesktopCapturerCriticalSection;

	static constexpr ETextureCreateFlags TextureCreateFlags = TexCreate_SRGB | TexCreate_Dynamic;

	void CreateTexture(FTexture2DRHIRef& TargetRef, int32 Width, int32 Height);
};