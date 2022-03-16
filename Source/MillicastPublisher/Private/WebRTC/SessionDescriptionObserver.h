// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"

class FWebRTCPeerConnection;

namespace detail
{

template<typename T>
class TSessionDescriptionObserver : public rtc::RefCountedObject<T>
{
	friend class ::FWebRTCPeerConnection;
	TFunction<void(const std::string&)> OnFailureCallback;
public:
	TSessionDescriptionObserver() : OnFailureCallback(nullptr) {}

	void OnFailure(webrtc::RTCError Error) override {
		if(OnFailureCallback) OnFailureCallback(Error.message());
	}

	template<typename Callback>
	void SetOnFailureCallback(Callback&& c)
	{
		OnFailureCallback = std::forward<Callback>(c);
	}
};

}

template<typename T>
class TSessionDescriptionObserver;

template<>
class TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver> :
	public detail::TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver>
{
	friend class FWebRTCPeerConnection;
	// Type, Sdp
	TFunction<void(const std::string&, const std::string&)> OnSuccessCallback;

public:

	void OnSuccess(webrtc::SessionDescriptionInterface* Desc) override {
		std::string Sdp;
		Desc->ToString(&Sdp);

		if(OnSuccessCallback) OnSuccessCallback(Desc->type(), Sdp);
	}

	template<typename Callback>
	void SetOnSuccessCallback(Callback&& c)
	{
		OnSuccessCallback = std::forward<Callback>(c);
	}
};

template<typename T>
class TSessionDescriptionObserver;

template<>
class TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver> :
	public detail::TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver>
{
	friend class FWebRTCPeerConnection;
	TFunction<void()> OnSuccessCallback;

public:

	void OnSuccess() override {
		if(OnSuccessCallback) OnSuccessCallback();
	}

	template<typename Callback>
	void SetOnSuccessCallback(Callback&& c)
	{
		OnSuccessCallback = std::forward<Callback>(c);
	}
};

