#include "GoNNarrativeLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNNarrative);

class FGoNNarrativeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNNarrative"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNNarrative"));
	}
};

IMPLEMENT_MODULE(FGoNNarrativeModule, GoNNarrative);
