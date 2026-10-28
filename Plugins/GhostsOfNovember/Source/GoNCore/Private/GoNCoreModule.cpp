#include "GoNCoreLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNCore);

class FGoNCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNCore"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNCore, Log, TEXT("GoN Core: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNCore"));
	}
};

IMPLEMENT_MODULE(FGoNCoreModule, GoNCore);

static FAutoConsoleCommand GGoNCoreBuildInfoCommand(
	TEXT("gon.core.buildinfo"),
	TEXT("GoN Core: print the compile time of every loaded GoN module (check before diagnosing a report)."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		for (const TPair<FString, FString>& Build : GoN::Core::GetModuleBuilds())
		{
			UE_LOG(LogGoNCore, Display, TEXT("GoN Core: %-14s compiled %s"), *Build.Key, *Build.Value);
		}
	}));

static FAutoConsoleCommand GGoNCoreModulesCommand(
	TEXT("gon.core.modules"),
	TEXT("GoN Core: list the plugin's modules and whether each is loaded."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		int32 Missing = 0;
		for (const FName& Module : GoN::Core::ExpectedModules())
		{
			const bool bLoaded = FModuleManager::Get().IsModuleLoaded(Module);
			Missing += bLoaded ? 0 : 1;
			UE_LOG(LogGoNCore, Display, TEXT("GoN Core: %-14s %s"), *Module.ToString(), bLoaded ? TEXT("loaded") : TEXT("NOT LOADED"));
		}
		UE_LOG(LogGoNCore, Display, TEXT("GoN Core: %d/%d modules loaded"), GoN::Core::ExpectedModules().Num() - Missing, GoN::Core::ExpectedModules().Num());
	}));
