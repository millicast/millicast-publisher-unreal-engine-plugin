// Copyright Millicast 2022. All Rights Reserved.

#include "AVEncoderContext.h"
#include "CudaModule.h"
#include "VulkanRHIPrivate.h"
#include "MillicastPublisherPrivate.h"

#if PLATFORM_WINDOWS
	#include "VideoCommon.h"
	#include "D3D11State.h"
	#include "D3D11Resources.h"
	#include "D3D12RHICommon.h"
	#include "D3D12RHIPrivate.h"
	#include "D3D12Resources.h"
	#include "D3D12Texture.h"
	#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <VersionHelpers.h>
THIRD_PARTY_INCLUDES_END
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

FAVEncoderContext::FAVEncoderContext(int InCaptureWidth, int InCaptureHeight, bool bInFixedResolution)
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
	return bFixedResolution;
}

int FAVEncoderContext::GetCaptureWidth() const
{
	return CaptureWidth;
}

int FAVEncoderContext::GetCaptureHeight() const
{
	return CaptureHeight;
}

TSharedPtr<AVEncoder::FVideoEncoderInput> FAVEncoderContext::GetVideoEncoderInput() const
{
	return VideoEncoderInput;
}

void FAVEncoderContext::SetCaptureResolution(int NewCaptureWidth, int NewCaptureHeight)
{
	// Don't change resolution if we are in a fixed resolution capturer or the user has indicated they do not want this behaviour.
	if (bFixedResolution)
	{
		return;
	}

	// Check is requested resolution is same as current resolution, if so, do nothing.
	if (CaptureWidth == NewCaptureWidth && CaptureHeight == NewCaptureHeight)
	{
		return;
	}

	verifyf(NewCaptureWidth > 0, TEXT("Capture width must be greater than zero."));
	verifyf(NewCaptureHeight > 0, TEXT("Capture height must be greater than zero."));

	CaptureWidth = NewCaptureWidth;
	CaptureHeight = NewCaptureHeight;

	VideoEncoderInput->SetResolution(CaptureWidth, CaptureHeight);

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

	FString RHIName = GDynamicRHI->GetName();
	bool bIsResizable = !bInFixedResolution;

	if (RHIName == TEXT("Vulkan"))
	{
		if (IsRHIDeviceAMD())
		{
			FVulkanDynamicRHI* DynamicRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI);
			AVEncoder::FVulkanDataStruct VulkanData = { DynamicRHI->GetInstance(), DynamicRHI->GetDevice()->GetPhysicalHandle(), DynamicRHI->GetDevice()->GetInstanceHandle() };

			return AVEncoder::FVideoEncoderInput::CreateForVulkan(&VulkanData, InWidth, InHeight, bIsResizable);
		}
		else if (IsRHIDeviceNVIDIA())
		{
			if (FModuleManager::Get().IsModuleLoaded("CUDA"))
			{
				return AVEncoder::FVideoEncoderInput::CreateForCUDA(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext(), InWidth, InHeight, bIsResizable);
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
		return AVEncoder::FVideoEncoderInput::CreateForD3D11(GDynamicRHI->RHIGetNativeDevice(), InWidth, InHeight, bIsResizable, IsRHIDeviceAMD());
	}
	else if (RHIName == TEXT("D3D12"))
	{
		return AVEncoder::FVideoEncoderInput::CreateForD3D12(GDynamicRHI->RHIGetNativeDevice(), InWidth, InHeight, bIsResizable, IsRHIDeviceNVIDIA());
	}
#endif

	UE_LOG(LogMillicastPublisher, Error, TEXT("Current RHI %s is not supported in Pixel Streaming"), *RHIName);
	return nullptr;
}

#if PLATFORM_WINDOWS
FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureDX11(AVEncoder::FVideoEncoderInputFrame* InputFrame)
{
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable, ERHIAccess::CopyDest, CreateInfo);
	InputFrame->SetTexture((ID3D11Texture2D*)Texture->GetNativeResource(), [this, InputFrame](ID3D11Texture2D* NativeTexture) { BackBuffers.Remove(InputFrame); });
	BackBuffers.Add(InputFrame, Texture);
	return Texture;
}

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureDX12(AVEncoder::FVideoEncoderInputFrame* InputFrame)
{
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable, ERHIAccess::CopyDest, CreateInfo);
	InputFrame->SetTexture((ID3D12Resource*)Texture->GetNativeResource(), [this, InputFrame](ID3D12Resource* NativeTexture) { BackBuffers.Remove(InputFrame); });
	BackBuffers.Add(InputFrame, Texture);
	return Texture;
}
#endif // PLATFORM_WINDOWS

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTexturePureVulkan(AVEncoder::FVideoEncoderInputFrame* InputFrame)
{
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture =
		GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV, ERHIAccess::Present, CreateInfo);

	FVulkanTexture2D* VulkanTexture = static_cast<FVulkanTexture2D*>(Texture.GetReference());
	InputFrame->SetTexture(VulkanTexture->Surface.Image, [this, InputFrame](VkImage NativeTexture) { BackBuffers.Remove(InputFrame); });
	BackBuffers.Add(InputFrame, Texture);
	return Texture;
}

FAVEncoderContext::FCapturerInput FAVEncoderContext::ObtainCapturerInput()
{
	if (!VideoEncoderInput.IsValid())
	{
		UE_LOG(LogMillicastPublisher, Error, TEXT("VideoEncoderInput is nullptr cannot capture a frame."));
		return FAVEncoderContext::FCapturerInput();
	}

	// Obtain a frame from video encoder input, we use this frame to store an RHI specific texture.
	// Note: obtain frame will recycle frames when they are no longer being used and become "available".
	AVEncoder::FVideoEncoderInputFrame* InputFrame = VideoEncoderInput->ObtainInputFrame();

	if (InputFrame == nullptr)
	{
		return FAVEncoderContext::FCapturerInput();
	}

	// Back buffer already contains a texture for this particular frame, no need to go and make one.
	if (BackBuffers.Contains(InputFrame))
	{
		return FAVEncoderContext::FCapturerInput(InputFrame, BackBuffers[InputFrame]);
	}

	// Got here, backbuffer does not contain this frame/texture already, so we must create a new platform specific texture.
	FString RHIName = GDynamicRHI->GetName();

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
			return FAVEncoderContext::FCapturerInput();
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
		return FAVEncoderContext::FCapturerInput();
	}

	return FAVEncoderContext::FCapturerInput(InputFrame, OutTexture);
}

FTexture2DRHIRef FAVEncoderContext::SetBackbufferTextureCUDAVulkan(AVEncoder::FVideoEncoderInputFrame* InputFrame)
{
	// Create a texture that can be exposed to external memory
	FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
	FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(CaptureWidth, CaptureHeight, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV, ERHIAccess::Present, CreateInfo);
	FVulkanTexture2D* VulkanTexture = static_cast<FVulkanTexture2D*>(Texture.GetReference());
	VkDevice Device = static_cast<FVulkanDynamicRHI*>(GDynamicRHI)->GetDevice()->GetInstanceHandle();

#if PLATFORM_WINDOWS
	HANDLE Handle;
	// It is recommended to use NT handles where available, but these are only supported from Windows 8 onward, for earliers versions of Windows
	// we need to use a Win7 style handle. NT handles require us to close them when we are done with them to prevent memory leaks.
	// Refer to remarks section of https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiresource1-createsharedhandle
	bool bUseNTHandle = IsWindows8OrGreater();

	{
		// Generate VkMemoryGetWin32HandleInfoKHR
		VkMemoryGetWin32HandleInfoKHR MemoryGetHandleInfoKHR = {};
		MemoryGetHandleInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
		MemoryGetHandleInfoKHR.pNext = NULL;
		MemoryGetHandleInfoKHR.memory = VulkanTexture->Surface.GetAllocationHandle();
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
		CudaExtMemHandleDesc.handle.win32.name = NULL;
		CudaExtMemHandleDesc.handle.win32.handle = Handle;
		CudaExtMemHandleDesc.size = VulkanTexture->Surface.GetAllocationOffset() + VulkanTexture->Surface.GetMemorySize();

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&mappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

	// Only store handle to be closed on frame destruction if it is an NT handle
	Handle = bUseNTHandle ? Handle : NULL;
#else
	void* Handle = nullptr;

	// Get the CUarray to that textures memory making sure the clear it when done
	int Fd;

	{
		// Generate VkMemoryGetFdInfoKHR
		VkMemoryGetFdInfoKHR MemoryGetFdInfoKHR = {};
		MemoryGetFdInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
		MemoryGetFdInfoKHR.pNext = NULL;
		MemoryGetFdInfoKHR.memory = VulkanTexture->Surface.GetAllocationHandle();
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
		CudaExtMemHandleDesc.size = VulkanTexture->Surface.GetAllocationOffset() + VulkanTexture->Surface.GetMemorySize();

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&mappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogPixelStreaming, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

#endif

	CUmipmappedArray mappedMipArray = nullptr;
	CUarray mappedArray = nullptr;

	{
		CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipmapDesc = {};
		mipmapDesc.numLevels = 1;
		mipmapDesc.offset = VulkanTexture->Surface.GetAllocationOffset();
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

	FCUDAModule::CUDA().cuCtxPopCurrent(NULL);

	InputFrame->SetTexture(mappedArray, AVEncoder::FVideoEncoderInputFrame::EUnderlyingRHI::Vulkan, Handle, [this, mappedArray, mappedMipArray, mappedExternalMemory, InputFrame](CUarray NativeTexture) {
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
			auto result = FCUDAModule::CUDA().cuDestroyExternalMemory(mappedExternalMemory);
			if (result != CUDA_SUCCESS)
			{
				UE_LOG(LogMillicastPublisher, Error, TEXT("Failed to destroy mappedExternalMemoryArray: %d"), result);
			}
		}

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);

		// finally remove the input frame
		BackBuffers.Remove(InputFrame);
	});

	BackBuffers.Add(InputFrame, Texture);
	return Texture;
}
