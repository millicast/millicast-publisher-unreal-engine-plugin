// Copyright Millicast 2022. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include "Camera/CameraComponent.h"
#include <Engine/TextureRenderTarget2D.h>
#include <Misc/FrameRate.h>
#include <Slate/SceneViewport.h>
#include <Widgets/SViewport.h>

#include "MillicastViewportCapturerComponent.generated.h"

/** A component used to capture viewport from a Millicast Camera Actor */
UCLASS(BlueprintType, Blueprintable, Category = "Millicast Publisher", META = (DisplayName = "Millicast Viewport Capturer Component", BlueprintSpawnableComponent))
class MILLICASTPUBLISHER_API UMillicastViewportCapturerComponent : public UCameraComponent,
	public FCommonViewportClient,
	public FRenderTarget
{
	GENERATED_UCLASS_BODY()

public:
	UTextureRenderTarget2D* RenderTarget = nullptr;

private:
	/** Width and height of the viewport. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings",
		META = (DisplayName = "Capture Size", AllowPrivateAccess = true,
			EditCondition = "bOverrideBroadcastSettings"))
	FIntPoint TargetSize = FIntPoint(1280, 720);

	/** Target FPS to capture the viewport */
	UPROPERTY(BlueprintReadwrite, EditAnywhere, Category = "Capture Settings",
		META = (DisplayName = "Capture Rate", AllowPrivateAccess = true,
			EditCondition = "bOverrideBroadcastSettings"))
	FFrameRate TargetRate = FFrameRate(60, 1);

protected:
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);
	virtual void CloseRequested(FViewport* Viewport) override;

	// RenderTarget overrides
	virtual FIntPoint GetSizeXY() const override   { return TargetSize;  }
	virtual float GetDisplayGamma() const override { return GAMMA; }
	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const override 
	{ 
		return (FTexture2DRHIRef&)RenderableTexture; 
	}
private:
	void SetupView();

private:
	FMinimalViewInfo         ViewInfo;
	FEngineShowFlags         EngineShowFlags;
	FSceneViewStateReference ViewState;
	FSceneViewInitOptions    ViewInitOptions;

	FSceneView              * View       = nullptr;
	FSceneViewFamilyContext * ViewFamily = nullptr;

	FCriticalSection UpdateRenderContext;

	TSharedPtr<SViewport>      ViewportWidget    = nullptr;
	TSharedPtr<FSceneViewport> SceneViewport     = nullptr;
	FTexture2DRHIRef           RenderableTexture = nullptr;

	static constexpr auto GAMMA = 2.2f;
};