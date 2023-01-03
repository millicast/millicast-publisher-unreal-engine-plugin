// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "WebRTC/PeerConnection.h"


namespace Millicast::Publisher
{
	class FRTCStatsCollector : public webrtc::RTCStatsCollectorCallback
	{
		double LastVideoStatTimestamp;
		double LastAudioStatTimestamp;

	public:
		FRTCStatsCollector(class FWebRTCPeerConnection* InPeerConnection);
		~FRTCStatsCollector();

		void Poll();

		// Begin RTCStatsCollectorCallback interface
		void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
		void AddRef() const override;
		rtc::RefCountReleaseStatus Release() const override;
		// End RTCStatsCollectorCallback interface

		double Rtt; // ms
		size_t Width; // px
		size_t Height; // px
		size_t FramePerSecond;
		double VideoBitrate; // bps
		double AudioBitrate; // bps
		double AudioTargetBitrate; // bps
		double VideoTargetBitrate; // bps
		size_t VideoTotalSent; // bytes
		size_t AudioTotalSent; // bytes
		int VideoPacketRetransmitted; // num packets
		int AudioPacketRetransmitted; // num packets
		FString VideoCodec; // mimetype
		FString AudioCodec; // mimetype
		int FramesDropped;
		int VideoNackCount;
		int AudioNackCount;

		double TotalEncodedFrames;
		double AvgEncodeTime;
		double TotalEncodeTime;

		TOptional<FString> QualityLimitationReason;
		uint32 QualityLimitationResolutionChange;
		TOptional<FString> ContentType;

		double Timestamp; // us

		const FString& Cluster() const { return PeerConnection->ClusterId; }
		const FString& Server() const { return PeerConnection->ServerId; }

	private:
		mutable int32 RefCount;
		FWebRTCPeerConnection* PeerConnection;
	};

	/*
	 * Some basic performance stats about how the publisher is running, e.g. how long capture/encode takes.
	 * Stats are drawn to screen for now as it is useful to observe them in realtime.
	 */
	class FPublisherStats : FTickableGameObject
	{
	public:
		TArray<FRTCStatsCollector*> StatsCollectors;

		void TextureReadbackStart();
		void TextureReadbackEnd();
		void FrameRendered();

		void SetEncoderStats(double LatencyMs, double BitrateMbps, int QP);

	private:
		// Intent is to access through FPublisherStats::Get()
		static FPublisherStats Instance;

		uint64 TextureReadbackStartTime = 0;
		int TextureReadbacks = 0;
		double TextureReadbackAvg = 0;

		uint64 LastFrameRendered = 0;
		int Frames = 0;
		double SubmitFPS = 0;

		int EncoderStatSamples = 0;
		double EncoderLatencyMs = 0;
		double EncoderBitrateMbps = 0;
		double EncoderQP = 0;

		bool bRegisterEngineStats = false;

		bool OnToggleStats(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream);
		int32 OnRenderStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation);
		void RegisterEngineHooks();

	public:
		static FPublisherStats& Get() { return Instance; }

		void Tick(float DeltaTime);
		FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(MillicastProducerStats, STATGROUP_Tickables); }

		void RegisterStatsCollector(FRTCStatsCollector* Collector);
		void UnregisterStatsCollector(FRTCStatsCollector* Collector);
	};

}