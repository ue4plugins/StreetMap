// Copyright 2017 Mike Fricker. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StreetMapRuntime : ModuleRules
	{
        public StreetMapRuntime(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivatePCHHeaderFile = "StreetMapRuntime.h";
			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
                    "PropertyEditor",
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
			if (Target.Version.MinorVersion >= 20)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"NavigationSystem",
					}
				);
			}
		}
	}
}