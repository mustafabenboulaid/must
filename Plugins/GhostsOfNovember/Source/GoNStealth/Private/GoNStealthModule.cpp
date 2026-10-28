#include "GoNStealthLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNStealth);

class FGoNStealthModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNStealth"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNStealth, Log, TEXT("GoN Stealth: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNStealth"));
	}
};

IMPLEMENT_MODULE(FGoNStealthModule, GoNStealth);

static FAutoConsoleCommand GGoNStealthStatusCommand(
	TEXT("gon.stealth.status"),
	TEXT("GoN Stealth: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNStealth, Display, TEXT("GoN Stealth: skeleton module; systems arrive in their milestone (M3)."));
	}));
