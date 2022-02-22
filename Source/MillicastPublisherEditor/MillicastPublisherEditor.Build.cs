// Copyright Millicast 2022. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MillicastPublisherEditor : ModuleRules
	{
		public MillicastPublisherEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"UnrealEd"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MillicastPublisher",
					"Engine",
					"MediaAssets",
					"Projects",
					"SlateCore",
					"PlacementMode",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
				});

			PrivateIncludePaths.Add("MillicastPublisherEditor/Private");
		}
	}
}
