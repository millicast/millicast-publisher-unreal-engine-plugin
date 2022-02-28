// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherSource.h"
#include "MillicastPublisherPrivate.h"
#include "WebRTC/RenderTargetCapturer.h"

#include <RenderTargetPool.h>

UMillicastPublisherSource::UMillicastPublisherSource()
{}

void UMillicastPublisherSource::BeginDestroy()
{
	StopCapture();
	Super::BeginDestroy();
}

/*
 * IMediaOptions interface
 */

void UMillicastPublisherSource::MuteVideo(bool Muted)
{
	if (VideoSource)
	{
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
	return !StreamName.IsEmpty() && !PublishingToken.IsEmpty();
}

#if WITH_EDITOR
bool UMillicastPublisherSource::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

void UMillicastPublisherSource::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);
}

#endif //WITH_EDITOR

IMillicastVideoSource::FVideoTrackInterface UMillicastPublisherSource::StartCapture()
{
	if (RenderTarget != nullptr)
	{
		VideoSource = TUniquePtr<IMillicastVideoSource>(IMillicastVideoSource::Create(RenderTarget));
	}
	else
	{
		VideoSource = TUniquePtr<IMillicastVideoSource>(IMillicastVideoSource::Create());
	}

	return VideoSource->StartCapture();
}

void UMillicastPublisherSource::StopCapture()
{
	if (VideoSource) 
	{
		VideoSource->StopCapture();
		VideoSource = nullptr;
	}
}

void UMillicastPublisherSource::ChangeRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	if (InRenderTarget != nullptr && RenderTarget != nullptr && VideoSource != nullptr)
	{
		RenderTarget = InRenderTarget;
		auto* src = static_cast<RenderTargetCapturer*>(VideoSource.Get());
		src->SwitchTarget(RenderTarget);
	}
}

