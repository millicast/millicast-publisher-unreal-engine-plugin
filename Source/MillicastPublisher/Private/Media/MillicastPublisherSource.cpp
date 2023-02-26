// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherSource.h"

#include "AudioDeviceCapturer.h"
#include "AudioSubmixCapturer.h"

#include "MillicastPublisherPrivate.h"
#include "RenderTargetCapturer.h"

#include "Engine/Canvas.h"
#include "Subsystems/MillicastAudioDeviceCaptureSubsystem.h"
#include "WebRTC/PeerConnection.h"


#include <RenderTargetPool.h>

UMillicastPublisherSource::UMillicastPublisherSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Add default StreamUrl
	StreamUrl = "https://director.millicast.com/api/director/publish";
}

void UMillicastPublisherSource::BeginDestroy()
{
	UE_LOG(LogMillicastPublisher, Display, TEXT("Destroy MillicastPublisher Source"));

	// Stop the capture before destroying the object
	StopCapture();

	// Call parent Destroyer
	Super::BeginDestroy();
}

void UMillicastPublisherSource::Initialize(const FString& InPublishingToken, const FString& InStreamName, const FString& InSourceId,const FString& InStreamUrl)
{
	PublishingToken = InPublishingToken;
	SourceId = InSourceId;
	StreamName = InStreamName;
	StreamUrl = InStreamUrl;
}

/*
 * IMediaOptions interface
 */

void UMillicastPublisherSource::MuteVideo(bool Muted)
{
	if (!VideoSource)
	{
		return;
	}

	if (Muted)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Mute video"));
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Unmute video"));
	}

	auto Track = VideoSource->GetTrack();
	Track->set_enabled(!Muted);
}

void UMillicastPublisherSource::MuteAudio(bool Muted)
{
	if (!AudioSource)
	{
		return;
	}

	auto Track = AudioSource->GetTrack();
	Track->set_enabled(!Muted);
}

void UMillicastPublisherSource::SetAudioDeviceById(const FString& Id)
{
	if (!CaptureAudio || AudioCaptureType != AudioCapturerType::DEVICE)
	{
		UE_LOG(LogMillicastPublisher, Warning, 
			TEXT("You must enable the audio capture and set the Device audio capturer type first"));
		return;
	}

	if (AudioSource)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("You can't change the capture device while capturing"));
		return;
	}

	// Set CaptureDeviceIndex
	{
		auto* Subsystem = GEngine->GetEngineSubsystem<UMillicastAudioDeviceCaptureSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceById] UMillicastAudioDeviceCaptureSubsystem not found"));
			return;
		}

		const auto Index = Subsystem->Devices.IndexOfByPredicate([&Id](const auto& e) { return Id == e.DeviceId; });
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceById] Device not found: %s"), *Id);
			return;
		}

		CaptureDeviceIndex = Index;
	}
}

void UMillicastPublisherSource::SetAudioDeviceByName(const FString& Name)
{
	if (!CaptureAudio || AudioCaptureType != AudioCapturerType::DEVICE)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("You must enable the audio capture and set the Device audio capturer type first"));
		return;
	}

	if (AudioSource)
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("You can't change the capture device while capturing"));
		return;
	}

	// Set CaptureDeviceIndex
	{
		auto* Subsystem = GEngine->GetEngineSubsystem<UMillicastAudioDeviceCaptureSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceByName] UMillicastAudioDeviceCaptureSubsystem not found"));
			return;
		}

		const auto Index = Subsystem->Devices.IndexOfByPredicate([&Name](const auto& e) { return Name == e.DeviceName; });
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("[UMillicastPublisherSource::SetAudioDeviceByName] Device not found: %s"), *Name);
			return;
		}

		CaptureDeviceIndex = Index;
	}
}

void UMillicastPublisherSource::SetVolumeMultiplier(float f) 
{
	VolumeMultiplier = f;
	if (AudioSource) 
	{
		auto* src = static_cast<Millicast::Publisher::AudioDeviceCapturer*>(AudioSource.Get());
		src->SetVolumeMultiplier(f);
	}
}

FString UMillicastPublisherSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == MillicastPublisherOption::StreamName)
	{
		return StreamName;
	}
	
	if (Key == MillicastPublisherOption::PublishingToken)
	{
		return PublishingToken;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool UMillicastPublisherSource::HasMediaOption(const FName& Key) const
{
	if (Key == MillicastPublisherOption::StreamName || Key == MillicastPublisherOption::PublishingToken)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

/*
 * UMediaSource interface
 */

FString UMillicastPublisherSource::GetUrl() const
{
	return StreamUrl;
}

bool UMillicastPublisherSource::Validate() const
{
	// Check if stream name and publishing token are not empty
	return !StreamName.IsEmpty() && !PublishingToken.IsEmpty();
}

void UMillicastPublisherSource::StartCapture(TFunction<void(IMillicastSource::FStreamTrackInterface)> Callback)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Start capture"));

	// If video is enabled, create video capturer
	if (CaptureVideo)
	{
		// If a render target has been set, create a Render Target capturer
		if (RenderTarget != nullptr)
		{
			VideoSource = TSharedPtr<IMillicastVideoSource>(IMillicastVideoSource::Create(RenderTarget));
		}
		else
		{
			VideoSource = IMillicastVideoSource::Create();
		}

		//
		if (VideoSource && RenderTarget)
		{
			auto* RenderTargetVideoSource = static_cast<Millicast::Publisher::RenderTargetCapturer*>(VideoSource.Get());
			RenderTargetVideoSource->OnFrameRendered.AddLambda([this]()
			{
				if (LayeredTextures.IsEmpty() && !bSupportCustomDrawCanvas)
				{
					return;
				}

				TryInitRenderTargetCanvas();
				
				if (!RenderTargetCanvas)
				{
					return;
				}

				OnFrameRendered.Broadcast(RenderTargetCanvas);

				// Render Layered Textures
				for (const auto& Texture : LayeredTextures)
				{
					// TODO [RW] Engine API mistake. Function param should be const UTexture*
					const FVector2D CoordinatePosition;
					RenderTargetCanvas->K2_DrawTexture(const_cast<UTexture*>(Texture.Texture), Texture.Position, Texture.Size, CoordinatePosition);
				}

				/*FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

				for (auto& LTexture : LayeredTexture)
				{
					if (LTexture.Texture)
					{
						auto DestTexture = RenderTarget->GetResource()->GetTexture2DRHI();
						auto SourceTexture = LTexture.Texture->GetResource()->GetTexture2DRHI();
						CopyTexture(RHICmdList, SourceTexture, DestTexture, true, LTexture.Position);
					}
				}*/
			});
		}
		
		// Starts the capture and notify observers
		if (VideoSource && Callback)
		{
			Callback(VideoSource->StartCapture());
		}
	}

	// If audio is enabled, create audio capturer
	if (CaptureAudio)
	{
		using namespace Millicast::Publisher;

		AudioSource = TSharedPtr<IMillicastAudioSource>(IMillicastAudioSource::Create(AudioCaptureType));

		if (AudioCaptureType == AudioCapturerType::DEVICE)
		{
			auto source = static_cast<AudioDeviceCapturer*>(AudioSource.Get());
			source->SetAudioCaptureDevice(CaptureDeviceIndex);
			source->SetVolumeMultiplier(VolumeMultiplier);
		}
		else if (AudioCaptureType == AudioCapturerType::SUBMIX)
		{
			auto source = static_cast<AudioSubmixCapturer*>(AudioSource.Get());
			source->SetAudioSubmix(Submix);
		}

		// Start the capture and notify observers
		if (AudioSource && Callback)
		{
			Callback(AudioSource->StartCapture());
		}
	}
}

void UMillicastPublisherSource::StopCapture()
{
	UE_LOG(LogMillicastPublisher, Display, TEXT("Stop capture"));

	// Stop video capturer
	if (VideoSource) 
	{
		VideoSource->StopCapture();
		VideoSource = nullptr;
	}

	// Stop audio capturer
	if (AudioSource)
	{
		AudioSource->StopCapture();
		AudioSource = nullptr;
	}
}


void UMillicastPublisherSource::RegisterWorldContext(UObject* WorldContextObject)
{
	WorldContext = WorldContextObject;
}

void UMillicastPublisherSource::UnregisterWorldContext()
{
	WorldContext = nullptr;

	// Called at Endplay, so we also stop the canvas here
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(WorldContext, RenderTargetCanvasCtx);

	bRenderTargetInitialized = false;
	RenderTargetCanvas = nullptr;
	RenderTargetCanvasCtx = {};
}

void UMillicastPublisherSource::TryInitRenderTargetCanvas()
{
	if (bRenderTargetInitialized)
	{
		return;
	}

	if (LayeredTextures.IsEmpty() && !bSupportCustomDrawCanvas)
	{
		return;
	}

	if (!WorldContext)
	{
		return;
	}

	// NOTE [RW] This means we currently only support 1 world with this approach.
	// A more flexible API would need to be developed later

	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			bRenderTargetInitialized = false;
			return;
		}

		FVector2D DummySize;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RenderTarget, RenderTargetCanvas, DummySize, RenderTargetCanvasCtx);
	});
}

void UMillicastPublisherSource::ChangeRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	// This is allowed only when a capture has been starts with the Render Target capturer
	if (InRenderTarget != nullptr && RenderTarget != nullptr && VideoSource != nullptr)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Changing render target"));
		RenderTarget = InRenderTarget;
		auto* src = static_cast<Millicast::Publisher::RenderTargetCapturer*>(VideoSource.Get());
		src->SwitchTarget(RenderTarget);
	}
}

#if WITH_EDITOR
bool UMillicastPublisherSource::CanEditChange(const FProperty* InProperty) const
{
	FString Name;
	InProperty->GetName(Name);

	// Can't change render target if Capture video is disabled
	if (Name == MillicastPublisherOption::RenderTarget.ToString())
	{
		return CaptureVideo;
	}

	if (Name == MillicastPublisherOption::CaptureDeviceIndex.ToString())
	{
		return CaptureAudio && AudioCaptureType == AudioCapturerType::DEVICE;
	}
	if (Name == MillicastPublisherOption::Submix.ToString())
	{
		return CaptureAudio && AudioCaptureType == AudioCapturerType::SUBMIX;
	}
	if (Name == MillicastPublisherOption::AudioCaptureType.ToString())
	{
		return CaptureAudio;
	}
	if (Name == MillicastPublisherOption::VolumeMultiplier.ToString())
	{
		return  CaptureAudio && AudioCaptureType == AudioCapturerType::DEVICE;
	}

	return Super::CanEditChange(InProperty);
}

void UMillicastPublisherSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}

#endif //WITH_EDITOR