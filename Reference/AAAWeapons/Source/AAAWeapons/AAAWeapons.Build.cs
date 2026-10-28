using UnrealBuildTool;

public class AAAWeapons : ModuleRules
{
	public AAAWeapons(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ImageCore", "InputCore", "AnimationCore", "AnimGraphRuntime", "AIModule", "AAATraffic", "UMG", "Slate", "SlateCore", "MeshDescription", "StaticMeshDescription" });
	}
}
