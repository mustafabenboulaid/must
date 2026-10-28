#include "GoNUILog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNUI);

class FGoNUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNUI"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNUI, Log, TEXT("GoN UI: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNUI"));
	}
};

IMPLEMENT_MODULE(FGoNUIModule, GoNUI);

static FAutoConsoleCommand GGoNUIStatusCommand(
	TEXT("gon.ui.status"),
	TEXT("GoN UI: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNUI, Display, TEXT("GoN UI: skeleton module; systems arrive in their milestone (M4/M6)."));
	}));
