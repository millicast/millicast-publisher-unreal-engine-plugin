// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "Tickable.h"

class FRTCStatsCollector : public webrtc::RTCStatsCollectorCallback
{
public:
	FRTCStatsCollector(class FWebRTCPeerConnection* InPeerConnection);
	~FRTCStatsCollector();

	void Poll();

	// Begin RTCStatsCollectorCallback interface
	void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
	void AddRef() const override;
	rtc::RefCountReleaseStatus Release() const override;
	// End RTCStatsCollectorCallback interface

	double TotalEncodeTime;
	double TotalEncodedFrames;
	double AvgEncodeTime;
	double EncodeFPS;

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

public:
	static FPublisherStats& Get() { return Instance; }

	void Tick(float DeltaTime);
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(MillicastProducerStats, STATGROUP_Tickables); }

	void RegisterStatsCollector(FRTCStatsCollector* Collector);
	void UnregisterStatsCollector(FRTCStatsCollector* Collector);
};
