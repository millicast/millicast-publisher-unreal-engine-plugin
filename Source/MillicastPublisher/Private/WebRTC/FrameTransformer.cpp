#include "FrameTransformer.h"

#include "PeerConnection.h"

namespace Millicast::Publisher
{

void FFrameTransformer::Transform(FTransformableFrame TransformableFrame)
{
	auto ssrc = TransformableFrame->GetSsrc();

	if (auto it = Callbacks.find(ssrc); it != Callbacks.end())
	{
		// clear previous data but keep capacity to avoid dynamic reallocation
		TransformedData.Empty();
		UserData.Empty();

		// get user data
		if (PeerConnection->OnTransformableFrame)
		{
			PeerConnection->OnTransformableFrame(ssrc, TransformableFrame->GetTimestamp(), UserData);
		}

		auto data_view = TransformableFrame->GetData();

		// Copy the frame data because data_view is non mutable
		// std::copy(data_view.begin(), data_view.end(), std::back_inserter(TransformedData));
		TransformedData.Append(data_view.data(), data_view.size());
		// Add the user data at the end of the frame data
		// std::copy(UserData.begin(), UserData.end(), std::back_inserter(TransformedData));
		TransformedData.Append(UserData);

		// Set the length of the user data in the last 4 bytes to help retrieving it on the viewer side
		uint32_t length = static_cast<uint32_t>(UserData.Num());
		TransformedData.Add((length >> 24) & 0xff);
		TransformedData.Add((length >> 16) & 0xff);
		TransformedData.Add((length >> 8) & 0xff);
		TransformedData.Add(length & 0xff);

		// Set the final transformed data.
		rtc::ArrayView<uint8_t> transformed_data_view(TransformedData.GetData(), TransformedData.Num());
		TransformableFrame->SetData(transformed_data_view);

		it->second->OnTransformedFrame(std::move(TransformableFrame));
	}
}

void FFrameTransformer::RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback)
{}

void FFrameTransformer::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> InCallback, uint32_t Ssrc)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Registering frame transformer callback for ssrc %d"), Ssrc);
	Callbacks[Ssrc] = InCallback;
}

void FFrameTransformer::UnregisterTransformedFrameCallback()
{}

void FFrameTransformer::UnregisterTransformedFrameSinkCallback(uint32_t ssrc)
{
	Callbacks.erase(ssrc);
}

}