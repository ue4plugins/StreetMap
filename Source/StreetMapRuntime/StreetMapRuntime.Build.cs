// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapRuntime : ModuleRules
	{
        public StreetMapRuntime(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"ShaderCore",
                    "PropertyEditor"
                }
			);
		}
	}
}