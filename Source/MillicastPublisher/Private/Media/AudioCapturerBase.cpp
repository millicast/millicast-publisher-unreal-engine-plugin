// Copyright Millicast 2022. All Rights Reserved.
#include "AudioCapturerBase.h"
#include "AudioSubmixCapturer.h"
#include "AudioDeviceCapturer.h"

#if PLATFORM_WINDOWS
#include "WasapiDeviceCapturer.h"
#endif

#include "MillicastPublisherPrivate.h"
#include "WebRTC/PeerConnection.h"
#include "common_audio/include/audio_util.h"

#include "Util.h"

IMillicastAudioSource* IMillicastAudioSource::Create(AudioCapturerType CapturerType)
{
	switch (CapturerType)
	{
	case AudioCapturerType::SUBMIX: return new AudioSubmixCapturer;
	case AudioCapturerType::DEVICE: return new AudioDeviceCapturer;
#if PLATFORM_WINDOWS
	case AudioCapturerType::LOOPBACK: return new WasapiDeviceCapturer(10, true);
#else
	case AudioCapturerType::LOOPBACK: return nullptr;
#endif
	}

	return nullptr;
}

AudioCapturerBase::AudioCapturerBase() noexcept : NumChannels(2), RtcAudioTrack(nullptr)
{
	AddRef(); // Add ref so it is not released by libwebrtc (ref count) when destroying the track.

	// Number of samples for 10ms of audio data
	NumSamples = SamplePerSecond * NumChannels * TimePerFrameMs / 1000;
	// Number of bits for each sample
	BitPerSample = sizeof(FSample) * 8 * NumChannels;
}

void AudioCapturerBase::SendAudio(const float* InAudioData, int32 InNumSamples, int32 InNumChannels)
{
	// Mix if needed
	Audio::TSampleBuffer<float> Buffer(InAudioData, InNumSamples, InNumChannels, SamplePerSecond);
	if (Buffer.GetNumChannels() != NumChannels)
	{
		Buffer.MixBufferToChannels(NumChannels);
	}

	const auto currentNum = AudioBuffer.Num(); // Get the current size of the buffer
	AudioBuffer.AddZeroed(Buffer.GetNumSamples()); // Allocate the new number of samples

	// Convert it from float to signed 16 bit audio data and add it at the end of the audio buffer
	webrtc::FloatToS16(Buffer.GetData(), Buffer.GetNumSamples(), AudioBuffer.GetData() + currentNum);

	// Send audio to webrtc
	SendAudio();
}

void AudioCapturerBase::SendAudio(const int16* InAudioData, int32 InNumSamples, int32 InNumChannels)
{
	// Mix if needed
	Audio::TSampleBuffer<int16> Buffer(InAudioData, InNumSamples, InNumChannels, SamplePerSecond);
	if (Buffer.GetNumChannels() != NumChannels)
	{
		Buffer.MixBufferToChannels(NumChannels);
	}

	const auto currentNum = AudioBuffer.Num(); // Get the current size of the buffer
	AudioBuffer.Append(Buffer.GetData(), Buffer.GetNumSamples()); // Append data to the end of the buffer

	// Send audio frame to webrtc
	SendAudio();
}

void AudioCapturerBase::SendAudio()
{
	// while there is enough samples in the buffer for a whole audio frame
	while (AudioBuffer.Num() >= NumSamples)
	{
		for (auto Sink : Sinks) // call all the audio sinks
		{
			// Divide by the number of channels because it is included in the bit per sample
			const auto NewNumSample = NumSamples / NumChannels;
			// Call on data
			Sink->OnData(AudioBuffer.GetData(), BitPerSample, SamplePerSecond, NumChannels, NewNumSample);
		}
		// Remove the audio data we just sent to the audio sinks
		AudioBuffer.RemoveAt(0, NumSamples);
	}
}

void AudioCapturerBase::CreateRtcSourceTrack()
{
	// Get PCF to create audio source and audio track
	auto peerConnectionFactory = FWebRTCPeerConnection::GetPeerConnectionFactory();

	// Create audio track
	RtcAudioTrack  = peerConnectionFactory->CreateAudioTrack(to_string(TrackId.Get("audio")), this);
}

IMillicastSource::FStreamTrackInterface AudioCapturerBase::GetTrack()
{
	return RtcAudioTrack;
}

void AudioCapturerBase::AddSink(webrtc::AudioTrackSinkInterface* Sink)
{
	Sinks.Emplace(Sink);
}

void AudioCapturerBase::RemoveSink(webrtc::AudioTrackSinkInterface* Sink)
{
	Sinks.Remove(Sink);
}

void AudioCapturerBase::SetNumChannel(uint8 InNumChannel)
{
	NumChannels = InNumChannel;

	NumSamples = NumChannels * SamplePerSecond * TimePerFrameMs / 1000;
	BitPerSample = NumChannels * sizeof(FSample) * 8;
}
