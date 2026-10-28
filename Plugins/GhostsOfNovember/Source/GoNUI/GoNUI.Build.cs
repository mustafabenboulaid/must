using UnrealBuildTool;

// HUD, subtitles, notebook/codex UI, tactical map bridge (M4/M6)
public class GoNUI : ModuleRules
{
	public GoNUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
