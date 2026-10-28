using UnrealBuildTool;

// GameMode base, settings, save game, build info, shared pure utilities
public class GoNCore : ModuleRules
{
	public GoNCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "Projects" });
	}
}
