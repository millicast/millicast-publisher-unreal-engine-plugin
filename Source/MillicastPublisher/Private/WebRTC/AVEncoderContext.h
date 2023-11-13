// Copyright Dolby.io 2023. All Rights Reserved.

#pragma once

#include "MillicastTypes.h"

namespace Millicast::Publisher
{
	struct FCapturedInput
	{
		FCapturedInput() = default;
		FCapturedInput(FVideoEncoderInputFrameType InFrame, FTexture2DRHIRef InTexture)
			: InputFrame(InFrame)
			, Texture(InTexture)
		{}

		FVideoEncoderInputFrameType InputFrame = nullptr;
		TOptional<FTexture2DRHIRef> Texture;
	};

	// Final because the class has no virtual destructor
	class FAVEncoderContext final
	{
	public:
		FAVEncoderContext(int32 InCaptureWidth, int32 InCaptureHeight, bool bInFixedResolution);

		int32 GetCaptureWidth() const { return CaptureWidth; }
		int32 GetCaptureHeight() const { return CaptureHeight; }
		bool IsFixedResolution() const;
		void SetCaptureResolution(int NewCaptureWidth, int NewCaptureHeight);

		FCapturedInput ObtainCapturedInput();
		TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const { return VideoEncoderInput; }

	private:
		TSharedPtr<AVEncoder::FVideoEncoderInput> CreateVideoEncoderInput(int InWidth, int InHeight, bool bInFixedResolution);
		void DeleteBackBuffers();

#if PLATFORM_WINDOWS
		FTexture2DRHIRef SetBackbufferTextureDX11(FVideoEncoderInputFrameType InputFrame);
		FTexture2DRHIRef SetBackbufferTextureDX12(FVideoEncoderInputFrameType InputFrame);
#endif // PLATFORM_WINDOWS

		FTexture2DRHIRef SetBackbufferTexturePureVulkan(FVideoEncoderInputFrameType InputFrame);
		FTexture2DRHIRef SetBackbufferTextureCUDAVulkan(FVideoEncoderInputFrameType InputFrame);

	private:
		int32 CaptureWidth = 0;
		int32 CaptureHeight = 0;
		bool bFixedResolution = false;
		
		TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;
		TMap<AVEncoder::FVideoEncoderInputFrame*, FTexture2DRHIRef> BackBuffers;
	};
}
