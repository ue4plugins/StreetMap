// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class StreetMapEditor : ModuleRules
    {
        public StreetMapEditor(TargetInfo Target)
        {
            PrivateIncludePaths.Add("StreetMapEditor/Private");
            PublicIncludePaths.Add("StreetMapEditor/Public");

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Slate",
                    "EditorStyle",
                    "SlateCore",
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "ShaderCore",
                    "RHI",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry",
                    "StreetMapRuntime",
                    "Projects"
                }
                );
        }
    }
}
