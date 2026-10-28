#include "GoNKnowledgeLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNKnowledge);

class FGoNKnowledgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNKnowledge"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNKnowledge, Log, TEXT("GoN Knowledge: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNKnowledge"));
	}
};

IMPLEMENT_MODULE(FGoNKnowledgeModule, GoNKnowledge);

static FAutoConsoleCommand GGoNKnowledgeStatusCommand(
	TEXT("gon.knowledge.status"),
	TEXT("GoN Knowledge: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNKnowledge, Display, TEXT("GoN Knowledge: skeleton module; systems arrive in their milestone (M4)."));
	}));
