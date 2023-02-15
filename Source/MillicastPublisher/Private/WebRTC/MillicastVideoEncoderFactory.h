// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

namespace Millicast::Publisher
{
	class FMillicastVideoEncoderFactory : public webrtc::VideoEncoderFactory
	{
	public:
		// webrtc::VideoEncoderFactory Interface begin
		virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
		virtual CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override;
		virtual std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override;
		// webrtc::VideoEncoderFactory Interface end
	};
}
