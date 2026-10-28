#include "GoNSurvivalLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNSurvival);

class FGoNSurvivalModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNSurvival"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNSurvival, Log, TEXT("GoN Survival: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNSurvival"));
	}
};

IMPLEMENT_MODULE(FGoNSurvivalModule, GoNSurvival);

static FAutoConsoleCommand GGoNSurvivalStatusCommand(
	TEXT("gon.survival.status"),
	TEXT("GoN Survival: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNSurvival, Display, TEXT("GoN Survival: skeleton module; systems arrive in their milestone (M5)."));
	}));
