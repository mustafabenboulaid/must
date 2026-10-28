#pragma once

#include "CoreMinimal.h"

// Each GoN module registers its compile time at startup. `gon.core.buildinfo` prints them,
// so a screenshot/log can be matched against the sources before diagnosing (brief §8.5).
namespace GoN::Core
{
	GONCORE_API void RegisterModuleBuild(const TCHAR* ModuleName, const ANSICHAR* CompiledAt);
	GONCORE_API void UnregisterModuleBuild(const TCHAR* ModuleName);
	GONCORE_API TArray<TPair<FString, FString>> GetModuleBuilds();

	// Every module listed in GhostsOfNovember.uplugin.
	GONCORE_API const TArray<FName>& ExpectedModules();
}
