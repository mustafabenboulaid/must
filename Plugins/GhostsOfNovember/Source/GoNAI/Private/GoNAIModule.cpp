#include "GoNAILog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNAI);

class FGoNAIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNAI"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNAI, Log, TEXT("GoN AI: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNAI"));
	}
};

IMPLEMENT_MODULE(FGoNAIModule, GoNAI);

static FAutoConsoleCommand GGoNAIStatusCommand(
	TEXT("gon.ai.status"),
	TEXT("GoN AI: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNAI, Display, TEXT("GoN AI: skeleton module; systems arrive in their milestone (M3)."));
	}));
