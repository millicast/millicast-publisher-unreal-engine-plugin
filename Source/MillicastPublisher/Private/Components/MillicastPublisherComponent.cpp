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

#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"

#define WEAK_CAPTURE WeakThis = TWeakObjectPtr<UMillicastPublisherComponent>(this)

constexpr auto HTTP_OK = 200;

FString ToString(EMillicastCodec Codec)
{
	switch (Codec)
	{
		default:
		case EMillicastCodec::MC_VP8:
			return TEXT("vp8");
		case EMillicastCodec::MC_VP9:
			return TEXT("vp9");
		case EMillicastCodec::MC_H264:
			return TEXT("h264");
	}
}

UMillicastPublisherComponent::UMillicastPublisherComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	MillicastMediaSource = nullptr;
	PeerConnection = nullptr;
	WS = nullptr;
	bIsPublishing = false;

	// Event received from websocket signaling
	EventBroadcaster.Emplace("active", [this](TSharedPtr<FJsonObject> Msg) { ParseActiveEvent(Msg); });
	EventBroadcaster.Emplace("inactive", [this](TSharedPtr<FJsonObject> Msg) { ParseInactiveEvent(Msg); });
	EventBroadcaster.Emplace("viewercount", [this](TSharedPtr<FJsonObject> Msg) { ParseViewerCountEvent(Msg); });

	PeerConnectionConfig = FWebRTCPeerConnection::GetDefaultConfig();

	MaximumBitrate = 4'000'000;
	StartingBitrate = 2'000'000;
	MinimumBitrate = 1'000'000;
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

void UMillicastPublisherComponent::ParseActiveEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	OnActive.Broadcast();
}

void UMillicastPublisherComponent::ParseInactiveEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	OnInactive.Broadcast();
}

void UMillicastPublisherComponent::ParseViewerCountEvent(TSharedPtr<FJsonObject> JsonMsg)
{
	auto DataJson = JsonMsg->GetObjectField("data");

	int Count = DataJson->GetIntegerField("viewercount");

	OnViewerCount.Broadcast(Count);
}

void UMillicastPublisherComponent::ParseDirectorResponse(FHttpResponsePtr Response)
{
	FString ResponseDataString = Response->GetContentAsString();
	UE_LOG(LogMillicastPublisher, Log, TEXT("Director response : \n %s \n"), *ResponseDataString);

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
	FScopeLock Lock(&CriticalSection);

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
	PeerConnection = FWebRTCPeerConnection::Create(PeerConnectionConfig);

	// Starts the capture first and add track to the peerconnection
	// TODO: add a boolean to let choose autoplay or not
	CaptureAndAddTracks();

	// Get session description observers
	auto * CreateSessionDescriptionObserver = PeerConnection->GetCreateDescriptionObserver();
	auto * LocalDescriptionObserver  = PeerConnection->GetLocalDescriptionObserver();
	auto * RemoteDescriptionObserver = PeerConnection->GetRemoteDescriptionObserver();

	CreateSessionDescriptionObserver->SetOnSuccessCallback([WEAK_CAPTURE](const std::string& type, const std::string& sdp) {
		if (!WeakThis.IsValid())
		{
			return;
		}

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
		{
			FScopeLock Lock(&WeakThis->CriticalSection);
			WeakThis->PeerConnection->SetLocalDescription(sdp_non_const, type);
		}
	});

	CreateSessionDescriptionObserver->SetOnFailureCallback([WEAK_CAPTURE](const std::string& err) {
		if (WeakThis.IsValid())
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("pc.createOffer() | Error: %S"), err.c_str());
			WeakThis->OnPublishingError.Broadcast(TEXT("Could not create offer"));
		}
	});

	LocalDescriptionObserver->SetOnSuccessCallback([WEAK_CAPTURE]() {
		if (!WeakThis.IsValid())
		{
			return;
		}

		FScopeLock Lock(&WeakThis->CriticalSection);

		UE_LOG(LogMillicastPublisher, Log, TEXT("pc.setLocalDescription() | sucess"));

		if (!WeakThis->WS || !WeakThis->WS.IsValid() || !WeakThis->PeerConnection)
		{
			UE_LOG(LogMillicastPublisher, Warning, TEXT("WebSocket is closed, can not send SDP"));
			WeakThis->OnPublishingError.Broadcast(TEXT("Websocket is closed. Can not send SDP to server."));

			return;
		}
		 
		std::string sdp;
		(*WeakThis->PeerConnection)->local_description()->ToString(&sdp);

		// Add events we want to receive from millicast
		TArray<TSharedPtr<FJsonValue>> eventsJson;
		TArray<FString> EvKeys;
		WeakThis->EventBroadcaster.GetKeys(EvKeys);

		for (auto& ev : EvKeys) 
		{
			eventsJson.Add(MakeShared<FJsonValueString>(ev));
		}

		// Fill signaling data
		auto DataJson = MakeShared<FJsonObject>();
		DataJson->SetStringField("name", WeakThis->MillicastMediaSource->StreamName);
		DataJson->SetStringField("sdp", ToString(sdp));
		DataJson->SetStringField("codec", ToString(WeakThis->SelectedCodec));
		DataJson->SetArrayField("events", eventsJson);

		// If multisource feature
		if (!WeakThis->MillicastMediaSource->SourceId.IsEmpty())
		{
			DataJson->SetStringField("sourceId", WeakThis->MillicastMediaSource->SourceId);
		}

		auto Payload = MakeShared<FJsonObject>();
		Payload->SetStringField("type", "cmd");
		Payload->SetNumberField("transId", std::rand());
		Payload->SetStringField("name", "publish");
		Payload->SetObjectField("data", DataJson);

		FString StringStream;
		auto Writer = TJsonWriterFactory<>::Create(&StringStream);
		FJsonSerializer::Serialize(Payload, Writer);

		WeakThis->WS->Send(StringStream);

		UE_LOG(LogMillicastPublisher, Log, TEXT("WebSocket publish payload : %s"), *StringStream);
	});

	LocalDescriptionObserver->SetOnFailureCallback([WEAK_CAPTURE](const std::string& err) {
		if (WeakThis.IsValid())
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Set local description failed : %s"), *ToString(err));
			WeakThis->OnPublishingError.Broadcast(TEXT("Could not set local description"));
		}
	});

	RemoteDescriptionObserver->SetOnSuccessCallback([WEAK_CAPTURE]() {
		if (WeakThis.IsValid())
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Set remote description suceeded"));

			WeakThis->bIsPublishing = true;
			WeakThis->OnPublishing.Broadcast();
		}
	});
	RemoteDescriptionObserver->SetOnFailureCallback([WEAK_CAPTURE](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set remote description failed : %s"), *ToString(err));
		if (WeakThis.IsValid())
		{
			WeakThis->OnPublishingError.Broadcast(TEXT("Could not set remote description"));
		}
	});

	// Send only
	PeerConnection->OaOptions.offer_to_receive_video = false;
	PeerConnection->OaOptions.offer_to_receive_audio = false;

	// Bitrate settings
	webrtc::PeerConnectionInterface::BitrateParameters bitrateParameters;
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
		bitrateParameters.current_bitrate_bps = *StartingBitrate;
	}
	(*PeerConnection)->SetBitrate(bitrateParameters);

	// Create offer
	UE_LOG(LogMillicastPublisher, Log, TEXT("Create offer"));
	PeerConnection->CreateOffer();
	PeerConnection->EnableStats(RtcStatsEnabled);

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
		FString ServerId = DataJson->GetStringField("publisherId");
		FString ClusterId = DataJson->GetStringField("clusterId");

		FScopeLock Lock(&CriticalSection);
		if (PeerConnection) 
		{
			PeerConnection->SetRemoteDescription(to_string(Sdp));
			PeerConnection->ServerId = MoveTemp(ServerId);
			PeerConnection->ClusterId = MoveTemp(ClusterId);
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

		if (EventBroadcaster.Contains(eventName))
		{
			EventBroadcaster[eventName](ResponseJson);
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
	MillicastMediaSource->StartCapture([this](auto&& Track) {
		// Add transceiver with sendonly direction
		webrtc::RtpTransceiverInit init;
		init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
		init.stream_ids = { "unrealstream" };

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

		if (MillicastMediaSource->Simulcast)
		{
			SetSimulcast(init);
		}

		auto result = (*PeerConnection)->AddTransceiver(Track, init);

		if (result.ok())
		{
			UE_LOG(LogMillicastPublisher, Log, TEXT("Add transceiver for %s track : %s"), 
				Track->kind().c_str(), Track->id().c_str());
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

void UMillicastPublisherComponent::UpdateBitrateSettings()
{
	if (PeerConnection)
	{
		webrtc::PeerConnectionInterface::BitrateParameters BitrateParameters;

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
			BitrateParameters.current_bitrate_bps = *StartingBitrate;
		}

		auto error = (*PeerConnection)->SetBitrate(BitrateParameters);

		if (!error.ok())
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Could not set maximum bitrate: %S"), error.message());
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

void UMillicastPublisherComponent::EnableStats(bool Enable)
{
	RtcStatsEnabled = Enable;
	if (PeerConnection)
	{
		PeerConnection->EnableStats(Enable);
	}
}

