// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

// Wrapper for VPX encoders just to wrap the RHI texture to I420 step in Encode
class FVideoEncoderVPX : public webrtc::VideoEncoder
{
public:
	FVideoEncoderVPX(int VPXVersion);
	virtual ~FVideoEncoderVPX() override;

	virtual void SetFecControllerOverride(webrtc::FecControllerOverride* fec_controller_override) override;
	virtual int InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings) override;
	virtual int32 RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;
	virtual int32 Release() override;
	virtual int32 Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types) override;
	virtual void SetRates(RateControlParameters const& parameters) override;
	virtual void OnPacketLossRateUpdate(float packet_loss_rate) override;
	virtual void OnRttUpdate(int64_t rtt_ms) override;
	virtual void OnLossNotification(const LossNotification& loss_notification) override;
	virtual EncoderInfo GetEncoderInfo() const override;

private:
	std::unique_ptr<webrtc::VideoEncoder> WebRTCEncoder;
};
