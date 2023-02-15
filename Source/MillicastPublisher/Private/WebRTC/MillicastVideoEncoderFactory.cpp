// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastVideoEncoderFactory.h"
#include "absl/strings/match.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#include "media/base/h264_profile_level_id.h"
#else
#include "api/video_codecs/h264_profile_level_id.h"
#endif

#include "VideoEncoderNVENC.h"
#include "VideoEncoderVPX.h"

namespace Millicast::Publisher
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
#define H264NameSpace(x) webrtc::H264:: ## x
#else
#define H264NameSpace(x) webrtc::H264 ## x
#endif

webrtc::SdpVideoFormat CreateH264Format(H264NameSpace(Profile) profile, H264NameSpace(Level) level)
{
	const absl::optional<std::string> profile_string =
		H264NameSpace(ProfileLevelIdToString)( H264NameSpace(ProfileLevelId)(profile, level) );
	check(profile_string);
	
	return webrtc::SdpVideoFormat( cricket::kH264CodecName,
		{
			{ cricket::kH264FmtpProfileLevelId, *profile_string },
			{ cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
			{ cricket::kH264FmtpPacketizationMode, "1" }
		});
}

std::vector<webrtc::SdpVideoFormat> FMillicastVideoEncoderFactory::GetSupportedFormats() const
{
	std::vector<webrtc::SdpVideoFormat> VideoFormats;
	VideoFormats.push_back(CreateH264Format(H264NameSpace(Profile)::kProfileMain, H264NameSpace(Level)::kLevel1));
	VideoFormats.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
	VideoFormats.push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
	return VideoFormats;
}

FMillicastVideoEncoderFactory::CodecInfo FMillicastVideoEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
	CodecInfo codec_info;
	codec_info.has_internal_source = false;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
	if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)
		|| absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
	{
		codec_info.is_hardware_accelerated = false;
	}
	if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
	{
		codec_info.is_hardware_accelerated = true;
	}
#endif

	return codec_info;
}

std::unique_ptr<webrtc::VideoEncoder> FMillicastVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
	if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
	{
		return std::make_unique<Millicast::Publisher::FVideoEncoderVPX>(8);
	}

	if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
	{
		return std::make_unique<Millicast::Publisher::FVideoEncoderVPX>(9);
	}

	if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
	{
		return std::make_unique<Millicast::Publisher::FVideoEncoderNVENC>();
	}

	UE_LOG(LogMillicastPublisher, Warning, TEXT("CreateVideoEncoder called with unknown encoder: %s"), *FString(format.name.c_str()) );
	return nullptr;
}

}