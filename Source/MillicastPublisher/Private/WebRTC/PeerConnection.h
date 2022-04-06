// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"
#include "WebRTC/AudioDeviceModule.h"

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

	rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnection;

	static TUniquePtr<rtc::Thread>                       SignalingThread;
	static rtc::scoped_refptr<FAudioDeviceModule> AudioDeviceModule;
	static std::unique_ptr<webrtc::TaskQueueFactory>     TaskQueueFactory;

	using FCreateSessionDescriptionObserver = TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver>;
	using FSetSessionDescriptionObserver = TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver>;

	TUniquePtr<FCreateSessionDescriptionObserver> CreateSessionDescription;
	TUniquePtr<FSetSessionDescriptionObserver>    LocalSessionDescription;
	TUniquePtr<FSetSessionDescriptionObserver>    RemoteSessionDescription;

	template<typename Callback>
	webrtc::SessionDescriptionInterface* CreateDescription(const std::string&,
														   const std::string&,
														   Callback&&);

	static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory;
	static void CreatePeerConnectionFactory();
  
public:
	using FRTCConfig = webrtc::PeerConnectionInterface::RTCConfiguration;

	/** Offer/Answer options (e.g. offer to receive audio/video) */
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions OaOptions;

	FWebRTCPeerConnection() = default;

	/** Get WebRTC Peerconnection configuration */
	static FRTCConfig GetDefaultConfig();
	/** Create an instance of FWebRTCPeerConnection */
	static FWebRTCPeerConnection* Create(const FRTCConfig& config);
	/** Get the peerconnection factory pointer.*/
	static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> GetPeerConnectionFactory();
	/** Get the audio device module */
	static rtc::scoped_refptr<FAudioDeviceModule> GetAudioDeviceModule();

	/** Get local description observer to set callback for set local description success or failure */
	FSetSessionDescriptionObserver* GetLocalDescriptionObserver();
	/** Get remote description observer to set callback for set remote description success or failure */
	FSetSessionDescriptionObserver* GetRemoteDescriptionObserver();
	/** Get create description observer to set callback for set createOffer success or failure */
	FCreateSessionDescriptionObserver* GetCreateDescriptionObserver();

	const FSetSessionDescriptionObserver* GetLocalDescriptionObserver()     const;
	const FSetSessionDescriptionObserver* GetRemoteDescriptionObserver()    const;
	const FCreateSessionDescriptionObserver* GetCreateDescriptionObserver() const;

	/** Create offer and generates SDP */
	void CreateOffer();
	/** Set local SDP */
	void SetLocalDescription(const std::string& Sdp, const std::string& Type);
	/** Set remote SDP */
	void SetRemoteDescription(const std::string& Sdp, const std::string& Type=std::string("answer"));

	/* PeerConnection Observer interface */
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

	/* Return the webrtc peerconnection underlying pointer */
	webrtc::PeerConnectionInterface* operator->()
	{
		return PeerConnection.get();
	}
};
