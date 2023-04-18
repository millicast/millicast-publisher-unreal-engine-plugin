#include "SimulcastEncoderFactory.h"

#include "MillicastVideoEncoderFactory.h"
#include "SimulcastVideoEncoder.h"

namespace Millicast::Publisher
{

FSimulcastEncoderFactory::FSimulcastEncoderFactory() 
	: VideoEncoderFactory(MakeUnique<FMillicastVideoEncoderFactory>())
{
	EncoderFactories.SetNum(webrtc::kMaxSimulcastStreams);

	for (int i = 0; i < webrtc::kMaxSimulcastStreams; ++i)
	{
		EncoderFactories[i] = MakeUnique<FMillicastVideoEncoderFactory>();
	}
}

std::vector<webrtc::SdpVideoFormat> FSimulcastEncoderFactory::GetSupportedFormats() const
{
	return VideoEncoderFactory->GetSupportedFormats();
}

FSimulcastEncoderFactory::CodecInfo FSimulcastEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat& Format) const
{
	return VideoEncoderFactory->QueryVideoEncoder(Format);
}

std::unique_ptr<webrtc::VideoEncoder> FSimulcastEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
	return std::make_unique<FSimulcastVideoEncoder>(*this, format);
}

FMillicastVideoEncoderFactory* FSimulcastEncoderFactory::GetEncoderFactory(int StreamIndex)
{
	return EncoderFactories[StreamIndex].Get();
}

}