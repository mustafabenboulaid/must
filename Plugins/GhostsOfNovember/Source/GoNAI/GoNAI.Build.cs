using UnrealBuildTool;

// Perception, tactical AI, patrols (M3)
public class GoNAI : ModuleRules
{
	public GoNAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
