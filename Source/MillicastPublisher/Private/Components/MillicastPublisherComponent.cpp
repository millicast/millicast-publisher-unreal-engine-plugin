// Copyright Dolby.io 2023. All Rights Reserved.

#include "MillicastPublisherComponent.h"
#include "MillicastPublisherPrivate.h"
#include "WebRTC/FrameTransformer.h"

#include <string>

#include "Http.h"

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "WebRTC/PeerConnection.h"

#include "Util.h"

#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"

#define WEAK_CAPTURE WeakThis = TWeakObjectPtr<UMillicastPublisherComponent>(this)

constexpr auto HTTP_OK = 200;

inline FString ToString(EMillicastVideoCodecs Codec)
{
	switch (Codec)
	{
		default:
		case EMillicastVideoCodecs::Vp8:
			return TEXT("vp8");
		case EMillicastVideoCodecs::Vp9:
			return TEXT("vp9");
		case EMillicastVideoCodecs::H264:
			return TEXT("h264");
	}
}

inline FString ToString(EMillicastAudioCodecs Codec)
{
	switch (Codec)
	{
	default:
	case EMillicastAudioCodecs::Opus:
		return TEXT("opus");
	}
}

UMillicastPublisherComponent::UMillicastPublisherComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Event received from websocket signaling
	EventBroadcaster.Emplace("active", [this](TSharedPtr<FJsonObject> Msg) { ParseActiveEvent(Msg); });
	EventBroadcaster.Emplace("inactive", [this](TSharedPtr<FJsonObject> Msg) { ParseInactiveEvent(Msg); });
	EventBroadcaster.Emplace("viewercount", [this](TSharedPtr<FJsonObject> Msg) { ParseViewerCountEvent(Msg); });


	PeerConnectionConfig = 
		MakeUnique<Millicast::Publisher::FWebRTCPeerConnectionConfig>(
			Millicast::Publisher::FWebRTCPeerConnection::GetDefaultConfig());

	MaximumBitrate = 4'000'000;
	StartingBitrate = 2'000'000;
	MinimumBitrate = 1'000'000;
}

void UMillicastPublisherComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	UnPublish();
}


/**
	Initialize this component with the media source required for publishing Millicast audio, video.
	Returns false, if the MediaSource is already been set. This is usually the case when this component is
	initialized in Blueprints.
*/
bool UMillicastPublisherComponent::Initialize(UMillicastPublisherSource* InMediaSource)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Initialize Millicast Publisher component"));
	if (MillicastMediaSource == nullptr && InMediaSource != nullptr)
	{
		MillicastMediaSource = InMediaSource;
	}

	return InMediaSource != nullptr && InMediaSource == MillicastMediaSource;
}

void UMillicastPublisherComponent::SetupIceServersFromJson(TArray<TSharedPtr<FJsonValue>> IceServersField)
{
	using namespace Millicast::Publisher;

	(*PeerConnectionConfig)->servers.clear();
	for (const auto& Server : IceServersField)
	{
		// Grab Ice Server Details
		const TSharedPtr<FJsonObject>* IceServerJson;
		if (!Server->TryGetObject(IceServerJson))
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not read ice server json"));
			continue;
		}

		webrtc::PeerConnectionInterface::IceServer IceServer;

		// Grab urls
		{
			TArray<FString> IceServerUrls;
			if( (*IceServerJson)->TryGetStringArrayField("urls", IceServerUrls) )
			{
				for (const auto& Url : IceServerUrls)
				{
					IceServer.urls.push_back(to_string(Url));
				}
			}
		}

		// Grab username
		{
			FString IceServerUsername;
			if( (*IceServerJson)->TryGetStringField("username", IceServerUsername) )
			{
				IceServer.username = to_string(IceServerUsername);
			}
		}

		// Grab credentials
		{
			FString IceServerPassword;
			if( (*IceServerJson)->TryGetStringField("credential", IceServerPassword) )
			{
				IceServer.password = to_string(IceServerPassword);
			}
		}

		(*PeerConnectionConfig)->servers.push_back(IceServer);
	}
}

void UMillicastPublisherComponent::ParseActiveEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	OnActive.Broadcast();

	// Unmute audio and video
	if (Automute && MillicastMediaSource)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Auto unmuting media tracks"));
		MillicastMediaSource->MuteVideo(false);
		MillicastMediaSource->MuteAudio(false);
	}
}

void UMillicastPublisherComponent::ParseInactiveEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	OnInactive.Broadcast();

	if (Automute && MillicastMediaSource)
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Auto muting media tracks"));
		MillicastMediaSource->MuteVideo(true);
		MillicastMediaSource->MuteAudio(true);
	}
}

void UMillicastPublisherComponent::ParseViewerCountEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	const auto DataJson = JsonMsg->GetObjectField("data");
	const auto Count = DataJson->GetIntegerField("viewercount");

	OnViewerCount.Broadcast(Count);
}

void UMillicastPublisherComponent::ParseDirectorResponse(FHttpResponsePtr Response)
{
	const FString& ResponseDataString = Response->GetContentAsString();
	UE_LOG(LogMillicastPublisher, Log, TEXT("Director response : \n %s \n"), *ResponseDataString);

	TSharedPtr<FJsonObject> ResponseDataJson;
	const auto JsonReader = TJsonReaderFactory<>::Create(ResponseDataString);

	// Deserialize received JSON message
	if (FJsonSerializer::Deserialize(JsonReader, ResponseDataJson)) 
	{
		const auto* DataField = ResponseDataJson->GetObjectField("data").Get();

		// Extract JSON WebToken, Websocket URL and ice servers configuration
		const auto& Jwt = DataField->GetStringField("jwt");
		const auto& WebSocketUrlField = DataField->GetArrayField("urls")[0];
		const auto& IceServersField = DataField->GetArrayField("iceServers");

		FString WsUrl;
		WebSocketUrlField->TryGetString(WsUrl);

		UE_LOG(LogMillicastPublisher, Log, TEXT("WsUrl : %s \njwt : %s"), *WsUrl, *Jwt);

		SetupIceServersFromJson(IceServersField);

		// Creates websocket connection and starts signaling
		StartWebSocketConnection(WsUrl, Jwt);
	}
}

/**
	Authenticating through director api
*/
bool UMillicastPublisherComponent::Publish()
{
	if (!IsValid(MillicastMediaSource))
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("The MIllicats Media Source is not valid"));
		return false;
	}

	if (IsPublishing())
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Millicast Publisher Component is already publishing"));
		return false;
	}
	
	UE_LOG(LogMillicastPublisher, Log, TEXT("Making HTTP director request"));

	// Create an HTTP request
	const auto PostHttpRequestShared = FHttpModule::Get().CreateRequest();
	auto& PostHttpRequest = PostHttpRequestShared.Get();

	// Request parameters
	PostHttpRequest.SetURL(MillicastMediaSource->GetUrl());
	PostHttpRequest.SetVerb("POST");
	// Fill HTTP request headers
	PostHttpRequest.SetHeader("Content-Type", "application/json");
	PostHttpRequest.SetHeader("Authorization", "Bearer " + MillicastMediaSource->PublishingToken);

	// Creates JSON data fro the request
	auto RequestData = MakeShared<FJsonObject>();
	RequestData->SetStringField("streamName", MillicastMediaSource->StreamName);

	// Serialize JSON into FString
	FString SerializedRequestData;
	auto JsonWriter = TJsonWriterFactory<>::Create(&SerializedRequestData);
	FJsonSerializer::Serialize(RequestData, JsonWriter);

	// Fill HTTP request data
	PostHttpRequest.SetContentAsString(SerializedRequestData);

	PostHttpRequest.OnProcessRequestComplete()
		.BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
	{
		if (!bConnectedSuccessfully || Response->GetResponseCode() != HTTP_OK) 
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Publisher HTTP request failed [code] %d [response] %s \n [body] %s"), Response->GetResponseCode(), *Response->GetContentType(), *Response->GetContentAsString());

			FString ErrorMsg = Response->GetContentAsString();
			OnAuthenticationFailure.Broadcast(Response->GetResponseCode(), ErrorMsg);
			return;
		}

		// HTTP request sucessful
		ParseDirectorResponse(Response);
	});

	return PostHttpRequest.ProcessRequest();
}

bool UMillicastPublisherComponent::PublishWithWsAndJwt(const FString& WsUrl, const FString& Jwt)
{
	return IsValid(MillicastMediaSource) && StartWebSocketConnection(WsUrl, Jwt);
}

/**
	Attempts to stop publishing audio, video.
*/
void UMillicastPublisherComponent::UnPublish()
{
	// Close websocket connection, can exist in inactive connection state
	if (auto* pWS = WS.Get())
	{
		pWS->OnConnected().Remove(OnConnectedHandle);
		pWS->OnConnectionError().Remove(OnConnectionErrorHandle);
		pWS->OnClosed().Remove(OnClosedHandle);
		pWS->OnMessage().Remove(OnMessageHandle);
		pWS->Close();

		WS = nullptr;
	}

	if (!IsConnectionActive())
	{
		return;
	}
	State = EMillicastPublisherState::Disconnected;

	UE_LOG(LogMillicastPublisher, Display, TEXT("Unpublish"));

	FScopeLock Lock(&CriticalSection);
	
	// Release peerconnection and stop capture
	if(PeerConnection)
	{
		if (auto* CreateSessionDescriptionObserver = PeerConnection->GetCreateDescriptionObserver())
		{
			 CreateSessionDescriptionObserver->OnSuccessEvent.Remove(CreateSessionSuccessHandle);
			 CreateSessionDescriptionObserver->OnFailureEvent.Remove(CreateSessionFailureHandle);
		}
		if (auto* LocalDescriptionObserver = PeerConnection->GetLocalDescriptionObserver())
		{
			LocalDescriptionObserver->OnSuccessEvent.Remove(LocalSuccessHandle);
			LocalDescriptionObserver->OnFailureEvent.Remove(LocalFailureHandle);
		}
		if (auto* RemoteDescriptionObserver = PeerConnection->GetRemoteDescriptionObserver())
		{
			RemoteDescriptionObserver->OnSuccessEvent.Remove(RemoteSuccessHandle);
			RemoteDescriptionObserver->OnFailureEvent.Remove(RemoteFailureHandle);
		}

		PeerConnection.Reset();

		// Check here as the PublisherSource code has already been moved to not rely on cleanup handling by the ActorComponent anymore
		if( MillicastMediaSource->IsCapturing() )
		{
			MillicastMediaSource->StopCapture();
		}
	}
}

bool UMillicastPublisherComponent::IsPublishing() const
{
	return State == EMillicastPublisherState::Connected;
}

bool UMillicastPublisherComponent::StartWebSocketConnection(const FString& Url, const FString& Jwt)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Start WebSocket connection"));
	// Check if WebSocket module is loaded. It may crash otherwise.
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("MillicastPublisher");
	const FString OSName = UGameplayStatics::GetPlatformName();
	const FString PluginVersion = Plugin ? Plugin->GetDescriptor().VersionName : "Unknown";

	const TMap<FString, FString> Headers {
		{"user-agent", FString::Printf(TEXT("MillicastPublisherUE5/%s/%s"), *OSName, *PluginVersion)}
	};

	WS = FWebSocketsModule::Get().CreateWebSocket(Url + "?token=" + Jwt, FString(), Headers);

	// Attach callback
	OnConnectedHandle = WS->OnConnected().AddLambda([this]() { OnConnected(); });
	OnConnectionErrorHandle = WS->OnConnectionError().AddLambda([this](const FString& Error) { OnConnectionError(Error); });
	OnClosedHandle = WS->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) { OnClosed(StatusCode, Reason, bWasClean); });
	OnMessageHandle = WS->OnMessage().AddLambda([this](const FString& Msg) { OnMessage(Msg); });

	WS->Connect();

	return true;
}

bool UMillicastPublisherComponent::PublishToMillicast()
{
	if (IsConnectionActive())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("[UMillicastPublisherComponent::PublishToMillicast] Called twice in successsion"));
		return false;
	}
	State = EMillicastPublisherState::Connecting;

	using namespace Millicast::Publisher;

	PeerConnection.Reset(FWebRTCPeerConnection::Create(*(*PeerConnectionConfig)));

	// Starts the capture first and add track to the peerconnection
	// TODO: add a boolean to let choose autoplay or not
	CaptureAndAddTracks();

	// Get session description observers
	auto * CreateSessionDescriptionObserver = PeerConnection->GetCreateDescriptionObserver();
	auto * LocalDescriptionObserver  = PeerConnection->GetLocalDescriptionObserver();
	auto * RemoteDescriptionObserver = PeerConnection->GetRemoteDescriptionObserver();

	CreateSessionSuccessHandle = CreateSessionDescriptionObserver->OnSuccessEvent.AddLambda([this](const std::string& type, const std::string& sdp)
	{
		UE_LOG(LogMillicastPublisher, Display, TEXT("pc.createOffer() | sucess\nsdp : %s"), *FString(sdp.c_str()));

		// Search for this expression and add the stereo flag to enable stereo
		const std::string s = "minptime=10;useinbandfec=1";
		std::string sdp_non_const = sdp;
		std::ostringstream oss;
		oss << s << "; stereo=1";

		auto pos = sdp.find(s);
		if (pos != std::string::npos)
		{
			sdp_non_const.replace(sdp.find(s), s.size(), oss.str());
		}

		// Set local description
		{
			FScopeLock Lock(&CriticalSection);
			PeerConnection->SetLocalDescription(sdp_non_const, type);
		}
	});

	CreateSessionFailureHandle = CreateSessionDescriptionObserver->OnFailureEvent.AddLambda([this](const std::string& err)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("pc.createOffer() | Error: %s"), *FString(err.c_str()));
		HandleError("Could not create offer");
	});

	LocalSuccessHandle = LocalDescriptionObserver->OnSuccessEvent.AddLambda([this]()
	{
		FScopeLock Lock(&CriticalSection);

		UE_LOG(LogMillicastPublisher, Log, TEXT("pc.setLocalDescription() | sucess"));

		if (!WS || !WS.IsValid() || !PeerConnection)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("WebSocket is closed, can not send SDP"));
			HandleError("Websocket is closed. Can not send SDP to server.");
			return;
		}
		 
		std::string sdp;
		(*PeerConnection)->local_description()->ToString(&sdp);

		// Add events we want to receive from millicast
		TArray<TSharedPtr<FJsonValue>> eventsJson;
		TArray<FString> EvKeys;
		EventBroadcaster.GetKeys(EvKeys);

		for (auto& ev : EvKeys) 
		{
			eventsJson.Add(MakeShared<FJsonValueString>(ev));
		}

		// Fill signaling data
		auto DataJson = MakeShared<FJsonObject>();
		DataJson->SetStringField("name", MillicastMediaSource->StreamName);
		DataJson->SetStringField("sdp", ToString(sdp));
		DataJson->SetStringField("codec", ToString(SelectedVideoCodec));
		DataJson->SetArrayField("events", eventsJson);

		// If multisource feature
		if (!MillicastMediaSource->SourceId.IsEmpty())
		{
			DataJson->SetStringField("sourceId", MillicastMediaSource->SourceId);
		}

		auto Payload = MakeShared<FJsonObject>();
		Payload->SetStringField("type", "cmd");
		Payload->SetNumberField("transId", std::rand());
		Payload->SetStringField("name", "publish");
		Payload->SetObjectField("data", DataJson);

		FString StringStream;
		auto Writer = TJsonWriterFactory<>::Create(&StringStream);
		FJsonSerializer::Serialize(Payload, Writer);

		WS->Send(StringStream);

		UE_LOG(LogMillicastPublisher, Log, TEXT("WebSocket publish payload : %s"), *StringStream);
	});

	LocalFailureHandle = LocalDescriptionObserver->OnFailureEvent.AddLambda([this](const std::string& err)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set local description failed : %s"), *FString(err.c_str()));
		HandleError("Could not set local description");
	});

	RemoteSuccessHandle = RemoteDescriptionObserver->OnSuccessEvent.AddLambda([this]()
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Set remote description suceeded"));

		State = EMillicastPublisherState::Connected;
		OnPublishing.Broadcast();
	});

	RemoteFailureHandle = RemoteDescriptionObserver->OnFailureEvent.AddLambda([this](const std::string& err)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set remote description failed : %s"), *FString(err.c_str()));
		HandleError("Could not set remote description");
	});

	// Send only
	PeerConnection->OaOptions.offer_to_receive_video = false;
	PeerConnection->OaOptions.offer_to_receive_audio = false;

	// Bitrate settings
	webrtc::BitrateSettings bitrateParameters;
	if (MinimumBitrate.IsSet())
	{
		bitrateParameters.min_bitrate_bps = *MinimumBitrate;
	}
	if (MaximumBitrate.IsSet())
	{
		bitrateParameters.max_bitrate_bps = *MaximumBitrate;
	}
	if (StartingBitrate.IsSet())
	{
		bitrateParameters.start_bitrate_bps = *StartingBitrate;
	}
	(*PeerConnection)->SetBitrate(bitrateParameters);

	// Create offer
	UE_LOG(LogMillicastPublisher, Log, TEXT("Create offer"));
	PeerConnection->CreateOffer();
	PeerConnection->EnableStats(RtcStatsEnabled);

	return true;
}

bool UMillicastPublisherComponent::IsConnectionActive() const
{
	return State != EMillicastPublisherState::Disconnected;
}

/* WebSocket Callback
*****************************************************************************/

void UMillicastPublisherComponent::OnConnected()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Connected"));
	PublishToMillicast();
}

void UMillicastPublisherComponent::OnConnectionError(const FString& Error)
{
	State = EMillicastPublisherState::Disconnected;

	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Connection error : %s"), *Error);
	HandleError("Could not connect websocket");
}

void UMillicastPublisherComponent::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	State = EMillicastPublisherState::Disconnected;

	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Closed"));
}

void UMillicastPublisherComponent::OnMessage(const FString& Msg)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket new Message : %s"), *Msg);

	TSharedPtr<FJsonObject> ResponseJson;
	auto Reader = TJsonReaderFactory<>::Create(Msg);

	// Deserialize JSON message
	bool ok = FJsonSerializer::Deserialize(Reader, ResponseJson);
	if (!ok)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Could not deserialize JSON"));
		return;
	}

	FString Type;
	if (!ResponseJson->TryGetStringField("type", Type))
	{
		return;
	}

	// Signaling response
	if(Type == "response") 
	{
		auto DataJson = ResponseJson->GetObjectField("data");
		FString Sdp = DataJson->GetStringField("sdp");
		FString ServerId = DataJson->GetStringField("publisherId");
		FString ClusterId = DataJson->GetStringField("clusterId");

		FScopeLock Lock(&CriticalSection);
		if (PeerConnection) 
		{
			Sdp += FString(TEXT("a=x-google-flag:conference\r\n"));
			PeerConnection->SetRemoteDescription(Millicast::Publisher::to_string(Sdp));
			PeerConnection->ServerId = MoveTemp(ServerId);
			PeerConnection->ClusterId = MoveTemp(ClusterId);
		}
	}
	else if(Type == "error") // Error in the request data sent to millicast
	{
		FString ErrorMessage;
		ResponseJson->TryGetStringField("data", ErrorMessage);
		UE_LOG(LogMillicastPublisher, Error, TEXT("WebSocket error : %s"), *ErrorMessage);
	}
	else if(Type == "event") // Events received from millicast
	{
		FString EventName;
		ResponseJson->TryGetStringField("name", EventName);

		UE_LOG(LogMillicastPublisher, Log, TEXT("Received event : %s"), *EventName);

		if (EventBroadcaster.Contains(EventName))
		{
			EventBroadcaster[EventName](ResponseJson);
		}
	}
	else 
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("WebSocket response type not handled (yet?) %s"), *Type);
	}
}

void UMillicastPublisherComponent::SetSimulcast(webrtc::RtpTransceiverInit& TransceiverInit)
{
	webrtc::RtpEncodingParameters params;
	params.active = true;
	params.max_bitrate_bps = MaximumBitrate.Get(4'000'000);
	params.rid = "h";
	TransceiverInit.send_encodings.push_back(params);

	params.max_bitrate_bps = MaximumBitrate.Get(4'000'000) / 2;
	params.active = true;
	params.rid = "m";
	params.scale_resolution_down_by = 2;
	TransceiverInit.send_encodings.push_back(params);

	params.max_bitrate_bps = MaximumBitrate.Get(4'000'000) / 4;
	params.active = true;
	params.rid = "l";
	params.scale_resolution_down_by = 4;
	TransceiverInit.send_encodings.push_back(params);
}

void UMillicastPublisherComponent::CaptureAndAddTracks()
{
	// Starts audio and video capture
	MillicastMediaSource->StartCapture(GetWorld(), Simulcast, [this](auto&& Track)
	{
		// Add transceiver with sendonly direction
		webrtc::RtpTransceiverInit init;
		init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
		init.stream_ids = { "unrealstream" };

		if (Track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind && Simulcast)
		{
			SetSimulcast(init);
		}
		else
		{
			webrtc::RtpEncodingParameters Encoding;
			if (MinimumBitrate.IsSet())
			{
				Encoding.min_bitrate_bps = *MinimumBitrate;
			}
			if (MaximumBitrate.IsSet())
			{
				Encoding.max_bitrate_bps = *MaximumBitrate;
			}
			Encoding.max_framerate = 60;
			Encoding.network_priority = webrtc::Priority::kHigh;
			init.send_encodings.push_back(Encoding);
		}

		auto result = (*PeerConnection)->AddTransceiver(Track, init);

		if (result.ok())
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Add transceiver for %s track : %s"), 
				*FString( Track->kind().c_str() ), *FString( Track->id().c_str() ) );
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't add transceiver for %s track %s : %s"), 
				*FString(Track->kind().c_str()),
				*FString(Track->id().c_str()),
				*FString(result.error().message()));

			return;
		}

		if (Track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind && bUseFrameTransformer)
		{
			auto FrameTransformer = rtc::make_ref_counted<Millicast::Publisher::FFrameTransformer>(PeerConnection.Get());

			PeerConnection->OnTransformableFrame = [this](uint32 Ssrc, uint32 Timestamp, TArray<uint8>& Data) {
				Metadata = &Data;
				OnAddFrameMetadata.Broadcast(Ssrc, Timestamp);
			};

			auto Transceiver = result.value();
			Transceiver->sender()->SetEncoderToPacketizerFrameTransformer(FrameTransformer);
		}
	});

	if (Automute)
	{
		// Muting media tracks until there are viewers watching the stream
		UE_LOG(LogMillicastPublisher, Log, TEXT("Auto muting media tracks until viewers are watching"));
		MillicastMediaSource->MuteVideo(true);
		MillicastMediaSource->MuteAudio(true);
	}
}

void UMillicastPublisherComponent::UpdateBitrateSettings()
{
	if (PeerConnection)
	{
		webrtc::BitrateSettings BitrateParameters;

		if (MinimumBitrate.IsSet())
		{
			BitrateParameters.min_bitrate_bps = *MinimumBitrate;
		}

		if (MaximumBitrate.IsSet())
		{
			BitrateParameters.max_bitrate_bps = *MaximumBitrate;
		}

		if (StartingBitrate.IsSet())
		{
			BitrateParameters.start_bitrate_bps = *StartingBitrate;
		}

		const auto Error = (*PeerConnection)->SetBitrate(BitrateParameters);
		if (!Error.ok())
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Could not set maximum bitrate: %s"), *FString(Error.message()));
		}
	}
}

void UMillicastPublisherComponent::SetMinimumBitrate(int Bps)
{
	MinimumBitrate = Bps;

	UpdateBitrateSettings();
}

void UMillicastPublisherComponent::SetMaximumBitrate(int Bps)
{
	MaximumBitrate = Bps;

	UpdateBitrateSettings();
}

void UMillicastPublisherComponent::SetStartingBitrate(int Bps)
{
	StartingBitrate = Bps;

	UpdateBitrateSettings();
}

bool UMillicastPublisherComponent::SetVideoCodec(EMillicastVideoCodecs InVideoCodec)
{
	if (IsConnectionActive())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Cannot set video codec while publishing"));
		return false;
	}

	SelectedVideoCodec = InVideoCodec;
	return true;
}

bool UMillicastPublisherComponent::SetAudioCodec(EMillicastAudioCodecs InAudioCodec)
{
	if (IsConnectionActive())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Cannot set audio codec while publishing"));
		return false;
	}

	SelectedAudioCodec = InAudioCodec;
	return true;
}

void UMillicastPublisherComponent::EnableStats(bool Enable)
{
	RtcStatsEnabled = Enable;
	if (PeerConnection)
	{
		PeerConnection->EnableStats(Enable);
	}
}

void UMillicastPublisherComponent::EnableFrameTransformer(bool Enable)
{
	bUseFrameTransformer = Enable;
}

void UMillicastPublisherComponent::AddMetadata(const TArray<uint8>& Data)
{
	Metadata->Append(Data);
}

#if WITH_EDITOR
bool UMillicastPublisherComponent::CanEditChange(const FProperty* InProperty) const
{
	FString Name;
	InProperty->GetName(Name);

	if (Name == "Simulcast")
	{
		return SelectedVideoCodec != EMillicastVideoCodecs::Vp9;
	}

	return Super::CanEditChange(InProperty);
}

void UMillicastPublisherComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName Name = PropertyChangedEvent.GetPropertyName();

	if (Name == "SelectedVideoCodec")
	{
		if (SelectedVideoCodec == EMillicastVideoCodecs::Vp9)
		{
			Simulcast = false;
		}
	}
}

#endif

void UMillicastPublisherComponent::HandleError(const FString& Message)
{
	State = EMillicastPublisherState::Disconnected;
	OnPublishingError.Broadcast(Message);
}
