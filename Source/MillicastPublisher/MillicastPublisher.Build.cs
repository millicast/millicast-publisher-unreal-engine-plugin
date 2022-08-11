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
		}
	}
}
