// Copyright Millicast 2022. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class MillicastPublisher: ModuleRules
	{
		public MillicastPublisher(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;


			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AudioCaptureCore",
					"Core",
					"CoreUObject",
					"Engine",
					"MediaAssets",
					"OpenSSL",
					"RenderCore",
					"TimeManagement",
					"WebRTC",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AudioPlatformConfiguration",
					"AudioMixer",
					"CinematicCamera",
					"HeadMountedDisplay",
					"HTTP",
					"InputCore",
					"Json",
					"libOpus",
					"MediaUtils",
					"MediaIOCore",
					"Projects",
					"Slate",
					"SlateCore",
					"Renderer",
					"RHI",
					"SSL",
					"WebSockets",
					"HeadMountedDisplay",
					"CinematicCamera",
					"InputCore",
					"libOpus",
					"AudioPlatformConfiguration",
					"AVEncoder",
					"RHICore"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MillicastPublisher/Private",
					Path.Combine(Path.GetFullPath(Target.RelativeEnginePath), "Source/ThirdParty/WebRTC/4147/Include/third_party/libyuv/include"), // for libyuv headers
				});

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");

			// required for casting UE4 BackBuffer to Vulkan Texture2D for NvEnc
			PrivateDependencyModuleNames.AddRange(new string[] { "CUDA", "VulkanRHI", "nvEncode" });

			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
			
			PrivateIncludePathModuleNames.Add("VulkanRHI");
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private"));
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				PublicDependencyModuleNames.Add("D3D11RHI");
				PublicDependencyModuleNames.Add("D3D12RHI");
				PrivateIncludePaths.AddRange(
					new string[]{
						Path.Combine(EngineDir, "Source/Runtime/D3D12RHI/Private"),
						Path.Combine(EngineDir, "Source/Runtime/D3D12RHI/Private/Windows")
					});

				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
				PublicSystemLibraries.AddRange(new string[] {
					"DXGI.lib",
					"d3d11.lib",
				});

				PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private/Windows"));
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
			{
				PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/Runtime/VulkanRHI/Private/Linux"));
			}
		}
	}
}
