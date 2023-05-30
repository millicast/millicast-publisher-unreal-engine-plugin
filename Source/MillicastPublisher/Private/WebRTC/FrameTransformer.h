// Copyright Dolby.io RealTime Streaming 2023. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"

namespace Millicast::Publisher
{

class FWebRTCPeerConnection;

class FFrameTransformer : public webrtc::FrameTransformerInterface
{
	std::unordered_map <uint64_t, rtc::scoped_refptr<webrtc::TransformedFrameCallback>> Callbacks; // ssrc, callback
	TArray<uint8> UserData, TransformedData;
	FWebRTCPeerConnection* PeerConnection{ nullptr };
	
public:
	using FTransformableFrame = std::unique_ptr<webrtc::TransformableFrameInterface>;

	explicit FFrameTransformer(FWebRTCPeerConnection* InPeerConnection) noexcept : PeerConnection(InPeerConnection) {}

	~FFrameTransformer() = default;

	/** webrtc::FrameTransformer overrides */
	void Transform(FTransformableFrame TransformableFrame) override;

	void RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback) override;
	void RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback, uint32_t Ssrc) override;
	void UnregisterTransformedFrameCallback() override;
	void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;
};

}