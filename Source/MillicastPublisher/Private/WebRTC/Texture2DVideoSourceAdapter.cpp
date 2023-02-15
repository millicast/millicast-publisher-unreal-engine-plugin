// Copyright Millicast 2022. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"

#include "Stats.h"

#include "MillicastPublisherPrivate.h"
#include "FrameBufferRHI.h"
#include "RHI/CopyTexture.h"
//#include "Stats.h"

namespace Millicast::Publisher
{

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY()))
		return;

	if (!CaptureContext)
	{
		FIntPoint FBSize = FrameBuffer->GetSizeXY();
		CaptureContext = MakeUnique<FAVEncoderContext>(FBSize.X, FBSize.Y, true);
	}

	FAVEncoderContext::FCapturerInput CapturerInput = CaptureContext->ObtainCapturerInput();
	FVideoEncoderInputFrameType InputFrame = CapturerInput.InputFrame;
	FTexture2DRHIRef Texture = CapturerInput.Texture.GetValue();

	const int32 FrameId = InputFrame->GetFrameID();
	
	InputFrame->SetTimestampUs(Timestamp);
	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#else
	const FIntPoint FBSize = FrameBuffer->GetSizeXY();
	InputFrame->SetWidth(FBSize.X);
	InputFrame->SetHeight(FBSize.Y);
#endif

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	CopyTexture(RHICmdList, FrameBuffer, Texture);

	rtc::scoped_refptr<FFrameBufferRHI> Buffer = new rtc::RefCountedObject<FFrameBufferRHI>(Texture, InputFrame, CaptureContext->GetVideoEncoderInput());
	webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
								   .set_video_frame_buffer(Buffer)
								   .set_timestamp_us(Timestamp)
								   .set_rotation(webrtc::VideoRotation::kVideoRotation_0)
								   .build();

	rtc::AdaptedVideoTrackSource::OnFrame(Frame);
	InputFrame->Release();

	FPublisherStats::Get().FrameRendered();
}

webrtc::MediaSourceInterface::SourceState FTexture2DVideoSourceAdapter::state() const
{
	return webrtc::MediaSourceInterface::kLive;
}

bool FTexture2DVideoSourceAdapter::AdaptVideoFrame(int64 TimestampUs, FIntPoint Resolution)
{
	int out_width, out_height, crop_width, crop_height, crop_x, crop_y;
	return rtc::AdaptedVideoTrackSource::AdaptFrame(Resolution.X, Resolution.Y, TimestampUs,
		&out_width, &out_height, &crop_width, &crop_height, &crop_x, &crop_y);
}

}