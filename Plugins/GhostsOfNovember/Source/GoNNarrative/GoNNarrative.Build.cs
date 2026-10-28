using UnrealBuildTool;

// Campaign/mission beat graph, consequences, checkpoints, timeline validator
public class GoNNarrative : ModuleRules
{
	public GoNNarrative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore", "Projects" });
		// Narrative JSON is read at runtime; stage it with packaged builds.
		RuntimeDependencies.Add("$(PluginDir)/Data/Narrative/...", StagedFileType.UFS);
	}
}
