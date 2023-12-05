// Copyright Dolby.io 2023. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"

#include "Stats.h"

#include "FrameBufferRHI.h"
#include "RHI/CopyTexture.h"

namespace Millicast::Publisher
{

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY()))
		return;
#if !PLATFORM_ANDROID && !PLATFORM_IOS
	TryInitializeCaptureContexts(FrameBuffer);

	auto SimulcastBuffer = rtc::make_ref_counted<FSimulcastFrameBuffer>();

	TArray<FVideoEncoderInputFrameType> InputFrames;
	InputFrames.Reserve( CaptureContexts.Num() );
	
	for (const auto& Context : CaptureContexts)
	{
		const auto& CapturedInput = Context->ObtainCapturedInput();
		FVideoEncoderInputFrameType InputFrame = CapturedInput.InputFrame;
		InputFrames.Add(InputFrame);

		const FTexture2DRHIRef Texture = CapturedInput.Texture.GetValue();
		
		InputFrame->SetTimestampUs(Timestamp);
		
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
#else
		InputFrame->SetWidth(Context->GetCaptureWidth());
		InputFrame->SetHeight(Context->GetCaptureHeight());
#endif

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		CopyTexture(RHICmdList, FrameBuffer, Texture);

		const auto& Buffer = rtc::make_ref_counted<FFrameBufferRHI>(Texture, InputFrame, Context->GetVideoEncoderInput());
		SimulcastBuffer->AddLayer(Buffer);
	}

	webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
								   .set_video_frame_buffer(SimulcastBuffer)
								   .set_timestamp_us(Timestamp)
								   .set_rotation(webrtc::VideoRotation::kVideoRotation_0)
								   .build();

	rtc::AdaptedVideoTrackSource::OnFrame(Frame);

	// Release the input frames that we obtained
	for( auto& InputFrame : InputFrames )
	{
		InputFrame->Release();
	}

	FPublisherStats::Get().FrameRendered();
#else
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(FrameBuffer->GetTexture2D()->GetSizeX(), FrameBuffer->GetTexture2D()->GetSizeY(), EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable, ERHIAccess::CopyDest, CreateInfo);
#else

	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("VideoCapturerBackBuffer"),
		FrameBuffer->GetTexture2D()->GetSizeX(), FrameBuffer->GetTexture2D()->GetSizeY(), EPixelFormat::PF_B8G8R8A8);
	CreateDesc.SetFlags(TexCreate_Shared | TexCreate_RenderTargetable);
	CreateDesc.SetInitialState(ERHIAccess::CopyDest);

	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture(CreateDesc);
#endif
	auto SimulcastBuffer = rtc::make_ref_counted<FSimulcastFrameBuffer>();
	auto InputFrame = MakeShared<AVEncoder::FVideoEncoderInputFrame>();

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	CopyTexture(RHICmdList, FrameBuffer, Texture);
	const auto& Buffer = rtc::make_ref_counted<FFrameBufferRHI>(Texture, InputFrame, nullptr);
	SimulcastBuffer->AddLayer(Buffer);
	webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
		.set_video_frame_buffer(SimulcastBuffer)
		.set_timestamp_us(Timestamp)
		.set_rotation(webrtc::VideoRotation::kVideoRotation_0)
		.build();
	FPublisherStats::Get().FrameRendered();
	rtc::AdaptedVideoTrackSource::OnFrame(Frame);
#endif
}
#if !PLATFORM_ANDROID && !PLATFORM_IOS

void FTexture2DVideoSourceAdapter::TryInitializeCaptureContexts(const FTexture2DRHIRef& FrameBuffer)
{
        if (!IsEmpty(CaptureContexts))
	{
		return;
	}
	
	const auto& FBSize = FrameBuffer->GetSizeXY();
	CaptureContexts.Add(MakeUnique<FAVEncoderContext>(FBSize.X, FBSize.Y, true));

	if (Simulcast)
	{
		CaptureContexts.Add(MakeUnique<FAVEncoderContext>(FBSize.X / 2, FBSize.Y / 2, true));
		CaptureContexts.Add(MakeUnique<FAVEncoderContext>(FBSize.X / 4, FBSize.Y / 4, true));
	}
}
#endif
bool FTexture2DVideoSourceAdapter::AdaptVideoFrame(int64 TimestampUs, FIntPoint Resolution)
{
	int out_width, out_height, crop_width, crop_height, crop_x, crop_y;
	return rtc::AdaptedVideoTrackSource::AdaptFrame(Resolution.X, Resolution.Y, TimestampUs,
		&out_width, &out_height, &crop_width, &crop_height, &crop_x, &crop_y);
}

}
