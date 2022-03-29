// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastViewportCapturerComponent.h"

#include <EngineModule.h>
#include <LegacyScreenPercentageDriver.h>

UMillicastViewportCapturerComponent::UMillicastViewportCapturerComponent(const FObjectInitializer& ObjectInitializer) :
	EngineShowFlags(ESFIM_Game), ViewState(), ViewportWidget(nullptr), SceneViewport(nullptr)
{
	ViewState.Allocate();

	bWantsInitializeComponent = true;

	/** Setup tick component */
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bHighPriority = true;
	PrimaryComponentTick.bRunOnAnyThread = false;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UMillicastViewportCapturerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Create viewport widget with gamma correction
	ViewportWidget = SNew(SViewport)
		.RenderDirectlyToWindow(false)
		.IsEnabled(true)
		.EnableGammaCorrection(true)
		.EnableBlending(true)
		.EnableStereoRendering(false)
		.ForceVolatile(true)
		.IgnoreTextureAlpha(false);

	SceneViewport = MakeShareable(new FSceneViewport(this, ViewportWidget));
	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	
	// Setup tick interval
	PrimaryComponentTick.TickInterval = TargetRate.Denominator / (float)TargetRate.Numerator;;

	/** Create Renderable texture */
	FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)) };

	FTexture2DRHIRef ReferenceTexture = RHICreateTexture2D(TargetSize.X, TargetSize.Y, EPixelFormat::PF_B8G8R8A8, 1,
		1, TexCreate_CPUReadback, CreateInfo);

	RHICreateTargetableShaderResource2D(TargetSize.X, TargetSize.Y, EPixelFormat::PF_B8G8R8A8, 1, TexCreate_Dynamic,
		TexCreate_RenderTargetable, false, CreateInfo, RenderableTexture,
		ReferenceTexture);
}

void UMillicastViewportCapturerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (UWorld* WorldContext = UActorComponent::GetWorld())
	{
		float TimeSeconds      = WorldContext->GetTimeSeconds();
		float RealTimeSeconds  = WorldContext->GetRealTimeSeconds();
		float DeltaTimeSeconds = WorldContext->GetDeltaSeconds();

		FScopeLock Lock(&UpdateRenderContext);

		if (ViewFamily != nullptr && View != nullptr)
		{
			GetCameraView(DeltaTimeSeconds, ViewInfo);

			View->UpdateProjectionMatrix(ViewInfo.CalculateProjectionMatrix());

			View->ViewLocation = ViewInfo.Location;
			View->ViewRotation = ViewInfo.Rotation;
			View->UpdateViewMatrix();

			View->StartFinalPostprocessSettings(ViewInfo.Location);
			View->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);
			View->EndFinalPostprocessSettings(ViewInitOptions);

			// Check if RenderTarget has been set
			if (RenderTarget)
			{
				// Create the canvas for rendering the view family
				FCanvas Canvas(this, nullptr, RealTimeSeconds, DeltaTimeSeconds, TimeSeconds,
					GMaxRHIFeatureLevel);

				ViewFamily->ViewExtensions.Empty();

				// Update the ViewFamily time
				ViewFamily->CurrentRealTime = RealTimeSeconds;
				ViewFamily->CurrentWorldTime = TimeSeconds;
				ViewFamily->DeltaWorldTime = DeltaTimeSeconds;

				// Start Rendering the ViewFamily
				GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily);

				if (RenderTarget->Resource->TextureRHI != RenderableTexture ||
					RenderTarget->Resource->TextureRHI->GetSizeXYZ() != RenderableTexture->GetSizeXYZ())
				{
					RenderTarget->Resource->TextureRHI = (FTexture2DRHIRef&)RenderableTexture;

					ENQUEUE_RENDER_COMMAND(FNDIViewportCaptureServiceUpdateTextureBackbuffer)
						([this](FRHICommandListImmediate& RHICmdList) {
							if (RenderTarget)
							{
								RHIUpdateTextureReference(RenderTarget->TextureReference.TextureReferenceRHI,
									RenderTarget->Resource->TextureRHI);
							}
						});
				}
			}
		}
		else
		{
			SetupView();
		}
	}
}

void UMillicastViewportCapturerComponent::CloseRequested(FViewport* Viewport)
{
	if (ViewFamily != nullptr)
	{
		delete ViewFamily;
		ViewFamily = nullptr;
	}
}

void UMillicastViewportCapturerComponent::SetupView()
{
	if (ViewFamily == nullptr)
	{
		FScopeLock Lock(&UpdateRenderContext);

		if (ViewFamily != nullptr)
		{
			delete ViewFamily;
			ViewFamily = nullptr;
		}

		// Ensure we have the features we require for nice clean scene capture
		EngineShowFlags.SetOnScreenDebug(false);
		EngineShowFlags.SetSeparateTranslucency(true);
		EngineShowFlags.SetScreenPercentage(true);
		EngineShowFlags.SetTemporalAA(true);
		EngineShowFlags.SetScreenSpaceAO(true);
		EngineShowFlags.SetScreenSpaceReflections(true);

		// Construct the ViewFamily we need to render
		ViewFamily = new FSceneViewFamilyContext(FSceneViewFamily::ConstructionValues(this, GetScene(), EngineShowFlags)
			.SetWorldTimes(0.0f, 0.0f, 0.0f)
			.SetRealtimeUpdate(true));

		// Update the ViewFamily with the properties we want to see in the capture
		ViewFamily->ViewMode = VMI_Lit;
		ViewFamily->EngineShowFlags = EngineShowFlags;
		ViewFamily->EngineShowFlags.ScreenSpaceReflections = true;
		ViewFamily->EngineShowFlags.ReflectionEnvironment = true;
		ViewFamily->SceneCaptureCompositeMode = SCCM_Additive;
		ViewFamily->Scene = GetScene();
		ViewFamily->bIsHDR = true;
		ViewFamily->bDeferClear = true;
		ViewFamily->bRealtimeUpdate = true;
		ViewFamily->bResolveScene = true;
		ViewFamily->SceneCaptureSource = SCS_FinalColorLDR;

		// Ensure that the viewport is capturing the way we expect it to
		ViewInitOptions.BackgroundColor = FLinearColor(0, 0, 0, 0);
		ViewInitOptions.ViewFamily = ViewFamily;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.StereoPass = eSSP_FULL;
		ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;
		ViewInitOptions.FOV = ViewInfo.FOV;
		ViewInitOptions.OverrideFarClippingPlaneDistance = 100000.0f;
		ViewInitOptions.CursorPos = FIntPoint(-1, -1);
		ViewInitOptions.ViewOrigin = ViewInfo.Location;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TargetSize.X, TargetSize.Y));

		// Initialize the FOV
		ViewInfo.FOV = FieldOfView;

		// Generate a default view rotation
		ViewInitOptions.ViewRotationMatrix =
			FInverseRotationMatrix(ViewInfo.Rotation) *
			FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));

		FMinimalViewInfo::CalculateProjectionMatrixGivenView(
			ViewInfo, EAspectRatioAxisConstraint::AspectRatio_MajorAxisFOV, SceneViewport.Get(), ViewInitOptions);

		// This is required by the 'BeginRenderingViewFamily' function call
		if (ViewFamily->GetScreenPercentageInterface() == nullptr)
		{
			ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*ViewFamily, 1.0f, true));
		}

		// Call the engine to create a view on the Render Thread
		ENQUEUE_RENDER_COMMAND(FNDIViewportCaptureServiceUpdateViewFamily)
			([&](FRHICommandListImmediate& RHICmdList) {
			FScopeLock Lock(&UpdateRenderContext);

			GetRendererModule().CreateAndInitSingleView(RHICmdList, ViewFamily, &ViewInitOptions);

			View = const_cast<FSceneView*>(ViewFamily->Views[0]);
		});
	}
}

void UMillicastViewportCapturerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	FCoreDelegates::OnBeginFrame.RemoveAll(this);
}