// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "MillicastPublisherSource.h"
#include "RtcCodecsConstants.h"
#include "MillicastPublisherComponent.generated.h"

// Forward declarations
class IWebSocket;
class IHttpResponse;

namespace webrtc
{
	struct RtpTransceiverInit;
}

namespace Millicast::Publisher
{
	class FWebRTCPeerConnection;
}

enum class EMillicastPublisherState : uint8
{
	Disconnected,
	Connecting,
	Connected
};

// Event declaration
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentAuthenticated, UMillicastPublisherComponent, OnAuthenticated);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FMillicastPublisherComponentAuthenticationFailure, UMillicastPublisherComponent, OnAuthenticationFailure, int, Code, const FString&, Msg);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentPublishing, UMillicastPublisherComponent, OnPublishing);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FMillicastPublisherComponentPublishingError, UMillicastPublisherComponent, OnPublishingError, const FString&, ErrorMsg);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentActive, UMillicastPublisherComponent, OnActive);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FMillicastPublisherComponentInactive, UMillicastPublisherComponent, OnInactive);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FMillicastPublisherComponentViewerCount, UMillicastPublisherComponent, OnViewerCount, int, Count);


/**
	A component used to publish audio, video feed to millicast.
*/
UCLASS(BlueprintType, Blueprintable, Category = "MillicastPublisher",
	   META = (DisplayName = "Millicast Publisher Component", BlueprintSpawnableComponent))
class MILLICASTPUBLISHER_API UMillicastPublisherComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

private:
	TMap <FString, TFunction<void(TSharedPtr<FJsonObject>)>> EventBroadcaster;

	/** The Millicast Media Source representing the configuration of the network source */
	UPROPERTY(EditDefaultsOnly, Category = "Properties",
			  META = (DisplayName = "Millicast Publisher Source", AllowPrivateAccess = true))
	UMillicastPublisherSource* MillicastMediaSource = nullptr;

	/** The video codec to be used to encode video */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", META = (DisplayName = "Video Codec"))
	EMillicastVideoCodecs SelectedVideoCodec;

	/** The video codec to be used to encode audio */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", META = (DisplayName = "Audio Codec"))
	EMillicastAudioCodecs SelectedAudioCodec;

	/** Whether to enable simulcast */
	UPROPERTY(EditDefaultsOnly, Category = "Properties", META = (DisplayName = "Simulcast"))
	bool Simulcast = false;

	/** Whether you want to automute the tracks when the number of viewer reach 0 
	* And unmute them when there are viewer watching the stream.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Properties", META = (DisplayName = "Automute"))
	bool Automute = false;
	
public:
	/**
		Initialize this component with the media source required for publishing  audio, video to Millicast.
		Returns false, if the MediaSource is already been set. This is usually the case when this component is
		initialized in Blueprints.
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Initialize"))
	bool Initialize(UMillicastPublisherSource* InMediaSource = nullptr);

	/**
		Begin publishing audio/video to Millicast using the info in the Publisher source object
		and calling the Publisher api
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Publish"))
	bool Publish();

	/**
		Begin publishing audio/video to Millicast using the websocket URL and JSON Web Token
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "PublishWithWsAndJwt"))
	bool PublishWithWsAndJwt(const FString& WebSocketUrl, const FString& Jwt);

	/**
		Attempts to stop publishing video from the Millicast feed
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "Unpublish"))
	void UnPublish();

	/**
	* Tells whether the compenent is publishing or not. true means it is publishing.
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "IsPublishing"))
	bool IsPublishing() const;

	/**
	* Set the minimum bitrate for the peerconnection
	* Have to be called before Publish
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetMinimumBitrate"))
	void SetMinimumBitrate(int Bps);

	/**
	* Set the maximum bitrate for the peerconnection
	* Have to be called before Publish
  */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetMaximumBitrate"))
	void SetMaximumBitrate(int Bps);

	/**
	* Set the starting bitrate for the peerconnection
	* Have to be called before Publish
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetStartingBitrate"))
	void SetStartingBitrate(int Bps);

	/**
	 * Set the video codec, must be called before Publish
	 * Return true if the video codec is set successfully, false if it is not set
	 */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetVideoCodec"))
	bool SetVideoCodec(EMillicastVideoCodecs InVideoCodec);

	/**
	 * Set the audio codec
	 * Return true if the video codec is set successfully, false if it is not set
	 */
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "SetAudioCodec"))
	bool SetAudioCodec(EMillicastAudioCodecs InAudioCodec);

	/**
	* Enable RTC stats gathering
	* Enter the cmd: ``stat millicast_publisher`` in order to display them
	*/
	UFUNCTION(BlueprintCallable, Category = "MillicastPublisher", META = (DisplayName = "EnableStat"))
	void EnableStats(bool Enable);

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif

public:
	/** Called when the response from the Publisher api is successfull */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentAuthenticated OnAuthenticated;

	/** Called when the response from the Publisher api is an error */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentAuthenticationFailure OnAuthenticationFailure;

	/** Called when the publisher is publishing to Millicast */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentPublishing OnPublishing;

	/** Called when the publisher is failed to publish to Millicast */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentPublishingError OnPublishingError;

	/** Called when the first viewer starts viewing the stream being published by this publisher */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentActive OnActive;

	/** Called when the last viewer quit viewing the stream being published by this publisher */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentInactive OnInactive;

	/** Called when the number of viewer watching the stream is updated */
	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FMillicastPublisherComponentViewerCount OnViewerCount;

private:
	void EndPlay(EEndPlayReason::Type Reason) override;

	/** Websocket callback */
	bool StartWebSocketConnection(const FString& url, const FString& jwt);
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Msg);

	/** Media Tracks */
	void CaptureAndAddTracks();

	/** Create the peerconnection and starts subscribing*/
	bool PublishToMillicast();

	void ParseDirectorResponse(TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response);
	void SetupIceServersFromJson(TArray<TSharedPtr<FJsonValue>> IceServersField);

	void ParseActiveEvent(TSharedPtr<FJsonObject> JsonMsg);
	void ParseInactiveEvent(TSharedPtr<FJsonObject> JsonMsg);
	void ParseViewerCountEvent(TSharedPtr<FJsonObject> JsonMsg);

	void SetSimulcast(webrtc::RtpTransceiverInit& TransceiverInit);

	void UpdateBitrateSettings();

	bool IsConnectionActive() const;

private:
	/** WebSocket Connection */
	TSharedPtr<IWebSocket> WS;
	FDelegateHandle OnConnectedHandle;
	FDelegateHandle OnConnectionErrorHandle;
	FDelegateHandle OnClosedHandle;
	FDelegateHandle OnMessageHandle;

	/** WebRTC */
	Millicast::Publisher::FWebRTCPeerConnection* PeerConnection = nullptr;
	webrtc::PeerConnectionInterface::RTCConfiguration PeerConnectionConfig;

	/** Publisher */
	TAtomic<EMillicastPublisherState> State = EMillicastPublisherState::Disconnected;
	bool RtcStatsEnabled = false;
	TOptional<int> MinimumBitrate; // in bps
	TOptional<int> MaximumBitrate; // in bps
	TOptional<int> StartingBitrate; // in bps

	FCriticalSection CriticalSection;
};
