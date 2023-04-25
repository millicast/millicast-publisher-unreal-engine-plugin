// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "Kismet/KismetRenderingLibrary.h"

#include "MillicastRenderTargetCanvas.generated.h"

USTRUCT()
struct FMillicastRenderTargetCanvas
{
	GENERATED_BODY()

	DECLARE_EVENT(RenderTargetCapturer, FMillicastOnRenderTargetCanvasInitialized)
	FMillicastOnRenderTargetCanvasInitialized OnInitialized;
	
	UCanvas* Get() const { return Canvas; }

	bool IsInitialized() const { return bInitialized; }

	void Initialize(UWorld* InWorld, UTextureRenderTarget2D* InRenderTarget);
	void Reset();
	
private:
	UPROPERTY()
	UCanvas* Canvas = nullptr;

	UPROPERTY()
	UWorld* World = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;
	
	// TODO [RW] atomic enum with 3 stages for absolute state management would be best, but won't touch the bool now unless it becomes problematic
	bool bInitialized = false;
	FDrawToRenderTargetContext CanvasCtx;
};
