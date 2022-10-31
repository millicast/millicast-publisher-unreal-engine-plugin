// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "VideoEncoderInput.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
//#include "Stats.h"

namespace libyuv
{
	extern "C"
	{
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
} // namespace libyuv

class FFrameBufferRHI : public webrtc::VideoFrameBuffer
{
public:
	FFrameBufferRHI(FTexture2DRHIRef SourceTexture,
		AVEncoder::FVideoEncoderInputFrame* InputFrame,
		TSharedPtr<AVEncoder::FVideoEncoderInput> InputVideoEncoderInput)
		: TextureRef(SourceTexture)
		, Frame(InputFrame)
		, VideoEncoderInput(InputVideoEncoderInput)
	{
		Frame->Obtain();

		//FPublisherStats::Get().TextureReadbackStart();
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (GDynamicRHI && GDynamicRHI->GetName() == FString(TEXT("D3D12")))
		{
			ReadTextureDX12(RHICmdList);
		}
		else
		{
			ReadTexture(RHICmdList);
		}
		//FPublisherStats::Get().TextureReadbackEnd();
	}

	~FFrameBufferRHI()
	{
		Frame->Release();
	}

	Type type() const override
	{
		return Type::kNative;
	}

	virtual int width() const override
	{
		return Frame->GetWidth();
	}

	virtual int height() const override
	{
		return Frame->GetHeight();
	}

	virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
	{
		if (!Buffer)
		{
			int Width = TextureRef->GetSizeX();
			int Height = TextureRef->GetSizeY();

			Buffer = webrtc::I420Buffer::Create(Width, Height);
			if (TextureData)
			{
				libyuv::ARGBToI420(
					static_cast<uint8*>(TextureData),
					PitchPixels * 4,
					Buffer->MutableDataY(),
					Buffer->StrideY(),
					Buffer->MutableDataU(),
					Buffer->StrideU(),
					Buffer->MutableDataV(),
					Buffer->StrideV(),
					Buffer->width(),
					Buffer->height());
			}
		}

		return Buffer;
	}

	virtual const webrtc::I420BufferInterface* GetI420() const override
	{
		return Buffer;
	}

	FTexture2DRHIRef GetTextureRHI() const
	{
		return TextureRef;
	}

	AVEncoder::FVideoEncoderInputFrame* GetFrame() const
	{
		return Frame;
	}

	TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const
	{
		return VideoEncoderInput;
	}

private:
	FTexture2DRHIRef TextureRef;
	AVEncoder::FVideoEncoderInputFrame* Frame;
	TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;
	rtc::scoped_refptr<webrtc::I420Buffer> Buffer;
	TUniquePtr<FRHIGPUTextureReadback> Readback;
	void* TextureData = nullptr;
	int PitchPixels = 0;

	void ReadTextureDX12(FRHICommandListImmediate& RHICmdList)
	{
		if (!Readback.IsValid())
		{
			Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("CaptureReadback"));
		}

		Readback->EnqueueCopy(RHICmdList, TextureRef);
		RHICmdList.BlockUntilGPUIdle();
		check(Readback->IsReady());
		TextureData = Readback->Lock(PitchPixels);

		Readback->Unlock();
	}

	void ReadTexture(FRHICommandListImmediate& RHICmdList)
	{
		uint32 Stride;
		TextureData = (uint8*)RHICmdList.LockTexture2D(TextureRef, 0, EResourceLockMode::RLM_ReadOnly, Stride, true);

		PitchPixels = Stride / 4;

		RHICmdList.UnlockTexture2D(TextureRef, 0, true);
	}
};
