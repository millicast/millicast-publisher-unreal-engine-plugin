// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherComponent.h"
#include "MillicastPublisherPrivate.h"

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

constexpr auto HTTP_OK = 200;

// lambda check if the event is bound before broadcasting.
auto MakeBroadcastEvent = [](auto&& Event) {
	return [&Event](auto&& ... Args) {
		if (Event.IsBound()) 
		{
			Event.Broadcast(std::forward<Args>()...);
		}
	};
};

UMillicastPublisherComponent::UMillicastPublisherComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PeerConnection = nullptr;
	WS = nullptr;
	bIsPublishing = false;

	Bitrates = MakeShared<webrtc::PeerConnectionInterface::BitrateParameters>();
	
	// Default bitrates
	Bitrates->max_bitrate_bps = 2'500'000; // 2.5 megabit (this is the default in WebRTC anyway)
	Bitrates->min_bitrate_bps = 100'000; // 100 kilobit
	Bitrates->current_bitrate_bps = 1'000'000; // 1 megabit

	// Event received from websocket signaling
	EventBroadcaster.Emplace("active", MakeBroadcastEvent(OnActive));
	EventBroadcaster.Emplace("inactive", MakeBroadcastEvent(OnInactive));

	PeerConnectionConfig = FWebRTCPeerConnection::GetDefaultConfig();
}

UMillicastPublisherComponent::~UMillicastPublisherComponent()
{
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
	PeerConnectionConfig.servers.clear();
	for (auto& elt : IceServersField)
	{
		const TSharedPtr<FJsonObject>* IceServerJson;
		bool ok = elt->TryGetObject(IceServerJson);

		if (!ok)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not read ice server json"));
			continue;
		}

		TArray<FString> iceServerUrls;
		FString iceServerPassword, iceServerUsername;

		bool hasUrls = (*IceServerJson)->TryGetStringArrayField("urls", iceServerUrls);
		bool hasUsername = (*IceServerJson)->TryGetStringField("username", iceServerUsername);
		bool hasPassword = (*IceServerJson)->TryGetStringField("credential", iceServerPassword);

		webrtc::PeerConnectionInterface::IceServer iceServer;
		if (hasUrls)
		{
			for (auto& url : iceServerUrls)
			{
				iceServer.urls.push_back(to_string(url));
			}
		}
		if (hasUsername)
		{
			iceServer.username = to_string(iceServerUsername);
		}
		if (hasPassword)
		{
			iceServer.password = to_string(iceServerPassword);
		}

		PeerConnectionConfig.servers.push_back(iceServer);
	}
}

void UMillicastPublisherComponent::ParseDirectorResponse(FHttpResponsePtr Response)
{
	FString ResponseDataString = Response->GetContentAsString();
	UE_LOG(LogMillicastPublisher, Log, TEXT("Director response : \n %S \n"), *ResponseDataString);

	TSharedPtr<FJsonObject> ResponseDataJson;
	auto JsonReader = TJsonReaderFactory<>::Create(ResponseDataString);

	// Deserialize received JSON message
	if (FJsonSerializer::Deserialize(JsonReader, ResponseDataJson)) 
	{
		TSharedPtr<FJsonObject> DataField = ResponseDataJson->GetObjectField("data");

		// Extract JSON WebToken, Websocket URL and ice servers configuration
		auto Jwt = DataField->GetStringField("jwt");
		auto WebSocketUrlField = DataField->GetArrayField("urls")[0];
		auto IceServersField = DataField->GetArrayField("iceServers");

		FString WsUrl;
		WebSocketUrlField->TryGetString(WsUrl);

		UE_LOG(LogMillicastPublisher, Log, TEXT("WsUrl : %S \njwt : %S"),
			*WsUrl, *Jwt);

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
	if (!IsValid(MillicastMediaSource)) return false;
	
	UE_LOG(LogMillicastPublisher, Log, TEXT("Making HTTP director request"));
	// Create an HTTP request
	auto PostHttpRequest = FHttpModule::Get().CreateRequest();
	// Request parameters
	PostHttpRequest->SetURL(MillicastMediaSource->GetUrl());
	PostHttpRequest->SetVerb("POST");
	// Fill HTTP request headers
	PostHttpRequest->SetHeader("Content-Type", "application/json");
	PostHttpRequest->SetHeader("Authorization", "Bearer " + MillicastMediaSource->PublishingToken);

	// Creates JSON data fro the request
	auto RequestData = MakeShared<FJsonObject>();
	RequestData->SetStringField("streamName", MillicastMediaSource->StreamName);

	// Serialize JSON into FString
	FString SerializedRequestData;
	auto JsonWriter = TJsonWriterFactory<>::Create(&SerializedRequestData);
	FJsonSerializer::Serialize(RequestData, JsonWriter);

	// Fill HTTP request data
	PostHttpRequest->SetContentAsString(SerializedRequestData);

	PostHttpRequest->OnProcessRequestComplete()
		.BindLambda([this](FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			bool bConnectedSuccessfully) {
		// HTTP request sucessful
		if (bConnectedSuccessfully && Response->GetResponseCode() == HTTP_OK) 
		{
			ParseDirectorResponse(Response);
		}
		else 
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Director HTTP request failed %d %S"), Response->GetResponseCode(), *Response->GetContentType());
			FString ErrorMsg = Response->GetContentAsString();
			OnAuthenticationFailure.Broadcast(Response->GetResponseCode(), ErrorMsg);
		}
	});

	return PostHttpRequest->ProcessRequest();
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
	UE_LOG(LogMillicastPublisher, Display, TEXT("Unpublish"));
	// Release peerconnection and stop capture
	if(PeerConnection)
	{
		delete PeerConnection;
		PeerConnection = nullptr;

		MillicastMediaSource->StopCapture();
	}

	// Close websocket connection
	if (WS) {
		WS->Close();
		WS = nullptr;
	}

	bIsPublishing = false;
}

bool UMillicastPublisherComponent::IsPublishing() const
{
	return bIsPublishing;
}

bool UMillicastPublisherComponent::StartWebSocketConnection(const FString& Url,
                                                     const FString& Jwt)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Start WebSocket connection"));
	// Check if WebSocket module is loaded. It may crash otherwise.
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	WS = FWebSocketsModule::Get().CreateWebSocket(Url + "?token=" + Jwt);

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
	PeerConnection = FWebRTCPeerConnection::Create(PeerConnectionConfig);
	PeerConnection->SetBitrates(Bitrates);

	// Starts the capture first and add track to the peerconnection
	// TODO: add a boolean to let choose autoplay or not
	CaptureAndAddTracks();

	// Get session description observers
	auto * CreateSessionDescriptionObserver = PeerConnection->GetCreateDescriptionObserver();
	auto * LocalDescriptionObserver  = PeerConnection->GetLocalDescriptionObserver();
	auto * RemoteDescriptionObserver = PeerConnection->GetRemoteDescriptionObserver();

	CreateSessionDescriptionObserver->SetOnSuccessCallback([this](const std::string& type, const std::string& sdp) {
		UE_LOG(LogMillicastPublisher, Display, TEXT("pc.createOffer() | sucess\nsdp : %S"), sdp.c_str());

		// Search for this expression and add the stereo flag to enable stereo
		const std::string s = "minptime=10;useinbandfec=1";
		std::string sdp_non_const = sdp;
		std::ostringstream oss;
		oss << s << "; stereo=1";

		auto pos = sdp.find(s);
		if (pos != std::string::npos) {
			sdp_non_const.replace(sdp.find(s), s.size(), oss.str());
		}

		// Set local description
		PeerConnection->SetLocalDescription(sdp_non_const, type);
	});

	CreateSessionDescriptionObserver->SetOnFailureCallback([this](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("pc.createOffer() | Error: %S"), err.c_str());
		OnPublishingError.Broadcast(TEXT("Could not create offer"));
	});

	LocalDescriptionObserver->SetOnSuccessCallback([this]() {
		UE_LOG(LogMillicastPublisher, Log, TEXT("pc.setLocalDescription() | sucess"));
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
		DataJson->SetStringField("codec", MillicastMediaSource->GetVideoCodec().ToLower());
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

		UE_LOG(LogMillicastPublisher, Log, TEXT("WebSocket publish payload : %S"), *StringStream);
	});

	LocalDescriptionObserver->SetOnFailureCallback([this](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set local description failed : %S"), *ToString(err));
		OnPublishingError.Broadcast(TEXT("Could not set local description"));
	});

	RemoteDescriptionObserver->SetOnSuccessCallback([this]() {
		UE_LOG(LogMillicastPublisher, Log, TEXT("Set remote description suceeded"));

		bIsPublishing = true;

		OnPublishing.Broadcast();
	});
	RemoteDescriptionObserver->SetOnFailureCallback([this](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set remote description failed : %S"), *ToString(err));
		OnPublishingError.Broadcast(TEXT("Could not set remote description"));
	});

	// Send only
	PeerConnection->OaOptions.offer_to_receive_video = false;
	PeerConnection->OaOptions.offer_to_receive_audio = false;

	// Create offer
	UE_LOG(LogMillicastPublisher, Log, TEXT("Create offer"));
	PeerConnection->CreateOffer();

	return true;
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
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Connection error : %s"), *Error);
	OnPublishingError.Broadcast(TEXT("Could not connect websocket"));
}

void UMillicastPublisherComponent::OnClosed(int32 StatusCode,
                                     const FString& Reason,
                                     bool bWasClean)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Closed"))
}

void UMillicastPublisherComponent::OnMessage(const FString& Msg)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket new Message : %s"), *Msg);

	TSharedPtr<FJsonObject> ResponseJson;
	auto Reader = TJsonReaderFactory<>::Create(Msg);

	// Deserialize JSON message
	bool ok = FJsonSerializer::Deserialize(Reader, ResponseJson);
	if (!ok) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Could not deserialize JSON"));
		return;
	}

	FString Type;
	if(!ResponseJson->TryGetStringField("type", Type)) return;

	// Signaling response
	if(Type == "response") 
	{
		auto DataJson = ResponseJson->GetObjectField("data");
		FString Sdp = DataJson->GetStringField("sdp");
		if (PeerConnection) 
		{
			PeerConnection->SetRemoteDescription(to_string(Sdp));
		}
	}
	else if(Type == "error") // Error in the request data sent to millicast
	{
		FString errorMessage;
		auto dataJson = ResponseJson->TryGetStringField("data", errorMessage);

		UE_LOG(LogMillicastPublisher, Error, TEXT("WebSocket error : %s"), *errorMessage);
	}
	else if(Type == "event") // Events received from millicast
	{
		FString eventName;
		ResponseJson->TryGetStringField("name", eventName);

		UE_LOG(LogMillicastPublisher, Log, TEXT("Received event : %s"), *eventName);

		EventBroadcaster[eventName]();
	}
	else 
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("WebSocket response type not handled (yet?) %s"), *Type);
	}
}

template<typename TransceiverType, cricket::MediaType T>
void UMillicastPublisherComponent::SetCodecPreference(TransceiverType Transceiver)
{
	auto senderCapabilities = FWebRTCPeerConnection::GetPeerConnectionFactory()->GetRtpSenderCapabilities(T);

	std::vector<webrtc::RtpCodecCapability> codecs;
	auto predicate = [this](const auto& c) -> bool
	{
		if constexpr (T == cricket::MediaType::MEDIA_TYPE_VIDEO) {
			return (c.name.c_str() == MillicastMediaSource->GetVideoCodec() || c.name == cricket::kRtxCodecName || c.name == cricket::kUlpfecCodecName || c.name == cricket::kRedCodecName);
		}
		else {
			return c.name.c_str() == MillicastMediaSource->GetAudioCodec();
		}
	};

	std::copy_if(senderCapabilities.codecs.begin(), senderCapabilities.codecs.end(),
		std::back_inserter(codecs), predicate);

	if (codecs.empty())
	{
		UE_LOG(LogMillicastPublisher, Warning, TEXT("Could not find specified codec"));
		return;
	}

	auto err = Transceiver->SetCodecPreferences(codecs);
	if (!err.ok())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("%s"), err.message());
	}
}

void UMillicastPublisherComponent::SetSimulcast(webrtc::RtpTransceiverInit& TransceiverInit)
{
	webrtc::RtpEncodingParameters params;
	params.active = true;
	params.max_bitrate_bps = Bitrates->max_bitrate_bps.value_or(4'000'000);
	params.rid = "h";
	TransceiverInit.send_encodings.push_back(params);

	params.max_bitrate_bps = Bitrates->max_bitrate_bps.value_or(4'000'000) / 2;
	params.active = true;
	params.rid = "m";
	params.scale_resolution_down_by = 2;
	TransceiverInit.send_encodings.push_back(params);

	params.max_bitrate_bps = Bitrates->max_bitrate_bps.value_or(4'000'000) / 4;
	params.active = true;
	params.rid = "l";
	params.scale_resolution_down_by = 4;
	TransceiverInit.send_encodings.push_back(params);
}

void UMillicastPublisherComponent::CaptureAndAddTracks()
{
	// Starts audio and video capture
	MillicastMediaSource->StartCapture([this](auto&& Track) {
		// Add transceiver with sendonly direction
		webrtc::RtpTransceiverInit init;
		init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
		init.stream_ids = { "unrealstream" };

		if (MillicastMediaSource->Simulcast)
		{
			SetSimulcast(init);
		}

		auto result = (*PeerConnection)->AddTransceiver(Track, init);

		if (result.ok())
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Add transceiver for %s track : %s"), 
				Track->kind().c_str(), Track->id().c_str());

			auto transceiver = result.value();
			SetCodecPreference<decltype(transceiver), cricket::MediaType::MEDIA_TYPE_VIDEO>(transceiver);
			SetCodecPreference<decltype(transceiver), cricket::MediaType::MEDIA_TYPE_AUDIO>(transceiver);
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Couldn't add transceiver for %s track %s : %s"), 
				Track->kind().c_str(),
				Track->id().c_str(),
				result.error().message());
		}
	});
}

void UMillicastPublisherComponent::SetBitrates(int InStartKbps, int InMinKbps, int InMaxKbps)
{
	Bitrates->max_bitrate_bps = InMaxKbps * 1000;
	Bitrates->min_bitrate_bps = InMinKbps * 1000;
	Bitrates->current_bitrate_bps = InStartKbps * 1000;
}