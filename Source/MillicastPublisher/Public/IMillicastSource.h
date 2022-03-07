// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "api/media_stream_interface.h"

class IMillicastSource
{
public:
	TOptional<FString> TrackId;

public:
	using FStreamTrackInterface = rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>;

	virtual FStreamTrackInterface StartCapture() = 0;
	virtual void StopCapture() = 0;
	virtual FStreamTrackInterface GetTrack() = 0;

	virtual ~IMillicastSource() = default;
};

class IMillicastVideoSource : public IMillicastSource
{	
public:
	using FVideoTrackInterface = rtc::scoped_refptr<webrtc::VideoTrackInterface>;

	// Create VideoSource with SlateWindow Capture
	static IMillicastVideoSource* Create();
	// Create VideoSource and capture from a RenderTarget
	static IMillicastVideoSource* Create(UTextureRenderTarget2D* RenderTarget);
};

class IMillicastAudioSource : public IMillicastSource
{
public:
	using FAudioTrackInterface = rtc::scoped_refptr<webrtc::AudioTrackInterface>;

	// Create audio source to capture audio from game
	static IMillicastAudioSource* Create();
};