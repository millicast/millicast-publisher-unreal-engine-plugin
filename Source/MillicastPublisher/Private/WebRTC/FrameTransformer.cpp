#include "FrameTransformer.h"

#include "PeerConnection.h"

namespace Millicast::Publisher
{

template<typename T>
void encode(TArray<uint8>& data, T value)
{
	constexpr auto size = sizeof(T);

	for (int i = 0; i < size; ++i)
	{
		auto decl = (size - i - 1) * 8;
		data.Add((value >> decl) & 0xff);
	}
}

using FMetadataHeader = uint32_t;
constexpr auto HEADER_TYPE_LENGTH = sizeof(FMetadataHeader);

// Magic value for the begining of the metadata
constexpr FMetadataHeader START_VALUE = 0xCAFEBABE;

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
		TransformedData.Append(data_view.data(), data_view.size());
		// Add magic value to signal the beginning of the user metadata
		encode(TransformedData, START_VALUE);
		// Add the user data at the end of the frame data
		TransformedData.Append(UserData);

		// Set the length of the user data in the last 4 bytes to help retrieving it on the viewer side
		auto length = static_cast<FMetadataHeader>(UserData.Num());
		encode(TransformedData, length);

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