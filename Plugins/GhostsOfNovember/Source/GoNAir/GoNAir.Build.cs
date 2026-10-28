using UnrealBuildTool;

// Aircraft (after the vertical slice)
public class GoNAir : ModuleRules
{
	public GoNAir(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
