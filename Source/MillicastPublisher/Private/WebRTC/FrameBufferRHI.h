// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "MillicastTypes.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "Util.h"

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


#if !WITH_AVENCODER
	namespace AVEncoder
	{
		class FVideoEncoderInputFrame
		{
		public:
			void Obtain()const {}
			void Release()const {}
		};
		using FVideoEncoderInput = nullptr_t;
	}
#endif
namespace Millicast::Publisher
{
	class FFrameBufferRHI : public webrtc::VideoFrameBuffer
	{
	public:
		FFrameBufferRHI(FTexture2DRHIRef SourceTexture,
			FVideoEncoderInputFrameType InputFrame,
			TSharedPtr<AVEncoder::FVideoEncoderInput> InputVideoEncoderInput)
			: TextureRef(SourceTexture)
			, Frame(InputFrame)
			, VideoEncoderInput(InputVideoEncoderInput)
		{
			Frame->Obtain();

			//FPublisherStats::Get().TextureReadbackStart();
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			if (GDynamicRHI && (GDynamicRHI->GetName() == FString(TEXT("D3D12")) || GDynamicRHI->GetName() == FString(TEXT("Vulkan"))))
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

		int width() const override
		{
			return TextureRef->GetTexture2D()->GetSizeX();
		}

		int height() const override
		{
			return TextureRef->GetTexture2D()->GetSizeY();
		}

		virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
		{
			if (!Buffer)
			{
				const auto Width = TextureRef->GetSizeX();
				const auto Height = TextureRef->GetSizeY();

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
			return nullptr;
		}


		FTexture2DRHIRef GetTextureRHI() const
		{
			return TextureRef;
		}

#if !PLATFORM_ANDROID && !PLATFORM_IOS
		FVideoEncoderInputFrameType GetFrame() const
		{
			return Frame;
		}
#endif
		TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const
		{
			return VideoEncoderInput;
		}

	private:
		FTexture2DRHIRef TextureRef;
		FVideoEncoderInputFrameType Frame;
		TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;
		rtc::scoped_refptr<webrtc::I420Buffer> Buffer = nullptr;
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

#if ENGINE_MAJOR_VERSION < 5
			int Width = TextureRef->GetSizeX();
			PitchPixels = Width;
#endif

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


	class FSimulcastFrameBuffer : public webrtc::VideoFrameBuffer
	{
	public:
		void AddLayer(rtc::scoped_refptr<FFrameBufferRHI> Layer)
		{
			FScopeLock Lock(&CriticalSection);
			FrameBuffers.Add(Layer);
		}

		int32 GetNumLayers() const
		{
			FScopeLock Lock(&CriticalSection);
			return FrameBuffers.Num();
		}

		rtc::scoped_refptr<FFrameBufferRHI> GetLayer(int Id) 
		{
			FScopeLock Lock(&CriticalSection);
			return ((Id < FrameBuffers.Num()) ? FrameBuffers[Id] : nullptr);
		}

		Type type() const override
		{
			return Type::kNative;
		}

		int width() const override
		{
			FScopeLock Lock(&CriticalSection);

      		if( IsEmpty(FrameBuffers) )
			{
				return 0;
			}

			return FrameBuffers[0]->width();
		}

		int height() const override
		{
			FScopeLock Lock(&CriticalSection);
		    if( IsEmpty(FrameBuffers) )
			{
				return 0;
			}
			
			return FrameBuffers[0]->height();
		}

		rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
		{
			return nullptr;
		}
		
		const webrtc::I420BufferInterface* GetI420() const override
		{
			return nullptr;
		}

	private:
		TArray<rtc::scoped_refptr<FFrameBufferRHI>> FrameBuffers;
		mutable FCriticalSection CriticalSection;
	};
}
