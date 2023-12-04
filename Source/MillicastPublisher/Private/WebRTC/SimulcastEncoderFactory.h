// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

namespace Millicast::Publisher
{
	class FMillicastVideoEncoderFactory;

	class FSimulcastEncoderFactory : public webrtc::VideoEncoderFactory
	{
	public:
		FSimulcastEncoderFactory();
		std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
		CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& Format) const override;

		std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override;

		FMillicastVideoEncoderFactory* GetEncoderFactory(int StreamIndex);

	private:
		TArray<TUniquePtr<FMillicastVideoEncoderFactory>> EncoderFactories;
		TUniquePtr<FMillicastVideoEncoderFactory> VideoEncoderFactory;
	};
}
