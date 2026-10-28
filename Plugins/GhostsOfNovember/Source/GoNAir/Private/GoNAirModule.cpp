#include "GoNAirLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNAir);

class FGoNAirModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNAir"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNAir, Log, TEXT("GoN Air: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNAir"));
	}
};

IMPLEMENT_MODULE(FGoNAirModule, GoNAir);

static FAutoConsoleCommand GGoNAirStatusCommand(
	TEXT("gon.air.status"),
	TEXT("GoN Air: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNAir, Display, TEXT("GoN Air: skeleton module; systems arrive in their milestone (after the vertical slice)."));
	}));
