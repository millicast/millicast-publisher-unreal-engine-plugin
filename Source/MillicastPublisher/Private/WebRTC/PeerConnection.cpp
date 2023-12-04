// Copyright Dolby.io 2023. All Rights Reserved.

#include "PeerConnection.h"
#include "MillicastPublisherPrivate.h"
#include "AudioDeviceModule.h"
#if !PLATFORM_ANDROID && !PLATFORM_IOS
#include "VideoEncoderFactory.h"
#endif
#include "MillicastVideoEncoderFactory.h"
#include "SimulcastEncoderFactory.h"

#include <sstream>

#include "AudioDeviceModule.h"
#include "Stats.h"

namespace Millicast::Publisher
{

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> FWebRTCPeerConnection::PeerConnectionFactory = nullptr;
TUniquePtr<rtc::Thread> FWebRTCPeerConnection::SignalingThread = nullptr;
rtc::scoped_refptr<FAudioDeviceModule> FWebRTCPeerConnection::AudioDeviceModule = nullptr;
std::unique_ptr<webrtc::TaskQueueFactory> FWebRTCPeerConnection::TaskQueueFactory = nullptr;

void FWebRTCPeerConnection::CreatePeerConnectionFactory()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Creating FWebRTCPeerConnectionFactory"));

	SignalingThread  = TUniquePtr<rtc::Thread>(rtc::Thread::Create().release());
	SignalingThread->SetName("WebRTCSignalingThread", nullptr);
	SignalingThread->Start();

	TaskQueueFactory = webrtc::CreateDefaultTaskQueueFactory();
	AudioDeviceModule = FAudioDeviceModule::Create();

	rtc::scoped_refptr<webrtc::AudioProcessing> AudioProcessingModule = webrtc::AudioProcessingBuilder().Create();
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
	// ApmConfig.level_estimation.enabled = false;

	// Apply the config.
	AudioProcessingModule->ApplyConfig(ApmConfig);

	PeerConnectionFactory = webrtc::CreatePeerConnectionFactory(
				nullptr, nullptr, SignalingThread.Get(), AudioDeviceModule,
				webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>(),
				webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>(),
				std::make_unique<FSimulcastEncoderFactory>(),
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

TArray<FString> FWebRTCPeerConnection::GetSupportedVideoCodecs()
{
	return GetSupportedCodecs<cricket::MediaType::MEDIA_TYPE_VIDEO>();
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

	FWebRTCPeerConnection* PeerConnectionInstance = new FWebRTCPeerConnection();
	webrtc::PeerConnectionDependencies deps(PeerConnectionInstance);

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	PeerConnectionInstance->PeerConnection =
		PeerConnectionFactory->CreatePeerConnection(Config,
			nullptr,
			nullptr,
			PeerConnectionInstance);
#else
	auto result = PeerConnectionFactory->CreatePeerConnectionOrError(Config, std::move(deps));
	if (!result.ok())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Could not create peerconnection : %S"), result.error().message());
		PeerConnectionInstance->PeerConnection = nullptr;
		return nullptr;
	}

	PeerConnectionInstance->PeerConnection = result.value();
#endif

	PeerConnectionInstance->CreateSessionDescription =
			MakeUnique<FCreateSessionDescriptionObserver>();
	PeerConnectionInstance->LocalSessionDescription  =
			MakeUnique<FSetSessionDescriptionObserver>();
	PeerConnectionInstance->RemoteSessionDescription =
			MakeUnique<FSetSessionDescriptionObserver>();

	return PeerConnectionInstance;
}

TArray<FString> FWebRTCPeerConnection::GetSupportedAudioCodecs()
{
	return GetSupportedCodecs<cricket::MediaType::MEDIA_TYPE_AUDIO>();
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

webrtc::SessionDescriptionInterface* FWebRTCPeerConnection::CreateDescription(const std::string& Type,
									const std::string& Sdp,
									std::string& Error)
{
	if (Type.empty() || Sdp.empty())
	{
		Error = "Wrong input parameter, type or sdp missing";
		return nullptr;
	}

	webrtc::SdpParseError ParseError;
	webrtc::SessionDescriptionInterface* SessionDescription(webrtc::CreateSessionDescription(Type, Sdp, &ParseError));

	if (!SessionDescription)
	{
		std::ostringstream oss;
		oss << "Can't parse received session description message. SdpParseError line "
			<< ParseError.line <<  " : " + ParseError.description;

		Error = oss.str();
		return nullptr;
	}

	return SessionDescription;
}

template<cricket::MediaType T>
TArray<FString> FWebRTCPeerConnection::GetSupportedCodecs()
{
	TArray<FString> codecs;

	auto senderCapabilities = GetPeerConnectionFactory()->GetRtpSenderCapabilities(T);

	// remove rtx red ulpfec from the list
	senderCapabilities.codecs.erase(std::remove_if(senderCapabilities.codecs.begin(), 
			senderCapabilities.codecs.end(),
			[](auto& c) { return c.name == cricket::kRtxCodecName || c.name == cricket::kUlpfecCodecName || c.name == cricket::kUlpfecCodecName; }),
		senderCapabilities.codecs.end()
	);

	// remove any duplicates
	auto it = std::unique(senderCapabilities.codecs.begin(), senderCapabilities.codecs.end(),
		[](auto& c1, auto& c2) { return c1.name == c2.name; });
	senderCapabilities.codecs.erase(it, senderCapabilities.codecs.end());

	for (auto& c : senderCapabilities.codecs)
	{
		codecs.Add(ToString(c.name));
	}

	return codecs;
}

void FWebRTCPeerConnection::SetBitrates(TSharedPtr<webrtc::BitrateSettings> InBitrates)
{
	Bitrates = InBitrates;
	PeerConnection->SetBitrate(*Bitrates);
}

void FWebRTCPeerConnection::ApplyBitrates(cricket::SessionDescription* Sdp)
{
	if(!Bitrates.IsValid())
	{
		return;
	}

	std::vector<cricket::ContentInfo>& ContentInfos = Sdp->contents();

	// Set the start, min, and max bitrate accordingly.
	for (cricket::ContentInfo& Content : ContentInfos)
	{
		cricket::MediaContentDescription* MediaDescription = Content.media_description();
		if (MediaDescription->type() == cricket::MediaType::MEDIA_TYPE_VIDEO)
		{
			cricket::VideoContentDescription* VideoDescription = MediaDescription->as_video();
			std::vector<cricket::VideoCodec> CodecsCopy = VideoDescription->codecs();
			for (cricket::VideoCodec& Codec : CodecsCopy)
			{
				// Note: These codec params are in kilobits, not bits!
				Codec.SetParam(cricket::kCodecParamMinBitrate, Bitrates->min_bitrate_bps.value());
				Codec.SetParam(cricket::kCodecParamStartBitrate, Bitrates->start_bitrate_bps.value());
				Codec.SetParam(cricket::kCodecParamMaxBitrate, Bitrates->max_bitrate_bps.value());
			}
			VideoDescription->set_codecs(CodecsCopy);
		}
	}
}

void FWebRTCPeerConnection::SetLocalDescription(const std::string& Sdp,
												const std::string& Type)
{
	std::string Error;
	auto * SessionDescription = CreateDescription(Type, Sdp, Error);

	if (!SessionDescription)
	{
		LocalSessionDescription->OnFailureEvent.Broadcast(Error);
		return;
	}

	ApplyBitrates(SessionDescription->description());

	PeerConnection->SetLocalDescription(LocalSessionDescription.Release(),
										SessionDescription);
}

void FWebRTCPeerConnection::SetRemoteDescription(const std::string& Sdp,
												 const std::string& Type)
{
	std::string Error;
	auto * SessionDescription = CreateDescription(Type, Sdp, Error);

	if (!SessionDescription)
	{
		RemoteSessionDescription->OnFailureEvent.Broadcast(Error);
		return;
	}

	ApplyBitrates(SessionDescription->description());

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
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("OnRenegociationNeeded"));
}

void FWebRTCPeerConnection::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState)
{}

void FWebRTCPeerConnection::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState)
{}

void FWebRTCPeerConnection::OnIceCandidate(const webrtc::IceCandidateInterface*)
{}

void FWebRTCPeerConnection::OnIceConnectionReceivingChange(bool)
{}


void FWebRTCPeerConnection::PollStats()
{
	if (PeerConnection)
	{
		std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> Transceivers = PeerConnection->GetTransceivers();
		for (rtc::scoped_refptr<webrtc::RtpTransceiverInterface> Transceiver : Transceivers)
		{
			PeerConnection->GetStats(Transceiver->sender(), RTCStatsCollector.Get());
		}
	}
}

void FWebRTCPeerConnection::EnableStats(bool Enable)
{
	if (Enable && !RTCStatsCollector)
	{
		RTCStatsCollector = MakeUnique<FRTCStatsCollector>(this);
	}
	else
	{
		RTCStatsCollector = nullptr;
	}
}

}