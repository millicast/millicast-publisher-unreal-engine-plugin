// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "RHI.h"
#include "RHIGPUReadback.h"

namespace libyuv {
	extern "C" {
		/** libyuv header can't be included here, so just declare this function to convert the frames. */
		int ARGBToI420(const uint8_t* src_bgra,
			int src_stride_bgra,
			uint8_t* dst_y,
			int dst_stride_y,
			uint8_t* dst_u,
			int dst_stride_u,
			uint8_t* dst_v,
			int dst_stride_v,
			int width,
			int height);
	}
}

class FB8G8R8A8ToI420FrameBuffer : public webrtc::VideoFrameBuffer
{
public:
	explicit FB8G8R8A8ToI420FrameBuffer(uint8* B8G8R8A8Pixels, int InWidth, int InHeight, int InStride) noexcept
		: Width(InWidth)
		, Height(InHeight)
	{
		/* Create an I420 buffer */
		Buffer = webrtc::I420Buffer::Create(Width, Height);

		uint8* DataY = Buffer->MutableDataY();
		uint8* DataU = Buffer->MutableDataU();
		uint8* DataV = Buffer->MutableDataV();

		const auto STRIDES = InStride * 4;

		libyuv::ARGBToI420(
			B8G8R8A8Pixels,
			STRIDES,
			Buffer->MutableDataY(),
			Buffer->StrideY(),
			Buffer->MutableDataU(),
			Buffer->StrideU(),
			Buffer->MutableDataV(),
			Buffer->StrideV(),
			Buffer->width(),
			Buffer->height());
	}

	/** Get video frame width */
	int width() const override { return Width; }

	/** Get video frame height */
	int height() const override { return Height; }

	/** Get buffer type */
	Type type() const override { return Type::kNative; }

	/** Get the I420 buffer */
	rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
	{
		return Buffer;
	}

private:
	int Width;
	int Height;
	rtc::scoped_refptr<webrtc::I420Buffer> Buffer;
};