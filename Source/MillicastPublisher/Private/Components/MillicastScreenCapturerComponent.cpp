// Copyright Dolby.io 2023. All Rights Reserved.

#include "MillicastScreenCapturerComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderTargetPool.h"
#include "Util.h"

#include "MillicastPublisherPrivate.h"

UMillicastScreenCapturerComponent::UMillicastScreenCapturerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void UMillicastScreenCapturerComponent::OnCaptureResult(webrtc::DesktopCapturer::Result result, 
	std::unique_ptr<webrtc::DesktopFrame> frame)
{	
	ENQUEUE_RENDER_COMMAND(DrawDesktopFrame)
	([this, frame=MoveTemp(frame)](FRHICommandListImmediate& RHICmdList)
		{
			FIntPoint FrameSize = FIntPoint(frame->size().width(), frame->size().height());

			RenderTarget->ResizeTarget(FrameSize.X, FrameSize.Y);
		
			if (!RenderTargetDescriptor.IsValid() ||
				RenderTargetDescriptor.GetSize() != FIntVector(FrameSize.X, FrameSize.Y, 0))
			{
				RenderTargetDescriptor = FPooledRenderTargetDesc::Create2DDesc(FrameSize,
					PF_B8G8R8A8,
					FClearValueBinding::None,
					TextureCreateFlags,
					TexCreate_RenderTargetable,
					false);

				GRenderTargetPool.FindFreeElement(RHICmdList,
					RenderTargetDescriptor,
					PooledRenderTarget,
					TEXT("MILLICASTPUBLISHER"));
			}

			auto SourceTexture = RenderTarget->GetResource()->GetTexture2DRHI();

			// Create the update region structure
			FUpdateTextureRegion2D Region(0, 0, 0, 0, FrameSize.X, FrameSize.Y);

			// Set the Pixel data of the webrtc Frame to the SourceTexture
			RHIUpdateTexture2D(SourceTexture, 0, Region, frame->stride(), frame->data());
			RHIUpdateTextureReference(RenderTarget->TextureReference.TextureReferenceRHI, SourceTexture);
		});
}

void UMillicastScreenCapturerComponent::InitializeComponent()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("~UMillicastScreenCapturerComponent"));
	webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();

	FScopeLock Lock(&DesktopCapturerCriticalSection);

	DesktopCapturer = TUniquePtr<webrtc::DesktopCapturer>(
		webrtc::DesktopCapturer::CreateWindowCapturer(options).release());

	webrtc::DesktopCapturer::SourceList source_list;

	DesktopCapturer->GetSourceList(&source_list);
	DesktopCapturer->SelectSource(source_list[0].id);
	DesktopCapturer->Start(this);
}

void UMillicastScreenCapturerComponent::UninitializeComponent()
{
	DesktopCapturer = nullptr;
}

void UMillicastScreenCapturerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (RenderTarget)
	{
		FScopeLock Lock(&DesktopCapturerCriticalSection);
		DesktopCapturer->CaptureFrame();
	}
}

TArray<FMillicastScreenCapturerInfo> UMillicastScreenCapturerComponent::GetMillicastScreenCapturerInfo()
{
	TArray<FMillicastScreenCapturerInfo> Info;
	webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();

	webrtc::DesktopCapturer::SourceList SourceList;

	// First get the monitor
	auto MonitorCapturer = webrtc::DesktopCapturer::CreateScreenCapturer(options);
	MonitorCapturer->GetSourceList(&SourceList);

	for (auto& Source : SourceList)
	{
		FMillicastScreenCapturerInfo SourceInfo;
		SourceInfo.Type = EMillicastScreenCapturerType::Monitor;
		SourceInfo.Name = Millicast::Publisher::ToString(Source.title);
		SourceInfo.Id = Source.id;

		UE_LOG(LogMillicastPublisher, Log,
			TEXT("Addind monitor source %s id %d"), *SourceInfo.Name, SourceInfo.Id)

		Info.Add(MoveTemp(SourceInfo));
	}

	// then the app 
	SourceList.clear();
	auto AppCapturer = webrtc::DesktopCapturer::CreateWindowCapturer(options);
	AppCapturer->GetSourceList(&SourceList);

	for (auto& Source : SourceList)
	{
		FMillicastScreenCapturerInfo SourceInfo;
		SourceInfo.Type = EMillicastScreenCapturerType::App;
		SourceInfo.Name = Millicast::Publisher::ToString(Source.title);
		SourceInfo.Id = Source.id;

		UE_LOG(LogMillicastPublisher, Log,
			TEXT("Addind app source %s id %d"), *SourceInfo.Name, SourceInfo.Id)

		Info.Add(MoveTemp(SourceInfo));
	}

	return Info;
}

void UMillicastScreenCapturerComponent::ChangeMillicastScreenCapturer(FMillicastScreenCapturerInfo Info)
{
	webrtc::DesktopCaptureOptions options = webrtc::DesktopCaptureOptions::CreateDefault();

	std::unique_ptr<webrtc::DesktopCapturer> NewCapturer = nullptr;

	switch (Info.Type)
	{
	case EMillicastScreenCapturerType::Monitor:
		NewCapturer = webrtc::DesktopCapturer::CreateScreenCapturer(options);
		break;
	case EMillicastScreenCapturerType::App:
		NewCapturer = webrtc::DesktopCapturer::CreateWindowCapturer(options);
		break;
	default:
		NewCapturer = nullptr;
	}
	
	if (NewCapturer && NewCapturer->SelectSource(Info.Id))
	{
		FScopeLock Lock(&DesktopCapturerCriticalSection);
		DesktopCapturer = nullptr;
		DesktopCapturer = TUniquePtr<webrtc::DesktopCapturer>(NewCapturer.release());

		DesktopCapturer->Start(this);
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Error, 
			TEXT("Could not select screen capturer source %s id %d"), *Info.Name, Info.Id)
	}
}

void UMillicastScreenCapturerComponent::CreateTexture(FTexture2DRHIRef& TargetRef, int32 Width, int32 Height)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("MillicastTexture2d"), Width, Height, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TextureCreateFlags);
	TargetRef = RHICreateTexture(CreateDesc);
#else
	TRefCountPtr<FRHITexture2D> ShaderTexture2D;
	FRHIResourceCreateInfo CreateInfo = {
	#if ENGINE_MAJOR_VERSION >= 5
		TEXT("ResourceCreateInfo"),
	#endif
		FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f))
	};

	RHICreateTargetableShaderResource2D(Width, Height, EPixelFormat::PF_B8G8R8A8, 1,
		TextureCreateFlags, TexCreate_RenderTargetable, false, CreateInfo,
		TargetRef, ShaderTexture2D);
#endif
}
