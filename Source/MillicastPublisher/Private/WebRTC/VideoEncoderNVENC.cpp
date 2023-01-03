// Copyright Millicast 2022. All Rights Reserved.

#include "VideoEncoderNVENC.h"
#include "FrameBufferRHI.h"
#include "VideoEncoderFactory.h"
#include "AVEncoderContext.h"
#include "RHI/CopyTexture.h"
#include "Stats.h"

namespace Millicast::Publisher
{

FVideoEncoderNVENC::FVideoEncoderNVENC()
{
	SharedContext = MakeShared<FVideoEncoderNVENC::FSharedContext>();
	SharedContext->ParentSection = &ContextSection;
}

FVideoEncoderNVENC::~FVideoEncoderNVENC()
{
	FScopeLock Lock(&ContextSection);
	SharedContext->OnEncodedImageCallback = nullptr;
	SharedContext->ParentSection = nullptr;
}

int32 FVideoEncoderNVENC::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
{
	FScopeLock Lock(&ContextSection);
	SharedContext->OnEncodedImageCallback = callback;
	return WEBRTC_VIDEO_CODEC_OK;
}

int32 FVideoEncoderNVENC::Release()
{
	FScopeLock Lock(&ContextSection);
	SharedContext->OnEncodedImageCallback = nullptr;
	return WEBRTC_VIDEO_CODEC_OK;
}

void FVideoEncoderNVENC::HandlePendingRateChange()
{
	if (PendingRateChange.IsSet())
	{
		const RateControlParameters& RateChangeParams = PendingRateChange.GetValue();

		EncoderConfig.MaxFramerate = RateChangeParams.framerate_fps;
		EncoderConfig.TargetBitrate = RateChangeParams.bitrate.get_sum_bps();
		EncoderConfig.MaxBitrate = RateChangeParams.bandwidth_allocation.bps();

		// Only the quality controlling peer should update the underlying encoder configuration with new bitrate/framerate.
		if (NVENCEncoder)
		{
			NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
		}

		// Clear the rate change request
		PendingRateChange.Reset();
	}
}

// Pass rate control parameters from WebRTC to our encoder
// This is how WebRTC can control the bitrate/framerate of the encoder.
void FVideoEncoderNVENC::SetRates(RateControlParameters const& parameters)
{
	PendingRateChange.Emplace(parameters);
}

webrtc::VideoEncoder::EncoderInfo FVideoEncoderNVENC::GetEncoderInfo() const
{
	VideoEncoder::EncoderInfo info;
	info.supports_native_handle = true;
	info.is_hardware_accelerated = true;
	info.has_internal_source = false;
	info.supports_simulcast = false;
	info.implementation_name = TCHAR_TO_UTF8(*FString::Printf(TEXT("MILLICAST_HW_ENCODER_%s"), GDynamicRHI->GetName()));

	// info.scaling_settings = VideoEncoder::ScalingSettings(LowQP, HighQP);

	// basically means HW encoder must be perfect and drop frames itself etc
	info.has_trusted_rate_controller = false;

	return info;
}

void FVideoEncoderNVENC::UpdateConfig(AVEncoder::FVideoEncoder::FLayerConfig const& InConfig)
{
	EncoderConfig = InConfig;

	// Only the quality controlling peer should update the underlying encoder configuration.
	if (NVENCEncoder)
	{
		NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
	}
}

int FVideoEncoderNVENC::InitEncode(webrtc::VideoCodec const* codec_settings, VideoEncoder::Settings const& settings)
{
	checkf(AVEncoder::FVideoEncoderFactory::Get().IsSetup(), TEXT("FVideoEncoderFactory not setup"));

	EncoderConfig.Width = codec_settings->width;
	EncoderConfig.Height = codec_settings->height;
	EncoderConfig.MaxBitrate = codec_settings->maxBitrate * 1000;
	EncoderConfig.TargetBitrate = codec_settings->startBitrate * 1000;
	EncoderConfig.MaxFramerate = codec_settings->maxFramerate;
	EncoderConfig.H264Profile = AVEncoder::FVideoEncoder::H264Profile::MAIN;
	EncoderConfig.RateControlMode = AVEncoder::FVideoEncoder::RateControlMode::VBR;

	return WEBRTC_VIDEO_CODEC_OK;
}

int32 FVideoEncoderNVENC::Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types)
{
	// Get the frame buffer out of the frame
	FFrameBufferRHI* VideoFrameBuffer = nullptr;

	// the buffer is modified by a black frame buffer by libwebrtc when the track is muted
	// So when get the video frame buffer it is no longer a FFrameBufferRHI which lead a segfault later
	// This condition is to re-create a FFrameBufferRHI with a black frame
	if (frame.video_frame_buffer()->GetI420() != nullptr)
	{
		return WEBRTC_VIDEO_CODEC_OK;
	}
	else
	{
		VideoFrameBuffer = static_cast<FFrameBufferRHI*>(frame.video_frame_buffer().get());
	}

	if (!NVENCEncoder)
	{
		CreateAVEncoder(VideoFrameBuffer->GetVideoEncoderInput());
		if (!NVENCEncoder)
		{
			return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
		}
	}

	// Change rates, if required.
	HandlePendingRateChange();

	// Detect video frame not matching encoder encoding resolution
	// Note: This can happen when UE application changes its resolution and the encoder is not programattically updated.
	const int FrameWidth = frame.width();
	const int FrameHeight = frame.height();
	if (EncoderConfig.Width != FrameWidth || EncoderConfig.Height != FrameHeight)
	{
		EncoderConfig.Width = FrameWidth;
		EncoderConfig.Height = FrameHeight;
		NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
	}

	AVEncoder::FVideoEncoder::FEncodeOptions EncodeOptions;
	if (frame_types && (*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey)
	{
		EncodeOptions.bForceKeyFrame = true;
	}

	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> EncoderInputFrame = VideoFrameBuffer->GetFrame();
	EncoderInputFrame->SetTimestampRTP(frame.timestamp());
	
	// Encode the frame!
	NVENCEncoder->Encode(EncoderInputFrame, EncodeOptions);

	return WEBRTC_VIDEO_CODEC_OK;
}

void OnEncodedPacket(uint32 InLayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InFrame, const AVEncoder::FCodecPacket& InPacket, webrtc::EncodedImageCallback* OnEncodedImageCallback)
{
	webrtc::EncodedImage Image;

	Image.timing_.packetization_finish_ms = FTimespan::FromSeconds(FPlatformTime::Seconds()).GetTotalMilliseconds();
	Image.timing_.encode_start_ms = InPacket.Timings.StartTs.GetTotalMilliseconds();
	Image.timing_.encode_finish_ms = InPacket.Timings.FinishTs.GetTotalMilliseconds();
	Image.timing_.flags = webrtc::VideoSendTiming::kTriggeredByTimer;

	Image.SetEncodedData(webrtc::EncodedImageBuffer::Create(const_cast<uint8_t*>(InPacket.Data.Get()), InPacket.DataSize));
	Image._encodedWidth = InFrame->GetWidth();
	Image._encodedHeight = InFrame->GetHeight();
	Image._frameType = InPacket.IsKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
	Image.content_type_ = webrtc::VideoContentType::UNSPECIFIED;
	Image.qp_ = InPacket.VideoQP;
	Image.SetSpatialIndex(InLayerIndex);
	Image.rotation_ = webrtc::VideoRotation::kVideoRotation_0;
	Image.SetTimestamp(InFrame->GetTimestampRTP());
	Image.capture_time_ms_ = InFrame->GetTimestampUs() / 1000.0;

	webrtc::CodecSpecificInfo CodecInfo;
	CodecInfo.codecType = webrtc::VideoCodecType::kVideoCodecH264;
	CodecInfo.codecSpecific.H264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
	CodecInfo.codecSpecific.H264.temporal_idx = webrtc::kNoTemporalIdx;
	CodecInfo.codecSpecific.H264.idr_frame = InPacket.IsKeyFrame;
	CodecInfo.codecSpecific.H264.base_layer_sync = false;

	const double EncoderLatencyMs = (InPacket.Timings.FinishTs.GetTotalMicroseconds() - InPacket.Timings.StartTs.GetTotalMicroseconds()) / 1000.0;
	const double BitrateMbps = InPacket.DataSize * 8 * InPacket.Framerate / 1000000.0;

	FPublisherStats::Get().SetEncoderStats(EncoderLatencyMs, BitrateMbps, InPacket.VideoQP);
	
	if (OnEncodedImageCallback)
	{
		OnEncodedImageCallback->OnEncodedImage(Image, &CodecInfo);
	}
}

void FVideoEncoderNVENC::CreateAVEncoder(TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput)
{
	const TArray<AVEncoder::FVideoEncoderInfo>& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();
	checkf(Available.Num() > 0, TEXT("No AVEncoders available. Check that the Hardware Encoders plugin is loaded."));

	if (!Available.IsEmpty())
	{
		TUniquePtr<AVEncoder::FVideoEncoder> EncoderTemp = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, EncoderInput, EncoderConfig);
		NVENCEncoder = TSharedPtr<AVEncoder::FVideoEncoder>(EncoderTemp.Release());
		checkf(NVENCEncoder, TEXT("Video encoder creation failed, check encoder config."));

		TWeakPtr<FVideoEncoderNVENC::FSharedContext> WeakContext = SharedContext;
		NVENCEncoder->SetOnEncodedPacket([WeakContext](uint32 InLayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InFrame, const AVEncoder::FCodecPacket& InPacket) {
			if (TSharedPtr<FVideoEncoderNVENC::FSharedContext> Context = WeakContext.Pin())
			{
				FScopeLock Lock(Context->ParentSection);
				OnEncodedPacket(InLayerIndex, InFrame, InPacket, Context->OnEncodedImageCallback);
			}
			});
	}
}

}