// Copyright Millicast 2022. All Rights Reserved.

#include "PeerConnection.h"
#include "MillicastPublisherPrivate.h"

#include <sstream>

#include "AudioDeviceModule.h"

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> FWebRTCPeerConnection::PeerConnectionFactory = nullptr;
TUniquePtr<rtc::Thread> FWebRTCPeerConnection::SignalingThread = nullptr;
rtc::scoped_refptr<FAudioDeviceModule> FWebRTCPeerConnection::AudioDeviceModule = nullptr;
std::unique_ptr<webrtc::TaskQueueFactory> FWebRTCPeerConnection::TaskQueueFactory = nullptr;

void FWebRTCPeerConnection::CreatePeerConnectionFactory()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Creating FWebRTCPeerConnectionFactory"));

	rtc::InitializeSSL();

	SignalingThread  = TUniquePtr<rtc::Thread>(rtc::Thread::Create().release());
	SignalingThread->SetName("WebRTCSignalingThread", nullptr);
	SignalingThread->Start();

	TaskQueueFactory = webrtc::CreateDefaultTaskQueueFactory();
	AudioDeviceModule = FAudioDeviceModule::Create(TaskQueueFactory.get());

	webrtc::AudioProcessing* AudioProcessingModule = webrtc::AudioProcessingBuilder().Create();
	webrtc::AudioProcessing::Config ApmConfig;

	ApmConfig.pipeline.multi_channel_capture = true;
	ApmConfig.pipeline.multi_channel_render = true;
	ApmConfig.pipeline.maximum_internal_processing_rate = 48000;
	ApmConfig.pre_amplifier.enabled = false;
	ApmConfig.high_pass_filter.enabled = false;
	ApmConfig.echo_canceller.enabled = false;
	ApmConfig.noise_suppression.enabled = false;
	ApmConfig.transient_suppression.enabled = false;
	ApmConfig.voice_detection.enabled = false;
	ApmConfig.gain_controller1.enabled = false;
	ApmConfig.gain_controller2.enabled = false;
	ApmConfig.residual_echo_detector.enabled = false;
	ApmConfig.level_estimation.enabled = false;

	// Apply the config.
	AudioProcessingModule->ApplyConfig(ApmConfig);

	PeerConnectionFactory = webrtc::CreatePeerConnectionFactory(
				nullptr, nullptr, SignalingThread.Get(), AudioDeviceModule,
				webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>(),
				webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>(),
				webrtc::CreateBuiltinVideoEncoderFactory(),
				webrtc::CreateBuiltinVideoDecoderFactory(),
				nullptr, AudioProcessingModule
	  ).release();

	// Check
	if (!PeerConnectionFactory)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Creating PeerConnectionFactory | Failed"));
		return;
	}

	webrtc::PeerConnectionFactoryInterface::Options Options;
	Options.crypto_options.srtp.enable_gcm_crypto_suites = true;
	PeerConnectionFactory->SetOptions(Options);
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> FWebRTCPeerConnection::GetPeerConnectionFactory()
{
	if (PeerConnectionFactory == nullptr)
	{
		CreatePeerConnectionFactory();
	}

	return PeerConnectionFactory;
}

rtc::scoped_refptr<FAudioDeviceModule> FWebRTCPeerConnection::GetAudioDeviceModule()
{
	if (PeerConnectionFactory == nullptr)
	{
		CreatePeerConnectionFactory();
	}

	return AudioDeviceModule;
}

webrtc::PeerConnectionInterface::RTCConfiguration FWebRTCPeerConnection::GetDefaultConfig()
{
	FRTCConfig Config(webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);

	Config.set_cpu_adaptation(false);
	Config.combined_audio_video_bwe.emplace(true);
	Config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

	return Config;
}
  
FWebRTCPeerConnection* FWebRTCPeerConnection::Create(const FRTCConfig& Config)
{
	if(PeerConnectionFactory == nullptr)
	{
		CreatePeerConnectionFactory();
	}

	FWebRTCPeerConnection * PeerConnectionInstance = new FWebRTCPeerConnection();
	webrtc::PeerConnectionDependencies deps(PeerConnectionInstance);

	PeerConnectionInstance->PeerConnection =
			PeerConnectionFactory->CreatePeerConnection(Config,
														nullptr,
														nullptr,
														PeerConnectionInstance);

	PeerConnectionInstance->CreateSessionDescription =
			MakeUnique<FCreateSessionDescriptionObserver>();
	PeerConnectionInstance->LocalSessionDescription  =
			MakeUnique<FSetSessionDescriptionObserver>();
	PeerConnectionInstance->RemoteSessionDescription =
			MakeUnique<FSetSessionDescriptionObserver>();

	return PeerConnectionInstance;
}

FWebRTCPeerConnection::FSetSessionDescriptionObserver*
FWebRTCPeerConnection::GetLocalDescriptionObserver()
{
	return LocalSessionDescription.Get();
}

FWebRTCPeerConnection::FSetSessionDescriptionObserver*
FWebRTCPeerConnection::GetRemoteDescriptionObserver()
{
	return RemoteSessionDescription.Get();
}

FWebRTCPeerConnection::FCreateSessionDescriptionObserver*
FWebRTCPeerConnection::GetCreateDescriptionObserver()
{
	return CreateSessionDescription.Get();
}

const FWebRTCPeerConnection::FSetSessionDescriptionObserver*
FWebRTCPeerConnection::GetLocalDescriptionObserver() const
{
	return LocalSessionDescription.Get();
}

const FWebRTCPeerConnection::FSetSessionDescriptionObserver*
FWebRTCPeerConnection::GetRemoteDescriptionObserver() const
{
	return RemoteSessionDescription.Get();
}

const FWebRTCPeerConnection::FCreateSessionDescriptionObserver*
FWebRTCPeerConnection::GetCreateDescriptionObserver() const
{
	return CreateSessionDescription.Get();
}


void FWebRTCPeerConnection::CreateOffer()
{
	SignalingThread->PostTask(RTC_FROM_HERE, [this]() {
		PeerConnection->CreateOffer(CreateSessionDescription.Release(),
									OaOptions);
	});
}

template<typename Callback>
webrtc::SessionDescriptionInterface* FWebRTCPeerConnection::CreateDescription(const std::string& Type,
									const std::string& Sdp,
									Callback&& Failed)
{
	if (Type.empty() || Sdp.empty())
	{
		std::string Msg = "Wrong input parameter, type or sdp missing";
		Failed(Msg);
		return nullptr;
	}

	webrtc::SdpParseError ParseError;
	webrtc::SessionDescriptionInterface* SessionDescription(webrtc::CreateSessionDescription(Type, Sdp, &ParseError));

	if (!SessionDescription)
	{
		std::ostringstream oss;
		oss << "Can't parse received session description message. SdpParseError line "
			<< ParseError.line <<  " : " + ParseError.description;

		Failed(oss.str());

		return nullptr;
	}

	return SessionDescription;
}

void FWebRTCPeerConnection::SetLocalDescription(const std::string& Sdp,
												const std::string& Type)
{
	  auto * SessionDescription = CreateDescription(Type,
													 Sdp,
													 std::ref(LocalSessionDescription->OnFailureCallback));

	  if(!SessionDescription) return;

	  PeerConnection->SetLocalDescription(LocalSessionDescription.Release(),
										  SessionDescription);
}

void FWebRTCPeerConnection::SetRemoteDescription(const std::string& Sdp,
												 const std::string& Type)
{
	auto * SessionDescription = CreateDescription(Type,
												  Sdp,
												  std::ref(RemoteSessionDescription->OnFailureCallback));

	if(!SessionDescription) return;

	PeerConnection->SetRemoteDescription(RemoteSessionDescription.Release(), SessionDescription);
}

void FWebRTCPeerConnection::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState)
{}

void FWebRTCPeerConnection::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>)
{}

void FWebRTCPeerConnection::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>)
{}

void FWebRTCPeerConnection::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>,
				const FMediaStreamVector&)
{}

void FWebRTCPeerConnection::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> Transceiver)
{}

void FWebRTCPeerConnection::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface>)
{}

void FWebRTCPeerConnection::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>)
{}

void FWebRTCPeerConnection::OnRenegotiationNeeded()
{}

void FWebRTCPeerConnection::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState)
{}

void FWebRTCPeerConnection::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState)
{}

void FWebRTCPeerConnection::OnIceCandidate(const webrtc::IceCandidateInterface*)
{}

void FWebRTCPeerConnection::OnIceConnectionReceivingChange(bool)
{}
