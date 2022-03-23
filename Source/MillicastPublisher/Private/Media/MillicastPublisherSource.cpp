// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherSource.h"
#include "MillicastPublisherPrivate.h"
#include "RenderTargetCapturer.h"

#include <RenderTargetPool.h>

UMillicastPublisherSource::UMillicastPublisherSource() : VideoSource(nullptr), AudioSource(nullptr)
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

/*
 * IMediaOptions interface
 */

void UMillicastPublisherSource::MuteVideo(bool Muted)
{
	if (VideoSource)
	{
		if (Muted)
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Mute video"));
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Unmute video"));
		}

		auto track = VideoSource->GetTrack();
		track->set_enabled(!Muted);
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
	if (Key == MillicastPublisherOption::StreamName || 
		Key == MillicastPublisherOption::PublishingToken)
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

	return Super::CanEditChange(InProperty);
}

void UMillicastPublisherSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}

#endif //WITH_EDITOR

void UMillicastPublisherSource::StartCapture(TFunction<void(IMillicastSource::FStreamTrackInterface)> Callback)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Start capture"));
	// If video is enabled, create video capturer
	if (CaptureVideo)
	{
		// If a render target has been set, create a Render Target capturer
		if (RenderTarget != nullptr)
		{
			VideoSource = TUniquePtr<IMillicastVideoSource>(IMillicastVideoSource::Create(RenderTarget));
		}
		else
		{
			VideoSource = TUniquePtr<IMillicastVideoSource>(IMillicastVideoSource::Create());
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
		AudioSource = TUniquePtr<IMillicastAudioSource>(IMillicastAudioSource::Create());
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

void UMillicastPublisherSource::ChangeRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	// This is allowed only when a capture has been starts with the Render Target capturer
	if (InRenderTarget != nullptr && RenderTarget != nullptr && VideoSource != nullptr)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Changing render target"));
		RenderTarget = InRenderTarget;
		auto* src = static_cast<RenderTargetCapturer*>(VideoSource.Get());
		src->SwitchTarget(RenderTarget);
	}
}

