// Copyright Millicast 2022. All Rights Reserved.

#include "AVEncoderContext.h"
#include "CudaModule.h"
#include "VulkanRHIPrivate.h"
#include "MillicastPublisherPrivate.h"

#if PLATFORM_WINDOWS
	#include "VideoCommon.h"
	#include "D3D12RHICommon.h"
	#include "D3D12RHIPrivate.h"
THIRD_PARTY_INCLUDES_START
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <VersionHelpers.h>
	#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace Millicast::Publisher
{

FAVEncoderContext::FAVEncoderContext(int32 InCaptureWidth, int32 InCaptureHeight, bool bInFixedResolution)
	: CaptureWidth(InCaptureWidth)
	, CaptureHeight(InCaptureHeight)
	, bFixedResolution(bInFixedResolution)
	, VideoEncoderInput(CreateVideoEncoderInput(InCaptureWidth, InCaptureHeight, bInFixedResolution))
{
	VideoEncoderInput->SetMaxNumBuffers(3);
}

void FAVEncoderContext::DeleteBackBuffers()
{
	BackBuffers.Empty();
}

bool FAVEncoderContext::IsFixedResolution() const
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	return bFixedResolution;
#else
	return false;
#endif
}

void FAVEncoderContext::SetCaptureResolution(int NewCaptureWidth, int NewCaptureHeight)
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	// Don't change resolution if we are in a fixed resolution capturer or the user has indicated they do not want this behaviour.
	if (bFixedResolution)
	{
		return;
	}
#endif

	// Check is requested resolution is same as current resolution, if so, do nothing.
	if (CaptureWidth == NewCaptureWidth && CaptureHeight == NewCaptureHeight)
	{
		return;
	}

	verifyf(NewCaptureWidth > 0, TEXT("Capture width must be greater than zero."));
	verifyf(NewCaptureHeight > 0, TEXT("Capture height must be greater than zero."));

	CaptureWidth = NewCaptureWidth;
	CaptureHeight = NewCaptureHeight;
	
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	VideoEncoderInput->SetResolution(CaptureWidth, CaptureHeight);
#else
	// VideoEncoderInput->SetCaptureResolution(CaptureWidth, CaptureHeight);
#endif

	// Flushes available frames only, active frames still get a chance to go through the pipeline and be naturally removed from the backbuffers.
	VideoEncoderInput->Flush();
}

TSharedPtr<AVEncoder::FVideoEncoderInput> FAVEncoderContext::CreateVideoEncoderInput(int InWidth, int InHeight, bool bInFixedResolution)
{
	if (!GDynamicRHI)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("GDynamicRHI not valid for some reason."));
		return nullptr;
	}

	const FString& RHIName = GDynamicRHI->GetName();

#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	bool bIsResizable = !bInFixedResolution;
#else
	bool bIsResizable = true;
#endif

	if (RHIName == TEXT("Vulkan"))
	{
		if (IsRHIDeviceAMD())
		{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
			FVulkanDynamicRHI* DynamicRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI);
			AVEncoder::FVulkanDataStruct VulkanData = { DynamicRHI->GetInstance(), DynamicRHI->GetDevice()->GetPhysicalHandle(), DynamicRHI->GetDevice()->GetInstanceHandle() };

			return AVEncoder::FVideoEncoderInput::CreateForVulkan(&VulkanData, InWidth, InHeight, bIsResizable);
#else
			IVulkanDynamicRHI* DynamicRHI = GetDynamicRHI<IVulkanDynamicRHI>();
			AVEncoder::FVulkanDataStruct VulkanData = { DynamicRHI->RHIGetVkInstance(), DynamicRHI->RHIGetVkPhysicalDevice(), DynamicRHI->RHIGetVkDevice() };

			return AVEncoder::FVideoEncoderInput::CreateForVulkan(&VulkanData, bIsResizable);
#endif
		}
		else if (IsRHIDeviceNVIDIA())
		{
			if (FModuleManager::Get().IsModuleLoaded("CUDA"))
			{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
				return AVEncoder::FVideoEncoderInput::CreateForCUDA(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext(), InWidth, InHeight, bIsResizable);
#else
				return AVEncoder::FVideoEncoderInput::CreateForCUDA(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext(), bIsResizable);
#endif
			}
			else
			{
				UE_LOG(LogMillicastPublisher, Error, TEXT("CUDA module is not loaded!"));
				return nullptr;
			}
		}
	}
#if PLATFORM_WINDOWS
	else if (RHIName == TEXT("D3D11"))
	{
		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		return AVEncoder::FVideoEncoderInput::CreateForD3D11(GDynamicRHI->RHIGetNativeDevice(), InWidth, InHeight, bIsResizable, IsRHIDeviceAMD());
		#else
		return AVEncoder::FVideoEncoderInput::CreateForD3D11(GDynamicRHI->RHIGetNativeDevice(), bIsResizable, IsRHIDeviceAMD());
		#endif
	}
	else if (RHIName == TEXT("D3D12"))
	{
		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		return AVEncoder::FVideoEncoderInput::CreateForD3D12(GDynamicRHI->RHIGetNativeDevice(), InWidth, InHeight, bIsResizable, IsRHIDeviceNVIDIA());
		#else
		return AVEncoder::FVideoEncoderInput::CreateForD3D12(GDynamicRHI->RHIGetNativeDevice(), bIsResizable, IsRHIDeviceNVIDIA());
		#endif
	}
#endif

	UE_LOG(LogMillicastPublisher, Error, TEXT("Current RHI %s is not supported in Pixel Streaming"), *RHIName);
	return nullptr;
}

#if PLATFORM_WINDOWS
FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureDX11(FVideoEncoderInputFrameType InputFrame)
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable, ERHIAccess::CopyDest, CreateInfo);
#else
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("VideoCapturerBackBuffer"),
		CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TexCreate_Shared | TexCreate_RenderTargetable);
	CreateDesc.SetInitialState(ERHIAccess::CopyDest);

	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture(CreateDesc);
#endif

	InputFrame->SetTexture((ID3D11Texture2D*)Texture->GetNativeResource(), [this, InputFrame](ID3D11Texture2D* NativeTexture) { BackBuffers.Remove(InputFrameRef); });
	BackBuffers.Add(InputFrameRef, Texture);
	return Texture;
}

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureDX12(FVideoEncoderInputFrameType InputFrame)
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable, ERHIAccess::CopyDest, CreateInfo);
#else
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("VideoCapturerBackBuffer"), CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TexCreate_Shared | TexCreate_RenderTargetable);
	CreateDesc.SetInitialState(ERHIAccess::CopyDest);

	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture(CreateDesc);
#endif

	InputFrame->SetTexture((ID3D12Resource*)Texture->GetNativeResource(), [this, InputFrame](ID3D12Resource* NativeTexture) { BackBuffers.Remove(InputFrameRef); });
	BackBuffers.Add(InputFrameRef, Texture);
	return Texture;
}
#endif // PLATFORM_WINDOWS

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTexturePureVulkan(FVideoEncoderInputFrameType InputFrame)
{
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture =
		GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV, ERHIAccess::Present, CreateInfo);

	FVulkanTexture2D* VulkanTexture = static_cast<FVulkanTexture2D*>(Texture.GetReference());
	InputFrame->SetTexture(VulkanTexture->Surface.Image, [this, InputFrame](VkImage NativeTexture) { BackBuffers.Remove(InputFrame); });
#else
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("VideoCapturerBackBuffer"),
		CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV);
	CreateDesc.SetInitialState(ERHIAccess::Present);

	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture(CreateDesc);

	FVulkanTexture* VulkanTexture = static_cast<FVulkanTexture*>(Texture.GetReference());
	InputFrame->SetTexture(VulkanTexture->Image, [this, InputFrame](VkImage NativeTexture) { BackBuffers.Remove(InputFrameRef); });
#endif

	BackBuffers.Add(InputFrameRef, Texture);
	return Texture;
}

FCapturedInput FAVEncoderContext::ObtainCapturedInput()
{
	if (!VideoEncoderInput.IsValid())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("VideoEncoderInput is nullptr cannot capture a frame."));
		return {};
	}

	// Obtain a frame from video encoder input, we use this frame to store an RHI specific texture.
	// Note: obtain frame will recycle frames when they are no longer being used and become "available".
	FVideoEncoderInputFrameType InputFrame = VideoEncoderInput->ObtainInputFrame();
	if (!InputFrame)
	{
		return {};
	}

	// Back buffer already contains a texture for this particular frame, no need to go and make one.
	if (BackBuffers.Contains(InputFrameRef))
	{
		return FCapturedInput(InputFrame, BackBuffers[InputFrameRef]);
	}

	// Got here, backbuffer does not contain this frame/texture already, so we must create a new platform specific texture.
	const FString& RHIName = GDynamicRHI->GetName();

	FTexture2DRHIRef OutTexture;

	// VULKAN
	if (RHIName == TEXT("Vulkan"))
	{
		if (IsRHIDeviceAMD())
		{
			OutTexture = SetBackbufferTexturePureVulkan(InputFrame);
		}
		else if (IsRHIDeviceNVIDIA())
		{
			OutTexture = SetBackbufferTextureCUDAVulkan(InputFrame);
		}
		else
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Pixel Streaming only supports AMD and NVIDIA devices, this device is neither of those."));
			return {};
		}
	}
#if PLATFORM_WINDOWS
	// DX11
	else if (RHIName == TEXT("D3D11"))
	{
		OutTexture = SetBackbufferTextureDX11(InputFrame);
	}
	// DX12
	else if (RHIName == TEXT("D3D12"))
	{
		OutTexture = SetBackbufferTextureDX12(InputFrame);
	}
#endif // PLATFORM_WINDOWS
	else
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Pixel Streaming does not support this RHI - %s"), *RHIName);
		return {};
	}

	return FCapturedInput(InputFrame, OutTexture);
}

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureCUDAVulkan(FVideoEncoderInputFrameType InputFrame)
{
	// Create a texture that can be exposed to external memory
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV, ERHIAccess::Present, CreateInfo);
	FVulkanTexture2D* VulkanTexture = static_cast<FVulkanTexture2D*>(Texture.GetReference());
#else
	FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("VideoCapturerBackBuffer"),
		CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8);

	CreateDesc.SetFlags(TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV);
	CreateDesc.SetInitialState(ERHIAccess::Present);

	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture(CreateDesc);

	FVulkanTexture* VulkanTexture = static_cast<FVulkanTexture*>(Texture.GetReference());
#endif

	VkDevice Device = static_cast<FVulkanDynamicRHI*>(GDynamicRHI)->GetDevice()->GetInstanceHandle();

#if PLATFORM_WINDOWS
	HANDLE Handle;
	// It is recommended to use NT handles where available, but these are only supported from Windows 8 onward, for earliers versions of Windows
	// we need to use a Win7 style handle. NT handles require us to close them when we are done with them to prevent memory leaks.
	// Refer to remarks section of https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiresource1-createsharedhandle
	const bool bUseNTHandle = IsWindows8OrGreater();

	{
		// Generate VkMemoryGetWin32HandleInfoKHR
		VkMemoryGetWin32HandleInfoKHR MemoryGetHandleInfoKHR = {};
		MemoryGetHandleInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
		MemoryGetHandleInfoKHR.pNext = NULL;

		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		MemoryGetHandleInfoKHR.memory = VulkanTexture->Surface.GetAllocationHandle();
		#else
		MemoryGetHandleInfoKHR.memory = VulkanTexture->GetAllocationHandle();
		#endif
		
		MemoryGetHandleInfoKHR.handleType = bUseNTHandle ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT : VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;

		// While this operation is safe (and unavoidable) C4191 has been enabled and this will trigger an error with warnings as errors
	#pragma warning(push)
	#pragma warning(disable : 4191)
		PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)VulkanRHI::vkGetDeviceProcAddr(Device, "vkGetMemoryWin32HandleKHR");
		VERIFYVULKANRESULT(GetMemoryWin32HandleKHR(Device, &MemoryGetHandleInfoKHR, &Handle));
	#pragma warning(pop)
	}

	FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

	CUexternalMemory mappedExternalMemory = nullptr;

	{
		// generate a cudaExternalMemoryHandleDesc
		CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};
		CudaExtMemHandleDesc.type = bUseNTHandle ? CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32 : CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT;
		CudaExtMemHandleDesc.handle.win32.name = nullptr;
		CudaExtMemHandleDesc.handle.win32.handle = Handle;

		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		CudaExtMemHandleDesc.size = VulkanTexture->Surface.GetAllocationOffset() + VulkanTexture->Surface.GetMemorySize();
		#else
		CudaExtMemHandleDesc.size = VulkanTexture->GetAllocationOffset() + VulkanTexture->GetMemorySize();
		#endif

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&mappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

	// Only store handle to be closed on frame destruction if it is an NT handle
	Handle = bUseNTHandle ? Handle : nullptr;
#else
	void* Handle = nullptr;

	// Get the CUarray to that textures memory making sure the clear it when done
	int Fd;

	{
		// Generate VkMemoryGetFdInfoKHR
		VkMemoryGetFdInfoKHR MemoryGetFdInfoKHR = {};
		MemoryGetFdInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
		MemoryGetFdInfoKHR.pNext = nulllptr;

		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		MemoryGetFdInfoKHR.memory = VulkanTexture->Surface.GetAllocationHandle();
		#else
		MemoryGetFdInfoKHR.memory = VulkanTexture->GetAllocationHandle();
		#endif
		
		MemoryGetFdInfoKHR.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

		// While this operation is safe (and unavoidable) C4191 has been enabled and this will trigger an error with warnings as errors
	#pragma warning(push)
	#pragma warning(disable : 4191)
		PFN_vkGetMemoryFdKHR FPGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)VulkanRHI::vkGetDeviceProcAddr(Device, "vkGetMemoryFdKHR");
		VERIFYVULKANRESULT(FPGetMemoryFdKHR(Device, &MemoryGetFdInfoKHR, &Fd));
	#pragma warning(pop)
	}

	FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

	CUexternalMemory mappedExternalMemory = nullptr;

	{
		// generate a cudaExternalMemoryHandleDesc
		CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};
		CudaExtMemHandleDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
		CudaExtMemHandleDesc.handle.fd = Fd;

		#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		CudaExtMemHandleDesc.size = VulkanTexture->Surface.GetAllocationOffset() + VulkanTexture->Surface.GetMemorySize();
		#else
		CudaExtMemHandleDesc.size = VulkanTexture->GetAllocationOffset() + VulkanTexture->GetMemorySize();
		#endif

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&mappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

#endif

	CUmipmappedArray mappedMipArray = nullptr;
	CUarray mappedArray = nullptr;

	{
		CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipmapDesc = {};
		mipmapDesc.numLevels = 1;
#if ENGINE_MAJOR_VERSION < 5 || ENGINE_MINOR_VERSION == 0
		mipmapDesc.offset = VulkanTexture->Surface.GetAllocationOffset();
#else
		mipmapDesc.offset = VulkanTexture->GetAllocationOffset();
#endif
		mipmapDesc.arrayDesc.Width = Texture->GetSizeX();
		mipmapDesc.arrayDesc.Height = Texture->GetSizeY();
		mipmapDesc.arrayDesc.Depth = 0;
		mipmapDesc.arrayDesc.NumChannels = 4;
		mipmapDesc.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
		mipmapDesc.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST | CUDA_ARRAY3D_COLOR_ATTACHMENT;

		// get the CUarray from the external memory
		auto result = FCUDAModule::CUDA().cuExternalMemoryGetMappedMipmappedArray(&mappedMipArray, mappedExternalMemory, &mipmapDesc);
		if (result != CUDA_SUCCESS)
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to bind mipmappedArray error: %d"), result);
		}
	}

	// get the CUarray from the external memory
	CUresult mipMapArrGetLevelErr = FCUDAModule::CUDA().cuMipmappedArrayGetLevel(&mappedArray, mappedMipArray, 0);
	if (mipMapArrGetLevelErr != CUDA_SUCCESS)
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to bind to mip 0."));
	}

	FCUDAModule::CUDA().cuCtxPopCurrent(nullptr);

#if ENGINE_MAJOR_VERSION < 5
	InputFrame->SetTexture(mappedArray,
#else
	InputFrame->SetTexture(mappedArray, AVEncoder::FVideoEncoderInputFrame::EUnderlyingRHI::Vulkan, Handle, 
#endif
	[this, mappedArray, mappedMipArray, mappedExternalMemory, InputFrame](CUarray NativeTexture)
	{
		// free the cuda types
		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		if (mappedArray)
		{
			auto result = FCUDAModule::CUDA().cuArrayDestroy(mappedArray);
			if (result != CUDA_SUCCESS)
			{
				UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to destroy mappedArray: %d"), result);
			}
		}

		if (mappedMipArray)
		{
			auto result = FCUDAModule::CUDA().cuMipmappedArrayDestroy(mappedMipArray);
			if (result != CUDA_SUCCESS)
			{
				UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to destroy mappedMipArray: %d"), result);
			}
		}

		if (mappedExternalMemory)
		{
			const auto Result = FCUDAModule::CUDA().cuDestroyExternalMemory(mappedExternalMemory);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to destroy mappedExternalMemoryArray: %d"), Result);
			}
		}

		FCUDAModule::CUDA().cuCtxPopCurrent(nullptr);

		// finally remove the input frame
		BackBuffers.Remove(InputFrameRef);
	});

	BackBuffers.Add(InputFrameRef, Texture);
	return Texture;
}

}
