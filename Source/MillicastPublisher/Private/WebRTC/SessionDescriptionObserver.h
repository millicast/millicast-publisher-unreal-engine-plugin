// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTC/WebRTCInc.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFailure, const std::string&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuccess, const std::string&, const std::string&);
DECLARE_MULTICAST_DELEGATE(FOnSetSessionSuccess);

namespace Millicast::Publisher
{
	namespace Detail
	{
		template<typename T>
		class TSessionDescriptionObserver : public rtc::RefCountedObject<T>
		{
		public:
			FOnFailure OnFailureEvent;

			void OnFailure(webrtc::RTCError Error) override
			{
				if (!OnFailureEvent.IsBound())
				{
					return;
				}

				OnFailureEvent.Broadcast(Error.message());
			}
		};
	}

	template<typename T>
	class TSessionDescriptionObserver;

	template<>
	class TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver> :
		public Detail::TSessionDescriptionObserver<webrtc::CreateSessionDescriptionObserver>
	{
	public:
		FOnSuccess OnSuccessEvent;

		void OnSuccess(webrtc::SessionDescriptionInterface* Desc) override
		{
			std::string Sdp;
			Desc->ToString(&Sdp);

			OnSuccessEvent.Broadcast(Desc->type(), Sdp);
		}
	};

	template<>
	class TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver> :
		public Detail::TSessionDescriptionObserver<webrtc::SetSessionDescriptionObserver>
	{
	public:
		FOnSetSessionSuccess OnSuccessEvent;

		void OnSuccess() override {
			OnSuccessEvent.Broadcast();
		}
	};
}
