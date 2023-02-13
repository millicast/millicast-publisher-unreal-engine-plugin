// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include "RHICommandList.h"

/*
* Copy from one texture to another. If PixelFormat and resolution is the same just do a simple GPU<->GPU copy.
* If resolution or PixelFormat are mismatched then we have to render the source texture into the destination texture.
*/
void CopyTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture);
void CopyTexture_DiffSize(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef SourceTexture, FTexture2DRHIRef DestTexture);

void MillicastSetGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer);
