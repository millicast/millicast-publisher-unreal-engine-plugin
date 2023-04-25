#include "MillicastRenderTargetCanvas.h"

void FMillicastRenderTargetCanvas::Initialize(UWorld* InWorld, UTextureRenderTarget2D* InRenderTarget)
{
	if(bInitialized)
	{
		return;
	}
	bInitialized = true;

	RenderTarget = InRenderTarget;
	World = InWorld;
	
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		if (!IsValid(World))
		{
			bInitialized = false;
			return;
		}

		FVector2D DummySize;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RenderTarget, Canvas, DummySize, CanvasCtx);

		OnInitialized.Broadcast();
	});
}

void FMillicastRenderTargetCanvas::Reset()
{
	if(!bInitialized)
	{
		return;
	}

	if(IsValid(World))
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, CanvasCtx);
	}
	
	bInitialized = false;
	Canvas = nullptr;
	CanvasCtx = {};
}
