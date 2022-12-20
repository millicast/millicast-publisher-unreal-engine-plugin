// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastViewportCapturerComponent.h"
#include "MillicastPublisherPrivate.h"
#include "CanvasTypes.h"
#include <EngineModule.h>
#include <LegacyScreenPercentageDriver.h>

UMillicastViewportCapturerComponent::UMillicastViewportCapturerComponent(const FObjectInitializer& ObjectInitializer) :
	EngineShowFlags(ESFIM_Game), ViewState(), ViewportWidget(nullptr), SceneViewport(nullptr), bIsInitialized(false)
{
	AsyncTask(ENamedThreads::ActualRenderingThread, [this] {
		ViewState.Allocate(ERHIFeatureLevel::ES3_1);
		});

	bWantsInitializeComponent = true;

	this->PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	this->PrimaryComponentTick.bCanEverTick = true;
	this->PrimaryComponentTick.bHighPriority = true;
	this->PrimaryComponentTick.bRunOnAnyThread = false;
	this->PrimaryComponentTick.bStartWithTickEnabled = true;
	this->PrimaryComponentTick.bTickEvenWhenPaused = true;

	this->bWantsInitializeComponent = true;

	// this->PostProcessSettings.ReflectionsType = EReflectionsType::ScreenSpace;
	this->PostProcessSettings.ScreenSpaceReflectionIntensity = 90.0f;
	this->PostProcessSettings.ScreenSpaceReflectionMaxRoughness = 0.85f;
	this->PostProcessSettings.ScreenSpaceReflectionQuality = 100.0f;

	// this->PostProcessSettings.bOverride_ReflectionsType = true;
	this->PostProcessSettings.bOverride_ScreenSpaceReflectionIntensity = true;
	this->PostProcessSettings.bOverride_ScreenSpaceReflectionMaxRoughness = true;
	this->PostProcessSettings.bOverride_ScreenSpaceReflectionQuality = true;

	this->PostProcessSettings.MotionBlurAmount = 0.0f;
	this->PostProcessSettings.MotionBlurPerObjectSize = 0.35;
	this->PostProcessSettings.MotionBlurTargetFPS = 60.0f;
	this->PostProcessSettings.MotionBlurMax = 5.0f;

	this->PostProcessSettings.bOverride_MotionBlurAmount = true;
	this->PostProcessSettings.bOverride_MotionBlurMax = true;
	this->PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
	this->PostProcessSettings.bOverride_MotionBlurTargetFPS = true;

	// this->PostProcessSettings.ScreenPercentage = 100.0f;
	// this->PostProcessSettings.bOverride_ScreenPercentage = true;

	this->ViewportWidget = nullptr;
	this->SceneViewport = nullptr;
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
		.IgnoreTextureAlpha(true);

	SceneViewport = MakeShareable(new FSceneViewport(this, ViewportWidget));
	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());

	// Create Renderable texture
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("MillicastTexture2d"),
		TargetSize.X, TargetSize.Y, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TexCreate_RenderTargetable | TexCreate_Dynamic);
	CreateDesc.SetClearValue(FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f)));

	RenderableTexture = RHICreateTexture(CreateDesc);

	if (IsActive())
	{
		FCoreDelegates::OnBeginFrame.AddUObject(this, &UMillicastViewportCapturerComponent::UpdateTexture);
	}

	bIsInitialized = true;
}

void UMillicastViewportCapturerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMillicastViewportCapturerComponent::CloseRequested(FViewport* Viewport)
{
	if (ViewFamily != nullptr)
	{
		delete ViewFamily;
		ViewFamily = nullptr;
	}
}

void UMillicastViewportCapturerComponent::Activate(bool bReset)
{
	// If Initialized component has already been called.
	if (bIsInitialized)
	{
		FCoreDelegates::OnBeginFrame.AddUObject(this, &UMillicastViewportCapturerComponent::UpdateTexture);
	}

	Super::Activate(bReset);
}

void UMillicastViewportCapturerComponent::Deactivate()
{
	FCoreDelegates::OnBeginFrame.RemoveAll(this);
	Super::Deactivate();
}

void UMillicastViewportCapturerComponent::UpdateTexture()
{
	if (UWorld* WorldContext = UActorComponent::GetWorld())
	{
		auto Time = WorldContext->GetTime();

		FScopeLock Lock(&UpdateRenderContext);

		if (ViewFamily != nullptr && View != nullptr)
		{
			GetCameraView(Time.GetDeltaRealTimeSeconds(), ViewInfo);

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
				FCanvas Canvas(this, nullptr, Time, GMaxRHIFeatureLevel);

				ViewFamily->ViewExtensions.Empty();

				// Update the ViewFamily time
				ViewFamily->Time = Time;

				// Start Rendering the ViewFamily

				GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily);

				if (RenderTarget->GetResource()->TextureRHI != RenderableTexture ||
					RenderTarget->GetResource()->TextureRHI->GetSizeXYZ() != RenderableTexture->GetSizeXYZ())
				{
					RenderTarget->GetResource()->TextureRHI = (FTexture2DRHIRef&)RenderableTexture;
					//RP_CHANGE_BEGIN - nbabin - missing texture ref update
					ENQUEUE_RENDER_COMMAND(FMillicastViewportCaptureViewFamily)
						([&](FRHICommandListImmediate& RHICmdList) {
						// ensure that the texture reference is the same as the renderable texture
						RHIUpdateTextureReference(RenderTarget->TextureReference.TextureReferenceRHI,
							RenderTarget->GetResource()->TextureRHI);
							});
					//RP_CHANGE_END
				}
			}
		}
		else
		{
			SetupView();
		}
	}
}

void UMillicastViewportCapturerComponent::SetupView()
{
	if (ViewFamily == nullptr)
	{
		FScopeLock Lock(&UpdateRenderContext);

		// Ensure we have the features we require for nice clean scene capture
		EngineShowFlags.SetOnScreenDebug(false);
		EngineShowFlags.SetSeparateTranslucency(true);
		EngineShowFlags.SetScreenPercentage(true);
		EngineShowFlags.SetTemporalAA(true);
		EngineShowFlags.SetScreenSpaceAO(true);
		EngineShowFlags.SetScreenSpaceReflections(true);

		// Construct the ViewFamily we need to render
		ViewFamily = new FSceneViewFamilyContext(FSceneViewFamily::ConstructionValues(this, GetScene(), EngineShowFlags)
			.SetTime(FGameTime::CreateDilated(0.f, 0.f, 0.f, 0.f))
			.SetRealtimeUpdate(false));

		// Update the ViewFamily with the properties we want to see in the capture
		ViewFamily->ViewMode = VMI_Lit;
		ViewFamily->EngineShowFlags = EngineShowFlags;
		ViewFamily->EngineShowFlags.ScreenSpaceReflections = true;
		ViewFamily->EngineShowFlags.ReflectionEnvironment = true;
		ViewFamily->SceneCaptureCompositeMode = SCCM_Additive;
		ViewFamily->Scene = GetScene();
		ViewFamily->bIsHDR = false;
		ViewFamily->bDeferClear = true;
		ViewFamily->bRealtimeUpdate = true;
		ViewFamily->bResolveScene = true;
		ViewFamily->SceneCaptureSource = SCS_FinalColorLDR;

		// Ensure that the viewport is capturing the way we expect it to
		ViewInitOptions.BackgroundColor = FLinearColor(0, 0, 0, 0);
		ViewInitOptions.ViewFamily = ViewFamily;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.StereoPass = EStereoscopicPass::eSSP_FULL;
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
		ENQUEUE_RENDER_COMMAND(FMillicastViewportCaptureViewFamily)
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