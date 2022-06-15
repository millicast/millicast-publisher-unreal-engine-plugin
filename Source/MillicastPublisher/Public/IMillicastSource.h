// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "api/media_stream_interface.h"

/** Interface to start a capture a write data to WebRTC buffers in order to publish audio/video to Millicast */
class IMillicastSource
{
public:
	/** 
	* The track name. This is optional but you can set a specific name for the WebRTC track 
	* The default name depend on the capturer.
	* If you log the SDP, you will see the track with the name you set.
	*/
	TOptional<FString> TrackId;

public:
	using FStreamTrackInterface = rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>;

	/**
	* Starts a capture according to the specific capturer and creates the corresponding WebRTC video source and video track.
	* Returns the WebRTC video track so you can add it to the SDP to establish the peerconnection with Millicast.
	*/
	virtual FStreamTrackInterface StartCapture() = 0;

	/** Stops the capture. The WebRTC Video Source and track will be set to null. */
	virtual void StopCapture() = 0;

	/** Get the WebRTC Video tracks. This will be null if the capture is not started. */
	virtual FStreamTrackInterface GetTrack() = 0;

	virtual ~IMillicastSource() = default;
};

/**
* Specialized interface for video sources. A video source can be : 
* a SlateWindow capture (basically a screenshare of the game)
* Read data from a RenderTarget. This allow to capture a scene from a virtual camera.
* TODO: maybe add webcam capture
*/
class IMillicastVideoSource : public IMillicastSource
{	
public:
	using FVideoTrackInterface = rtc::scoped_refptr<webrtc::VideoTrackInterface>;

	/** Creates VideoSource with SlateWindow Capture */
	static IMillicastVideoSource* Create();
	/** Creates VideoSource and capture from a RenderTarget */
	static IMillicastVideoSource* Create(UTextureRenderTarget2D* RenderTarget);
};

UENUM(BlueprintType)
enum AudioCapturerType
{
	SUBMIX   UMETA(DisplayName = "Submix"),
	DEVICE   UMETA(DisplayName = "Device"),
	LOOPBACK UMETA(DisplayName = "Loopback (windows only)"),
};

/**
* Specialized interface for audio source.
* Basically, the audio source is reading audio data from the main audio device 
* and publish these data to Millicast.
* TODO: See if there is a way to attach a specific audio source, like RenderTarget for video
* TODO: See if we can use mic or other input devices using webrtc api.
*/
class IMillicastAudioSource : public IMillicastSource
{
public:
	using FAudioTrackInterface = rtc::scoped_refptr<webrtc::AudioTrackInterface>;

	/** Create audio source to capture audio from the main audio device */
	static IMillicastAudioSource* Create(AudioCapturerType CapturerType);
};