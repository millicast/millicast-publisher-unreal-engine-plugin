// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "RHI.h"

namespace libyuv {
	extern "C" {
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

public:

	explicit FTexture2DFrameBuffer(FTexture2DRHIRef SourceTexture) noexcept
	{
		Width = SourceTexture->GetSizeX();
		Height = SourceTexture->GetSizeY();

		Buffer = webrtc::I420Buffer::Create(Width, Height);

		FRHICommandListImmediate& RHICommandList = FRHICommandListExecutor::GetImmediateCommandList();

		FIntRect Rect(0, 0, Width, Height);
		TArray<FColor> ColorData;
		uint8* TextureData = new uint8[Width * Height * 4];
			
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

		libyuv::ARGBToI420(TextureData, Width * 4,
			DataY, Buffer->StrideY(), DataU, Buffer->StrideU(), DataV, Buffer->StrideV(),
			Width, Height);

		delete[] TextureData;
	}
	
	int width() const override { return Width; }
	int height() const override { return Height; }
	Type type() const override { return Type::kNative; }

	rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
	{
		return Buffer;
	}
};