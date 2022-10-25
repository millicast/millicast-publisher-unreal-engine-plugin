// Copyright Millicast 2022. All Rights Reserved.

#include "Stats.h"
#include "WebRTC/PeerConnection.h"
#include "MillicastPublisherPrivate.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMillicastStats, Log, All);
DEFINE_LOG_CATEGORY(LogMillicastStats);

FPublisherStats FPublisherStats::Instance;

double CalcEMA(double PrevAvg, int NumSamples, double Value)
{
	const double Mult = 2.0 / (NumSamples + 1.0);
	const double Result = (Value - PrevAvg) * Mult + PrevAvg;
	return Result;
}

void FPublisherStats::TextureReadbackStart()
{
	TextureReadbackStartTime = FPlatformTime::Cycles64();
}

void FPublisherStats::TextureReadbackEnd()
{
	uint64 ThisTime = FPlatformTime::Cycles64();
	double SecondsDelta = FPlatformTime::ToSeconds64(ThisTime - TextureReadbackStartTime);
	TextureReadbacks = FPlatformMath::Min(Frames + 1, 60);
	TextureReadbackAvg = CalcEMA(TextureReadbackAvg, TextureReadbacks, SecondsDelta);
}

void FPublisherStats::FrameRendered()
{
	uint64 ThisTime = FPlatformTime::Cycles64();
	double SecondsDelta = FPlatformTime::ToSeconds64(ThisTime - LastFrameRendered);
	double FPS = 1.0 / SecondsDelta;
	Frames = FPlatformMath::Min(Frames + 1, 60);
	SubmitFPS = CalcEMA(SubmitFPS, Frames, FPS);
	LastFrameRendered = ThisTime;
}

void FPublisherStats::SetEncoderStats(double LatencyMs, double BitrateMbps, int QP)
{
	EncoderStatSamples = FPlatformMath::Min(EncoderStatSamples + 1, 60);
	EncoderLatencyMs = CalcEMA(EncoderLatencyMs, EncoderStatSamples, LatencyMs);
	EncoderBitrateMbps = CalcEMA(EncoderBitrateMbps, EncoderStatSamples, BitrateMbps);
	EncoderQP = CalcEMA(EncoderQP, EncoderStatSamples, QP);
}

void FPublisherStats::Tick(float DeltaTime)
{
	if (!GEngine)
	{
		return;
	}

	int MessageKey = 100;

	int i = 0;
	for (FRTCStatsCollector* Collector : StatsCollectors)
	{
		Collector->Poll();
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Total Encode Time = %.2f s"), Collector->TotalEncodeTime), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Avg Encode Time = %.2f ms"), Collector->AvgEncodeTime), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode FPS = %.0f"), Collector->EncodeFPS), true);
		GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Stats Collector %d"), i), true);

		//UE_LOG(LogMillicastPublisher, Log, TEXT("Total Encode Time = %.2f s"), Collector->TotalEncodeTime);
		//UE_LOG(LogMillicastPublisher, Log, TEXT("Avg Encode Time = %.2f ms"), Collector->AvgEncodeTime);
		//UE_LOG(LogMillicastPublisher, Log, TEXT("Encode FPS = %.0f"), Collector->EncodeFPS);

		++i;
	}

	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("SubmitFPS = %.2f s"), SubmitFPS), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("TextureReadTime = %.6f s"), TextureReadbackAvg), true);

	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode Latency = %.2f ms"), EncoderLatencyMs), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode Bitrate = %.2f Mbps"), EncoderBitrateMbps), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode QP = %.0f"), EncoderQP), true);

	//UE_LOG(LogMillicastPublisher, Log, TEXT("SubmitFPS = %.2f s"), SubmitFPS);
	//UE_LOG(LogMillicastPublisher, Log, TEXT("TextureReadTime = %.2f s"), TextureReadbackAvg);
}

void FPublisherStats::RegisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Add(Connection);
}

void FPublisherStats::UnregisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Remove(Connection);
}

/*
 * FRTCStatsCollector
 */

FRTCStatsCollector::FRTCStatsCollector(class FWebRTCPeerConnection* InPeerConnection)
	: PeerConnection(InPeerConnection)
{
	FPublisherStats::Get().RegisterStatsCollector(this);
}

FRTCStatsCollector::~FRTCStatsCollector()
{
	FPublisherStats::Get().UnregisterStatsCollector(this);
}

void FRTCStatsCollector::Poll()
{
	PeerConnection->PollStats();
}

void FRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
{
	double NewTotalEncodedFrames = 0;
	double NewTotalEncodeTime = 0;

	for (const webrtc::RTCStats& Stats : *Report)
	{
		const FString StatsType = FString(Stats.type());
		const FString StatsId = FString(Stats.id().c_str());

		//UE_LOG(LogMillicastStats, Log, TEXT("Type: %s Id: %s"), *StatsType, *StatsId);

		for (const webrtc::RTCStatsMemberInterface* StatMember : Stats.Members())
		{
			const FString MemberName = FString(StatMember->name());
			const FString MemberValue = FString(StatMember->ValueToString().c_str());

			//UE_LOG(LogMillicastStats, Log, TEXT("    %s = %s"), *MemberName, *MemberValue);

			if (StatsType == "outbound-rtp")
			{
				if (MemberName == "totalEncodeTime")
				{
					NewTotalEncodeTime = FCString::Atod(*MemberValue);
				}
				else if (MemberName == "framesEncoded")
				{
					NewTotalEncodedFrames = FCString::Atod(*MemberValue);
				}
				else if (MemberName == "framesPerSecond")
				{
					EncodeFPS = FCString::Atod(*MemberValue);
				}
			}
		}
	}

	const double EncodedFramesDelta = NewTotalEncodedFrames - TotalEncodedFrames;
	if (EncodedFramesDelta)
	{
		const double EncodeTimeDelta = (NewTotalEncodeTime - TotalEncodeTime) * 1000.0;
		AvgEncodeTime = EncodeTimeDelta / EncodedFramesDelta;
		TotalEncodedFrames = NewTotalEncodedFrames;
		TotalEncodeTime = NewTotalEncodeTime;
	}
}

void FRTCStatsCollector::AddRef() const
{
	FPlatformAtomics::InterlockedIncrement(&RefCount);
}

rtc::RefCountReleaseStatus FRTCStatsCollector::Release() const
{
	if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
	{
		return rtc::RefCountReleaseStatus::kDroppedLastRef;
	}

	return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}