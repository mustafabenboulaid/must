using UnrealBuildTool;

// Scarce period weapons, loot, grenades (M5)
public class GoNCombat : ModuleRules
{
	public GoNCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
