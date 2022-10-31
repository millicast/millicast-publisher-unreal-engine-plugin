// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "libyuv/convert.h"

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