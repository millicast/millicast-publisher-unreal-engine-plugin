// Copyright Dolby.io 2023. All Rights Reserved.
#if WITH_AVENCODER

#include "VideoEncoderNVENC.h"
#include "FrameBufferRHI.h"
#include "AVEncoderContext.h"
#include "RHI/CopyTexture.h"
#include "Stats.h"
#include "VideoEncoderFactory.h"

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
	// the buffer is modified by a black frame buffer by libwebrtc when the track is muted
	// So when get the video frame buffer it is no longer a FFrameBufferRHI which lead a segfault later
	// This condition is to re-create a FFrameBufferRHI with a black frame
	if (frame.video_frame_buffer()->GetI420())
	{
		return WEBRTC_VIDEO_CODEC_OK;
	}

	// Get the frame buffer out of the frame
	auto* VideoFrameBuffer = static_cast<FFrameBufferRHI*>(frame.video_frame_buffer().get());

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

	FVideoEncoderInputFrameType EncoderInputFrame = VideoFrameBuffer->GetFrame();
	EncoderInputFrame->SetTimestampRTP(frame.timestamp());
	
	// Encode the frame!
	NVENCEncoder->Encode(EncoderInputFrame, EncodeOptions);

	return WEBRTC_VIDEO_CODEC_OK;
}

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
void CreateH264FragmentHeader(const uint8* CodedData, size_t CodedDataSize, webrtc::RTPFragmentationHeader& Fragments)
{
	// count the number of nal units
	for (int pass = 0; pass < 2; ++pass)
	{
		size_t num_nal = 0;
		size_t offset = 0;
		while (offset < CodedDataSize)
		{
			// either a 0,0,1 or 0,0,0,1 sequence indicates a new 'nal'
			size_t nal_maker_length = 3;
			if (offset < (CodedDataSize - 3) && CodedData[offset] == 0 && CodedData[offset + 1] == 0 && CodedData[offset + 2] == 1)
			{
			}
			else if (offset < (CodedDataSize - 4) && CodedData[offset] == 0 && CodedData[offset + 1] == 0 && CodedData[offset + 2] == 0 && CodedData[offset + 3] == 1)
			{
				nal_maker_length = 4;
			}
			else
			{
				++offset;
				continue;
			}
			if (pass == 1)
			{
				Fragments.fragmentationOffset[num_nal] = offset + nal_maker_length;
				Fragments.fragmentationLength[num_nal] = 0;
				if (num_nal > 0)
				{
					Fragments.fragmentationLength[num_nal - 1] = offset - Fragments.fragmentationOffset[num_nal - 1];
				}
			}
			offset += nal_maker_length;
			++num_nal;
		}
		if (pass == 0)
		{
			Fragments.VerifyAndAllocateFragmentationHeader(num_nal);
		}
		else if (pass == 1 && num_nal > 0)
		{
			Fragments.fragmentationLength[num_nal - 1] = offset - Fragments.fragmentationOffset[num_nal - 1];
		}
	}
}
#endif

void OnEncodedPacket(uint32 InLayerIndex, const FVideoEncoderInputFrameType InFrame, const AVEncoder::FCodecPacket& InPacket, webrtc::EncodedImageCallback* OnEncodedImageCallback)
{
	webrtc::EncodedImage Image;

	Image.timing_.packetization_finish_ms = FTimespan::FromSeconds(FPlatformTime::Seconds()).GetTotalMilliseconds();
	Image.timing_.encode_start_ms = InPacket.Timings.StartTs.GetTotalMilliseconds();
	Image.timing_.encode_finish_ms = InPacket.Timings.FinishTs.GetTotalMilliseconds();
	Image.timing_.flags = webrtc::VideoSendTiming::kTriggeredByTimer;

#if ENGINE_MAJOR_VERSION < 5
	Image.SetEncodedData(webrtc::EncodedImageBuffer::Create(const_cast<uint8_t*>(InPacket.Data), InPacket.DataSize));
#else
	Image.SetEncodedData(webrtc::EncodedImageBuffer::Create(const_cast<uint8_t*>(InPacket.Data.Get()), InPacket.DataSize));
#endif
	Image._encodedWidth = InFrame->GetWidth();
	Image._encodedHeight = InFrame->GetHeight();
	Image._frameType = InPacket.IsKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
	Image.content_type_ = webrtc::VideoContentType::UNSPECIFIED;
	Image.qp_ = InPacket.VideoQP;
	Image.SetSpatialIndex(InLayerIndex);
	Image.rotation_ = webrtc::VideoRotation::kVideoRotation_0;
	Image.SetTimestamp(InFrame->GetTimestampRTP());
	Image.capture_time_ms_ = InFrame->GetTimestampUs() / 1000.0;

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	Image._completeFrame = true;
#endif

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
	#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		webrtc::RTPFragmentationHeader FragHeader;

		#if ENGINE_MAJOR_VERSION < 5
		CreateH264FragmentHeader(InPacket.Data, InPacket.DataSize, FragHeader);
		#else
		CreateH264FragmentHeader(InPacket.Data.Get(), InPacket.DataSize, FragHeader);
		#endif

		OnEncodedImageCallback->OnEncodedImage(Image, &CodecInfo, &FragHeader);
	#else
		OnEncodedImageCallback->OnEncodedImage(Image, &CodecInfo);
	#endif
	}
}

void FVideoEncoderNVENC::CreateAVEncoder(TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput)
{
	const TArray<AVEncoder::FVideoEncoderInfo>& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();
	checkf(Available.Num() > 0, TEXT("No AVEncoders available. Check that the Hardware Encoders plugin is loaded."));

	if (Available.Num() == 0)
	{
		return;
	}

	TUniquePtr<AVEncoder::FVideoEncoder> EncoderTemp = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, EncoderInput, EncoderConfig);
	NVENCEncoder = TSharedPtr<AVEncoder::FVideoEncoder>(EncoderTemp.Release());
	checkf(NVENCEncoder, TEXT("Video encoder creation failed, check encoder config."));

	TWeakPtr<FVideoEncoderNVENC::FSharedContext> WeakContext = SharedContext;
	NVENCEncoder->SetOnEncodedPacket([WeakContext](uint32 InLayerIndex, const FVideoEncoderInputFrameType InputFrame, const AVEncoder::FCodecPacket& InPacket)
		{
			if (TSharedPtr<FVideoEncoderNVENC::FSharedContext> Context = WeakContext.Pin())
			{
				FScopeLock Lock(Context->ParentSection);
				OnEncodedPacket(InLayerIndex, InputFrame, InPacket, Context->OnEncodedImageCallback);
			}
		});
}

}
#endif
