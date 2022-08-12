
// Copyright Millicast 2022. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"
#include "Texture2DFrameBuffer.h"
#include "MillicastPublisherPrivate.h"

FTexture2DVideoSourceAdapter::FTexture2DVideoSourceAdapter() noexcept
	: AsyncTextureReadback(MakeShared<FAsyncTextureReadback>())
{

}

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY())) return;

	AsyncTextureReadback->ReadbackAsync_RenderThread(FrameBuffer, [this, Timestamp](uint8* B8G8R8A8Pixels, int Width, int Height, int Stride) {

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> Buffer = new rtc::RefCountedObject<FB8G8R8A8ToI420FrameBuffer>(B8G8R8A8Pixels, Width, Height, Stride);
		
		webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
			.set_video_frame_buffer(Buffer)
			.set_timestamp_us(Timestamp)
			.set_rotation(webrtc::VideoRotation::kVideoRotation_0)
			.build();

		rtc::AdaptedVideoTrackSource::OnFrame(Frame);

	});
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
