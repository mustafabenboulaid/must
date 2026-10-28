using UnrealBuildTool;

// Fatigue and resources (M5)
public class GoNSurvival : ModuleRules
{
	public GoNSurvival(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
