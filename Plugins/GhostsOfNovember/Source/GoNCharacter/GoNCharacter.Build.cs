using UnrealBuildTool;

// Characters, movement, camera, native animation (M2)
public class GoNCharacter : ModuleRules
{
	public GoNCharacter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GoNCore" });
	}
}
