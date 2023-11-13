// Copyright Dolby.io 2023. All Rights Reserved.

#include "Media/MillicastRenderTargetCanvas.h"

void UMillicastRenderTargetCanvas::Initialize(UWorld* InWorld, UTextureRenderTarget2D* InRenderTarget)
{
	if(State != EMillicastRenderTargetCanvasState::Uninitialized)
	{
		return;
	}
	State = EMillicastRenderTargetCanvasState::Initializing;

	RenderTarget = InRenderTarget;
	World = InWorld;
	
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		FScopeLock Lock(&CriticalSection);
		if (!IsValid(World))
		{
			State = EMillicastRenderTargetCanvasState::Uninitialized;
			return;
		}

		if (State != EMillicastRenderTargetCanvasState::Initializing)
		{
			return;
		}
		
		FVector2D DummySize;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RenderTarget, Canvas, DummySize, CanvasCtx);

		State = EMillicastRenderTargetCanvasState::Ready;
		OnInitialized.Broadcast();
	});
}

void UMillicastRenderTargetCanvas::Reset()
{
	RenderTarget = nullptr;
	World = nullptr;
	
	FScopeLock Lock(&CriticalSection);
	if(State != EMillicastRenderTargetCanvasState::Ready)
	{
		return;
	}

	if(IsValid(World))
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, CanvasCtx);
	}

	State = EMillicastRenderTargetCanvasState::Uninitialized;
	Canvas = nullptr;
	CanvasCtx = {};
}
