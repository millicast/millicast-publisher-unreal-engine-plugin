
// Copyright Millicast 2022. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"
#include "Texture2DFrameBuffer.h"

#include "MillicastPublisherPrivate.h"

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer, bool ReadColor)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY())) return;

	rtc::scoped_refptr<webrtc::VideoFrameBuffer> Buffer; 
	
	if (ReadColor)
	{
		Buffer = new rtc::RefCountedObject<FColorTexture2DFrameBuffer>(FrameBuffer);
	}
	else
	{
		Buffer = new rtc::RefCountedObject<FTexture2DFrameBuffer>(FrameBuffer);
	}

	webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
		.set_video_frame_buffer(Buffer)
		.set_timestamp_us(Timestamp)
		.set_rotation(webrtc::VideoRotation::kVideoRotation_0)
		.build();

	rtc::AdaptedVideoTrackSource::OnFrame(Frame);
}

webrtc::MediaSourceInterface::SourceState FTexture2DVideoSourceAdapter::state() const
{
	return webrtc::MediaSourceInterface::SourceState::kLive;
}

bool FTexture2DVideoSourceAdapter::AdaptVideoFrame(int64 TimestampUs, FIntPoint Resolution)
{
	int out_width, out_height, crop_width, crop_height, crop_x, crop_y;
	return rtc::AdaptedVideoTrackSource::AdaptFrame(Resolution.X, Resolution.Y, TimestampUs,
		 &out_width, &out_height, &crop_width, &crop_height, &crop_x, &crop_y);
}
