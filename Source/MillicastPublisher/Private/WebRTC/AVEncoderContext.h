// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "VideoEncoderInput.h"

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
#define FVideoEncoderInputFrameType AVEncoder::FVideoEncoderInputFrame*
#define InputFrameRef InputFrame
#else
#define FVideoEncoderInputFrameType TSharedPtr<AVEncoder::FVideoEncoderInputFrame>
#define InputFrameRef InputFrame.Get()
#endif

namespace Millicast::Publisher
{
	class FAVEncoderContext
	{
	public:
		struct FCapturerInput
		{
			FCapturerInput() = default;
			FCapturerInput(FVideoEncoderInputFrameType InFrame, FTexture2DRHIRef InTexture)
				: InputFrame(InFrame)
				, Texture(InTexture)
			{
			}

			FVideoEncoderInputFrameType InputFrame = nullptr;
			TOptional<FTexture2DRHIRef> Texture;
		};

	public:
		FAVEncoderContext(int InCaptureWidth, int InCaptureHeight, bool bInFixedResolution);

		int GetCaptureWidth() const;
		int GetCaptureHeight() const;
		bool IsFixedResolution() const;
		void SetCaptureResolution(int NewCaptureWidth, int NewCaptureHeight);

		FCapturerInput ObtainCapturerInput();
		TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const;

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
		int CaptureWidth;
		int CaptureHeight;
		bool bFixedResolution;
		TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput = nullptr;
		TMap<AVEncoder::FVideoEncoderInputFrame*, FTexture2DRHIRef> BackBuffers;
	};

}