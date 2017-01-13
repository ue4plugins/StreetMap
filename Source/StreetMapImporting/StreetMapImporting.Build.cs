// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapImporting : ModuleRules
	{
        public StreetMapImporting(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "UnrealEd",
                    "XmlParser",
                    "AssetTools",
                    "StreetMapRuntime"
                }
			);
		}
	}
}