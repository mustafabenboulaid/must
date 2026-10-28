#include "GoNSocietyLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNSociety);

class FGoNSocietyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNSociety"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNSociety, Log, TEXT("GoN Society: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNSociety"));
	}
};

IMPLEMENT_MODULE(FGoNSocietyModule, GoNSociety);

static FAutoConsoleCommand GGoNSocietyStatusCommand(
	TEXT("gon.society.status"),
	TEXT("GoN Society: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNSociety, Display, TEXT("GoN Society: skeleton module; systems arrive in their milestone (M4)."));
	}));
