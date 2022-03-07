// Copyright Millicast 2022. All Rights Reserved.

#include "AudioGameCapturer.h"
#include "WebRTC/PeerConnection.h"

#include "Util.h"

IMillicastAudioSource* IMillicastAudioSource::Create()
{
	return new AudioGameCapturer;
}

AudioGameCapturer::AudioGameCapturer() noexcept :
	RtcAudioSource(nullptr), RtcAudioTrack(nullptr)
{
}

IMillicastSource::FStreamTrackInterface AudioGameCapturer::StartCapture()
{
	auto peerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	cricket::AudioOptions options;
	options.echo_cancellation.emplace(false);
	options.auto_gain_control.emplace(false);
	options.noise_suppression.emplace(false);
	options.highpass_filter.emplace(false);
	options.stereo_swapping.emplace(false);
	options.typing_detection.emplace(false);
	options.experimental_agc.emplace(false);
	options.experimental_ns.emplace(false);
	options.residual_echo_detector.emplace(false);

	options.audio_jitter_buffer_max_packets = 1000;
	options.audio_jitter_buffer_fast_accelerate = false;
	options.audio_jitter_buffer_min_delay_ms = 0;
	options.audio_jitter_buffer_enable_rtx_handling = false;

	RtcAudioSource = peerConnectionFactory->CreateAudioSource(options);
	RtcAudioTrack = peerConnectionFactory->CreateAudioTrack(to_string(TrackId.Get("audio")), RtcAudioSource);

	return RtcAudioTrack;
}

void AudioGameCapturer::StopCapture()
{
	RtcAudioTrack = nullptr;
	RtcAudioSource = nullptr;
}

IMillicastSource::FStreamTrackInterface AudioGameCapturer::GetTrack()
{
	return RtcAudioTrack;
}
