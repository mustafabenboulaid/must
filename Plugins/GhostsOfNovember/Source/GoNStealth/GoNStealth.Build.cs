using UnrealBuildTool;

// Suspicion and exposure (M3)
public class GoNStealth : ModuleRules
{
	public GoNStealth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
