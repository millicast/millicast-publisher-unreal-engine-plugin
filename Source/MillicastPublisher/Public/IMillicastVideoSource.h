// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "api/media_stream_interface.h"
// #include "TextureRenderTarget2D.h"

class IMillicastVideoSource
{	
public:
	using FVideoTrackInterface = rtc::scoped_refptr<webrtc::VideoTrackInterface>;

	IMillicastVideoSource() noexcept = default;
	virtual ~IMillicastVideoSource() = default;

	virtual FVideoTrackInterface StartCapture() = 0;
	virtual void StopCapture() = 0;
	virtual FVideoTrackInterface GetTrack() = 0;

	// Create VideoSource with SlateWindow Capture
	static IMillicastVideoSource* Create();
	// Create VideoSource and capture from a RenderTarget
	// static IMillicastVideoSource* Create(UTextureRenderTarget2D* RenderTarget);
};