// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"

#include "SessionDescriptionObserver.h"

namespace webrtc {

class AudioDeviceModule;
class TaskQueueFactory;

}  // webrtc

/*
 * Small wrapper for the WebRTC peerconnection
 */
class FWebRTCPeerConnection : public webrtc::PeerConnectionObserver
{
	using FMediaStreamVector = std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>;
	using FRTCConfig = webrtc::PeerConnectionInterface::RTCConfiguration;

	rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnection;

	static TUniquePtr<rtc::Thread>                  SignalingThread;
	static rtc::scoped_refptr<webrtc::AudioDeviceModule> AudioDeviceModule;
	static std::unique_ptr<webrtc::TaskQueueFactory> TaskQueueFactory;

	using FCreateSessionDescriptionObserver = TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver>;
	using FSetSessionDescriptionObserver = TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver>;

	TUniquePtr<FCreateSessionDescriptionObserver> CreateSessionDescription;
	TUniquePtr<FSetSessionDescriptionObserver>    LocalSessionDescription;
	TUniquePtr<FSetSessionDescriptionObserver>    RemoteSessionDescription;

	rtc::VideoSinkInterface<webrtc::VideoFrame>* VideoSink;

	template<typename Callback>
	webrtc::SessionDescriptionInterface* CreateDescription(const std::string&,
														   const std::string&,
														   Callback&&);

	static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory;
	static void CreatePeerConnectionFactory();
  
public:

	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions OaOptions;

	FWebRTCPeerConnection() = default;

	static FRTCConfig GetDefaultConfig();
	static FWebRTCPeerConnection* Create(const FRTCConfig& config);
	static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> GetPeerConnectionFactory();

	FSetSessionDescriptionObserver* GetLocalDescriptionObserver();
	FSetSessionDescriptionObserver* GetRemoteDescriptionObserver();
	FCreateSessionDescriptionObserver* GetCreateDescriptionObserver();

	const FSetSessionDescriptionObserver* GetLocalDescriptionObserver()     const;
	const FSetSessionDescriptionObserver* GetRemoteDescriptionObserver()    const;
	const FCreateSessionDescriptionObserver* GetCreateDescriptionObserver() const;

	void CreateOffer();
	void SetLocalDescription(const std::string& Sdp, const std::string& Type);
	void SetRemoteDescription(const std::string& Sdp, const std::string& Type=std::string("answer"));

	// PeerConnection Observer interface
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
	  const FMediaStreamVector& streams) override;
	void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
	void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)  override;
	void OnRenegotiationNeeded() override;
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceConnectionReceivingChange(bool receiving) override;

	webrtc::PeerConnectionInterface* operator->()
	{
		return PeerConnection.get();
	}
};
