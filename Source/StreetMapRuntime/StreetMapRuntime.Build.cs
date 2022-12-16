namespace UnrealBuildTool.Rules
{
	public class StreetMapRuntime : ModuleRules
	{
        public StreetMapRuntime(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
                    "NavigationSystem"
                }
			);
		}
	}
}