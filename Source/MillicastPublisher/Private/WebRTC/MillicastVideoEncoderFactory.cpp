// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastVideoEncoderFactory.h"
#include "absl/strings/match.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "media/base/h264_profile_level_id.h"
#include "VideoEncoderNVENC.h"
#include "VideoEncoderVPX.h"

webrtc::SdpVideoFormat CreateH264Format(webrtc::H264::Profile profile, webrtc::H264::Level level)
{
	const absl::optional<std::string> profile_string =
		webrtc::H264::ProfileLevelIdToString(webrtc::H264::ProfileLevelId(profile, level));
	check(profile_string);
	return webrtc::SdpVideoFormat(
		cricket::kH264CodecName,
		{ { cricket::kH264FmtpProfileLevelId, *profile_string },
			{ cricket::kH264FmtpLevelAsymmetryAllowed, "1" },
			{ cricket::kH264FmtpPacketizationMode, "1" } });
}

FMillicastVideoEncoderFactory::FMillicastVideoEncoderFactory()
{
}

FMillicastVideoEncoderFactory::~FMillicastVideoEncoderFactory()
{
}

std::vector<webrtc::SdpVideoFormat> FMillicastVideoEncoderFactory::GetSupportedFormats() const
{
	std::vector<webrtc::SdpVideoFormat> VideoFormats;
	VideoFormats.push_back(CreateH264Format(webrtc::H264::kProfileMain, webrtc::H264::kLevel1));
	VideoFormats.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
	VideoFormats.push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
	return VideoFormats;
}

FMillicastVideoEncoderFactory::CodecInfo FMillicastVideoEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
	CodecInfo codec_info;
	codec_info.has_internal_source = false;
	if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)
		|| absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
	{
		codec_info.is_hardware_accelerated = false;
	}
	if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
	{
		codec_info.is_hardware_accelerated = true;
	}

	return codec_info;
}

std::unique_ptr<webrtc::VideoEncoder> FMillicastVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
	if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
	{
		return std::make_unique<FVideoEncoderVPX>(8);
	}
	if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
	{
		return std::make_unique<FVideoEncoderVPX>(9);
	}
	if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName))
	{
		return std::make_unique<FVideoEncoderNVENC>();
	}
	return nullptr;
}
