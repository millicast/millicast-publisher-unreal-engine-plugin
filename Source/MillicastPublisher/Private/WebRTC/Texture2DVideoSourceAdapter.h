// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "RHI.h"

/** Video Source adapter to create webrtc video frame from a Texture 2D and push it into webrtc pipelines */
class FTexture2DVideoSourceAdapter : public rtc::AdaptedVideoTrackSource
{
public:
	FTexture2DVideoSourceAdapter() noexcept = default;
	~FTexture2DVideoSourceAdapter() = default;

	void OnFrameReady(const FTexture2DRHIRef& FrameBuffer, bool ReadColor = false);

	webrtc::MediaSourceInterface::SourceState state() const override;
	absl::optional<bool> needs_denoising() const override { return false; }
	bool is_screencast() const override { return false; }
	bool remote() const override { return false; }

private:
	bool AdaptVideoFrame(int64 TimestampUs, FIntPoint Resolution);

	FCriticalSection CriticalSection;
};