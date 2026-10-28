using UnrealBuildTool;

// Local trust and communication network (M4)
public class GoNSociety : ModuleRules
{
	public GoNSociety(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
