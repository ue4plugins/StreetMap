// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class StreetMapImporting : ModuleRules
    {
        public StreetMapImporting(ReadOnlyTargetRules Target)
			: base(Target)
        {
            PrivatePCHHeaderFile = "StreetMapImporting.h";
            
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "UnrealEd",
                    "XmlParser",
                    "AssetTools",
                    "Projects",
                    "Slate",
                    "EditorStyle",
                    "SlateCore",
                    "PropertyEditor",
                    "RenderCore",
                    "RHI",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry",
                    "StreetMapRuntime"
                }
            );
			if (Target.Version.MinorVersion <= 21)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"ShaderCore",
					}
				);
			}
        }
    }
}