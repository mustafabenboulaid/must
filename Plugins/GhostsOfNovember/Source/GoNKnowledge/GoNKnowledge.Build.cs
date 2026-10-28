using UnrealBuildTool;

// Field notebook, codex, investigation (M4)
public class GoNKnowledge : ModuleRules
{
	public GoNKnowledge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
