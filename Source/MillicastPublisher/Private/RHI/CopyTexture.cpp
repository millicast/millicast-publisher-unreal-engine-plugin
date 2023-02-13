// Copyright Millicast 2022. All Rights Reserved.

#include "CopyTexture.h"

#include "CommonRenderResources.h"
#include "ScreenRendering.h"

void MillicastSetGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer)
{
#if ENGINE_MAJOR_VERSION < 5
	SetGraphicsPipelineState(RHICmdList, Initializer);
#else
	SetGraphicsPipelineState(RHICmdList, Initializer, 0);
#endif
}

void CopyTexture_DiffSize(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture)
{
	RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));

	FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyTexture"));
	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestTexture->GetSizeX(), DestTexture->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		
		const auto* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

		FRHISamplerState* PixelSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		
		if (EnumHasAnyFlags(SourceTexture->GetFlags(), TexCreate_SRGB))
		{
			TShaderMapRef<FScreenPSsRGBSource> PixelShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			MillicastSetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, PixelSampler, SourceTexture);
		}
		else
		{
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			MillicastSetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, PixelSampler, SourceTexture);
		}

		auto* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");
		RendererModule->DrawRectangle(RHICmdList, 0, 0, // Dest X, Y
		                              DestTexture->GetSizeX(), // Dest Width
		                              DestTexture->GetSizeY(), // Dest Height
		                              0, 0, // Source U, V
		                              1, 1, // Source USize, VSize
		                              FIntPoint(DestTexture->GetSizeX(), DestTexture->GetSizeY()), // Target buffer size
		                              FIntPoint(1, 1), // Source texture size
		                              VertexShader, EDRF_Default);
	}
	RHICmdList.EndRenderPass();
}

/*
* Copy from one texture to another. If PixelFormat and resolution is the same just do a simple GPU<->GPU copy.
* If resolution or PixelFormat are mismatched then we have to render the source texture into the destination texture.
*/
void CopyTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture)
{
	if (SourceTexture->GetFormat() == DestTexture->GetFormat()
		&& SourceTexture->GetSizeX() == DestTexture->GetSizeX()
		&& SourceTexture->GetSizeY() == DestTexture->GetSizeY())
	{
		RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
		RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
		RHICmdList.CopyTexture(SourceTexture, DestTexture, {});
		return;
	}

	CopyTexture_DiffSize(RHICmdList, SourceTexture, DestTexture);
}
