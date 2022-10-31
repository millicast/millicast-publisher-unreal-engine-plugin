// Copyright Millicast 2022. All Rights Reserved.

#include "VideoEncoderVPX.h"
#include "FrameBufferRHI.h"

FVideoEncoderVPX::FVideoEncoderVPX(int VPXVersion)
{
	if (VPXVersion == 8)
	{
		WebRTCEncoder = webrtc::VP8Encoder::Create();
	}
	else if (VPXVersion == 9)
	{
		WebRTCEncoder = webrtc::VP9Encoder::Create();
	}
	else
	{
		checkf(false, TEXT("Bad VPX version number supplied to VideoEncoderVPX"));
	}
}

FVideoEncoderVPX::~FVideoEncoderVPX()
{
}

void FVideoEncoderVPX::SetFecControllerOverride(webrtc::FecControllerOverride* fec_controller_override)
{
	WebRTCEncoder->SetFecControllerOverride(fec_controller_override);
}

int FVideoEncoderVPX::InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings)
{
	return WebRTCEncoder->InitEncode(codec_settings, settings);
}

int32 FVideoEncoderVPX::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
{
	return WebRTCEncoder->RegisterEncodeCompleteCallback(callback);
}

int32 FVideoEncoderVPX::Release()
{
	return WebRTCEncoder->Release();
}

int32 FVideoEncoderVPX::Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types)
{
	WebRTCEncoder->Encode(frame, frame_types);
	return WEBRTC_VIDEO_CODEC_OK;
}

void FVideoEncoderVPX::SetRates(RateControlParameters const& parameters)
{
	WebRTCEncoder->SetRates(parameters);
}

void FVideoEncoderVPX::OnPacketLossRateUpdate(float packet_loss_rate)
{
	WebRTCEncoder->OnPacketLossRateUpdate(packet_loss_rate);
}

void FVideoEncoderVPX::OnRttUpdate(int64_t rtt_ms)
{
	WebRTCEncoder->OnRttUpdate(rtt_ms);
}

void FVideoEncoderVPX::OnLossNotification(const LossNotification& loss_notification)
{
	WebRTCEncoder->OnLossNotification(loss_notification);
}

webrtc::VideoEncoder::EncoderInfo FVideoEncoderVPX::GetEncoderInfo() const
{
	VideoEncoder::EncoderInfo info = WebRTCEncoder->GetEncoderInfo();
	info.supports_native_handle = true;
	return info;
}
