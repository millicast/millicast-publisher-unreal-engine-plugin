// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "Kismet/KismetRenderingLibrary.h"

#include "MillicastRenderTargetCanvas.generated.h"

enum class EMillicastRenderTargetCanvasState : uint8
{
	Uninitialized,
	Initializing,
	Ready
};

UCLASS()
class UMillicastRenderTargetCanvas : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_EVENT(RenderTargetCapturer, FMillicastOnRenderTargetCanvasInitialized)
	FMillicastOnRenderTargetCanvasInitialized OnInitialized;
	
	UCanvas* Get() const { return Canvas; }

	bool IsReady() const { return State == EMillicastRenderTargetCanvasState::Ready; }

	void Initialize(UWorld* InWorld, UTextureRenderTarget2D* InRenderTarget);
	void Reset();
	
private:
	UPROPERTY()
	UCanvas* Canvas = nullptr;

	UPROPERTY()
	UWorld* World = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;
	
	EMillicastRenderTargetCanvasState State = EMillicastRenderTargetCanvasState::Uninitialized;
	FDrawToRenderTargetContext CanvasCtx;

	FCriticalSection CriticalSection;
};
