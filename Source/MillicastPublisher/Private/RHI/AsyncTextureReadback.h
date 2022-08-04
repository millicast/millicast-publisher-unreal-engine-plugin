// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"
#include "Templates/SharedPointer.h"
#include "Rendering/SlateRenderer.h"


/*
* A mechanism to read a UE texture from GPU to CPU
* without blocking the render thread.
* Note: This method can be improved in future version of UE when DX12 and Vulkan fence/triggers are in the RHI properly.
*/
class FAsyncTextureReadback : public TSharedFromThis<FAsyncTextureReadback>
{
public:
	FAsyncTextureReadback() = default;
	virtual ~FAsyncTextureReadback();

	void ReadbackAsync_RenderThread(FTexture2DRHIRef TextureToReadback, TFunction<void(uint8* /*Pixels*/, int /*Width*/, int /*Height*/, int /*Stride*/)> OnReadbackComplete);

private:
	void Initialize(FTexture2DRHIRef Texture);

private:
	int Width = 0;
	int Height = 0;
	FTexture2DRHIRef StagingTexture;
	FTexture2DRHIRef ReadbackTexture;
	int32 MappedStride = 0;
	void* ReadbackBuffer = nullptr;

};
