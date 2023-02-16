// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "IMillicastSource.h"

#include <pc/local_audio_source.h>

namespace Millicast::Publisher
{
	class AudioCapturerBase : public IMillicastAudioSource, rtc::RefCountedObject<webrtc::LocalAudioSource>
	{
		uint8 NumChannels = 2; // TODO: Make it configurable
		int32 NumSamples;
		uint16 BitPerSample;

	protected:
		using FSample = int16;

		static constexpr int TimePerFrameMs = 10; // 10 ms audio frame for webrtc
		static constexpr int SamplePerSecond = 48000;

		FStreamTrackInterface RtcAudioTrack = nullptr;

		TSet<webrtc::AudioTrackSinkInterface*> Sinks;
		TArray<FSample> AudioBuffer;

	protected:
		void SendAudio(const float* InAudioData, int32 InNumSamples, int32 InNumChannels);
		void SendAudio(const int16* InAudioData, int32 InNumSamples, int32 InNumChannels);
		void SendAudio();
		void CreateRtcSourceTrack();
	public:
		AudioCapturerBase() noexcept;
		virtual ~AudioCapturerBase() override = default;
		FStreamTrackInterface GetTrack() override;

		void AddSink(webrtc::AudioTrackSinkInterface* Sink) override;
		void RemoveSink(webrtc::AudioTrackSinkInterface* Sink) override;

		uint8 GetNumChannel() const { return NumChannels; }
		void SetNumChannel(uint8 InNumChannel);
	};
}
