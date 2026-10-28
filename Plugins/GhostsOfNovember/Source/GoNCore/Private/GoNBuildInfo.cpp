#include "GoNBuildInfo.h"

#include "Misc/ScopeLock.h"

namespace GoN::Core
{
	namespace BuildInfoDetail
	{
		FCriticalSection& Mutex()
		{
			static FCriticalSection Section;
			return Section;
		}

		TMap<FString, FString>& Builds()
		{
			static TMap<FString, FString> Map;
			return Map;
		}
	}

	void RegisterModuleBuild(const TCHAR* ModuleName, const ANSICHAR* CompiledAt)
	{
		FScopeLock Lock(&BuildInfoDetail::Mutex());
		BuildInfoDetail::Builds().Add(ModuleName, ANSI_TO_TCHAR(CompiledAt));
	}

	void UnregisterModuleBuild(const TCHAR* ModuleName)
	{
		FScopeLock Lock(&BuildInfoDetail::Mutex());
		BuildInfoDetail::Builds().Remove(ModuleName);
	}

	TArray<TPair<FString, FString>> GetModuleBuilds()
	{
		FScopeLock Lock(&BuildInfoDetail::Mutex());
		TArray<TPair<FString, FString>> Out;
		for (const TPair<FString, FString>& Pair : BuildInfoDetail::Builds())
		{
			Out.Emplace(Pair.Key, Pair.Value);
		}
		Out.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
		return Out;
	}

	const TArray<FName>& ExpectedModules()
	{
		static const TArray<FName> Modules = {
			TEXT("GoNCore"), TEXT("GoNNarrative"), TEXT("GoNStealth"), TEXT("GoNSociety"), TEXT("GoNSurvival"),
			TEXT("GoNKnowledge"), TEXT("GoNCharacter"), TEXT("GoNCombat"), TEXT("GoNAI"), TEXT("GoNAir"),
			TEXT("GoNUI"), TEXT("GoNAudio"), TEXT("GoNBlockout"),
		};
		return Modules;
	}
}
