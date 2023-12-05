// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

namespace Millicast::Publisher
{
	class FMillicastVideoEncoderFactory : public webrtc::VideoEncoderFactory
	{
	public:

		// webrtc::VideoEncoderFactory Interface begin
		std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
		CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const /*override*/;
		std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override;

		// Todo : ForceKeyFrame

		// webrtc::VideoEncoderFactory Interface end
	};
}
