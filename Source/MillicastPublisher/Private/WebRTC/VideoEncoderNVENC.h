// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "VideoEncoder.h"
#include "CodecPacket.h"

class FVideoEncoderNVENC : public webrtc::VideoEncoder
{
public:
	FVideoEncoderNVENC();
	virtual ~FVideoEncoderNVENC() override;

	virtual int32 RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;
	virtual int32 Release() override;

	// WebRTC Interface
	virtual int InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings) override;
	virtual int32 Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types) override;
	virtual void SetRates(RateControlParameters const& parameters) override;
	virtual webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

	AVEncoder::FVideoEncoder::FLayerConfig GetConfig() const { return EncoderConfig; }

private:
	void UpdateConfig(AVEncoder::FVideoEncoder::FLayerConfig const& Config);
	void HandlePendingRateChange();
	void CreateAVEncoder(TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput);

	// "this" cannot be a shared_ptr because webrtc wants a unique_ptr
	// we use this shared context to make sure we dont try to call a cleared callback.
	struct FSharedContext
	{
		webrtc::EncodedImageCallback* OnEncodedImageCallback = nullptr;
		FCriticalSection* ParentSection = nullptr;
	};
	TSharedPtr<FSharedContext> SharedContext;
	FCriticalSection ContextSection; // used to prevent clearing of the callback while we're using it

	TSharedPtr<AVEncoder::FVideoEncoder> NVENCEncoder;
	AVEncoder::FVideoEncoder::FLayerConfig EncoderConfig;
	TOptional<RateControlParameters> PendingRateChange;
};
