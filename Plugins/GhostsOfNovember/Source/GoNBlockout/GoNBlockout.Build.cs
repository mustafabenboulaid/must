using UnrealBuildTool;

// Test gym and blockout kit (M0/M2)
public class GoNBlockout : ModuleRules
{
	public GoNBlockout(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore", "Projects" });
	}
}
