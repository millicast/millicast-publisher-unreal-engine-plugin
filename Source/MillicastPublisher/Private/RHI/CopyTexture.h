// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "CommonRenderResources.h"
#include "ScreenRendering.h"

/*
* Copy from one texture to another. If PixelFormat and resolution is the same just do a simple GPU<->GPU copy.
* If resolution or PixelFormat are mismatched then we have to render the source texture into the destination texture.
*/
inline void CopyTexture(FRHICommandList& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture)
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
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

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
