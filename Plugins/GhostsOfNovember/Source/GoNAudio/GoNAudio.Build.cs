using UnrealBuildTool;

// Music states, VO, environmental audio (M6)
public class GoNAudio : ModuleRules
{
	public GoNAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
