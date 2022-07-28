// Copyright Millicast 2022. All Rights Reserved.

#include "AsyncTextureReadback.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "Runtime/Renderer/Private/ScreenPass.h"

FAsyncTextureReadback::~FAsyncTextureReadback()
{
	GDynamicRHI->RHIUnmapStagingSurface(ReadbackTexture);
	ReadbackBuffer = nullptr;
}

/*
* Copy from one texture to another. If PixelFormat and resolution is the same just do a simple GPU<->GPU copy.
* If resolution or PixelFormat are mismatched then we have to render the source texture into the destination texture.
*/
void CopyTexture(FRHICommandList& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture)
{
	if (SourceTexture->GetFormat() == DestTexture->GetFormat()
		&& SourceTexture->GetSizeX() == DestTexture->GetSizeX()
		&& SourceTexture->GetSizeY() == DestTexture->GetSizeY())
	{

		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));

		// source and dest are the same. simple copy
		RHICmdList.CopyTexture(SourceTexture, DestTexture, {});
	}
	else
	{
		IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");

		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

		// source and destination are different. rendered copy
		FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyTexture"));
		{
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

			RHICmdList.SetViewport(0, 0, 0.0f, DestTexture->GetSizeX(), DestTexture->GetSizeY(), 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceTexture);

			FIntPoint TargetBufferSize(DestTexture->GetSizeX(), DestTexture->GetSizeY());
			RendererModule->DrawRectangle(RHICmdList, 0, 0, // Dest X, Y
				DestTexture->GetSizeX(),					// Dest Width
				DestTexture->GetSizeY(),					// Dest Height
				0, 0,										// Source U, V
				1, 1,										// Source USize, VSize
				TargetBufferSize,							// Target buffer size
				FIntPoint(1, 1),							// Source texture size
				VertexShader, EDRF_Default);
		}

		RHICmdList.EndRenderPass();

		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::RTV, ERHIAccess::CopyDest));
	}
}

void FAsyncTextureReadback::ReadbackAsync_RenderThread(FTexture2DRHIRef SourceTexture, TFunction<void(uint8*,int,int,int)> OnReadbackComplete)
{
	checkf(IsInRenderingThread(), TEXT("Texture readback can only occur on the rendering thread."));
	Initialize(SourceTexture);

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Copy the passed texture into a staging texture (we do this to ensure PixelFormat and texture type is correct for readback)
	RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopySrc, ERHIAccess::CopyDest));
	CopyTexture(RHICmdList, SourceTexture, StagingTexture);

	// Copy the staging texture from GPU to CPU
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(ReadbackTexture, ERHIAccess::CPURead, ERHIAccess::CopyDest));
	RHICmdList.CopyTexture(StagingTexture, ReadbackTexture, {});
	RHICmdList.Transition(FRHITransitionInfo(ReadbackTexture, ERHIAccess::CopyDest, ERHIAccess::CPURead));

	TSharedRef<FAsyncTextureReadback> ThisRef = AsShared();
	RHICmdList.EnqueueLambda([ThisRef, OnReadbackComplete](FRHICommandListImmediate&) {
		uint8* Pixels = static_cast<uint8*>(ThisRef->ReadbackBuffer);
		OnReadbackComplete(Pixels, ThisRef->Width, ThisRef->Height, ThisRef->MappedStride);
	});
	
}

void FAsyncTextureReadback::Initialize(FTexture2DRHIRef Texture)
{
	int InWidth = Texture->GetSizeX();
	int InHeight = Texture->GetSizeY();
	if (InWidth == Width && InHeight == Height)
	{
		// No need to initialize, we already have textures at the correct resolutions.
		return;
	}

	Width = InWidth;
	Height = InHeight;

	// Create a staging texture with a resolution matching the texture passed in.
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TextureReadbackStagingTexture"));
		StagingTexture = GDynamicRHI->RHICreateTexture2D(Width, Height, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, ERHIAccess::CopySrc, CreateInfo);
	}

	// Create a texture mapped to CPU memory so we can do an easy readback
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("MappedCPUReadbackTexture"));
		ReadbackTexture = GDynamicRHI->RHICreateTexture2D(Width, Height, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_CPUReadback, ERHIAccess::CPURead, CreateInfo);

		int32 BufferWidth = 0, BufferHeight = 0;
		GDynamicRHI->RHIMapStagingSurface(ReadbackTexture, nullptr, ReadbackBuffer, BufferWidth, BufferHeight);
		MappedStride = BufferWidth;
	}
	
}