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
			PrivatePCHHeaderFile = "Private/PCH.h";


			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
					"OpenSSL",
					"TimeManagement",
					"WebRTC",
					"RenderCore",
					"AudioCaptureCore"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
					"MediaUtils",
					"MediaIOCore",
					"Projects",
					"Slate",
					"SlateCore",
					"AudioMixer",
					"WebSockets",
					"HTTP",
					"Json",
					"SSL",
					"RHI",
					"HeadMountedDisplay",
					"CinematicCamera",
					"InputCore",
					"libOpus",
					"AudioPlatformConfiguration",
					"AVEncoder",
					"RHICore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MillicastPublisher/Private",
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
