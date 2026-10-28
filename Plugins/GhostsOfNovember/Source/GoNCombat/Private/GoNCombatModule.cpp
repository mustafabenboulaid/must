#include "GoNCombatLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNCombat);

class FGoNCombatModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNCombat"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNCombat, Log, TEXT("GoN Combat: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNCombat"));
	}
};

IMPLEMENT_MODULE(FGoNCombatModule, GoNCombat);

static FAutoConsoleCommand GGoNCombatStatusCommand(
	TEXT("gon.combat.status"),
	TEXT("GoN Combat: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNCombat, Display, TEXT("GoN Combat: skeleton module; systems arrive in their milestone (M5)."));
	}));
