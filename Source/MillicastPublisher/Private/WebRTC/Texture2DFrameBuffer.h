// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "RHI.h"

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

class FTexture2DFrameBuffer : public webrtc::VideoFrameBuffer
{
	int Width;
	int Height;

	rtc::scoped_refptr<webrtc::I420Buffer> Buffer;

	FTexture2DRHIRef TextureFrame;
	FCriticalSection CriticalSection;
	uint8* TextureData;

public:

	explicit FTexture2DFrameBuffer(FTexture2DRHIRef SourceTexture) noexcept
	{
		/* Get video farme height and  width */
		Width = SourceTexture->GetSizeX();
		Height = SourceTexture->GetSizeY();
		TextureFrame = SourceTexture;

		/* Create an I420 buffer */
		Buffer = webrtc::I420Buffer::Create(Width, Height);

		uint32 stride;
		TextureData = (uint8*)GDynamicRHI->RHILockTexture2D(TextureFrame, 0, EResourceLockMode::RLM_ReadOnly, stride, true);

		GDynamicRHI->RHIUnlockTexture2D(TextureFrame, 0, true);
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
		uint8* DataY = Buffer->MutableDataY();
		uint8* DataU = Buffer->MutableDataU();
		uint8* DataV = Buffer->MutableDataV();

		const auto STRIDES = Width * 4;
		libyuv::ARGBToI420(TextureData, STRIDES,
			DataY, Buffer->StrideY(), DataU, Buffer->StrideU(), DataV, Buffer->StrideV(),
			Width, Height);

		return Buffer;
	}
};

class FColorTexture2DFrameBuffer : public webrtc::VideoFrameBuffer
{
	int Width;
	int Height;

	rtc::scoped_refptr<webrtc::I420Buffer> Buffer;

public:

	explicit FColorTexture2DFrameBuffer(FTexture2DRHIRef SourceTexture) noexcept
	{
		/* Get video farme height and  width */
		Width = SourceTexture->GetSizeX();
		Height = SourceTexture->GetSizeY();

		/* Create an I420 buffer */
		Buffer = webrtc::I420Buffer::Create(Width, Height);


		/* Convert the texture2d frame to YUV pixel format */
		FRHICommandListImmediate& RHICommandList = FRHICommandListExecutor::GetImmediateCommandList();

		const auto ARGB_BUFFER_SIZE = Width * Height * 4;
		FIntRect Rect(0, 0, Width, Height);
		TArray<FColor> ColorData;
		uint8* TextureData = new uint8[ARGB_BUFFER_SIZE];

		FReadSurfaceDataFlags ReadSurfaceData{};
		ReadSurfaceData.SetMip(0);

		RHICommandList.ReadSurfaceData(SourceTexture, Rect, ColorData, ReadSurfaceData);

		for (uint64 i = 0; i < Width * Height; ++i) {
			const int64 ind = i * 4;
			TextureData[ind] = ColorData[i].B;
			TextureData[ind + 1] = ColorData[i].G;
			TextureData[ind + 2] = ColorData[i].R;
			TextureData[ind + 3] = ColorData[i].A;
		}

		uint8* DataY = Buffer->MutableDataY();
		uint8* DataU = Buffer->MutableDataU();
		uint8* DataV = Buffer->MutableDataV();

		/* The buffer is in BGRA, but for some reason, this is ARGGToI420 that we need to use */
		const auto STRIDES = Width * 4;
		libyuv::ARGBToI420(TextureData, STRIDES,
			DataY, Buffer->StrideY(), DataU, Buffer->StrideU(), DataV, Buffer->StrideV(),
			Width, Height);

		delete[] TextureData;
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
};