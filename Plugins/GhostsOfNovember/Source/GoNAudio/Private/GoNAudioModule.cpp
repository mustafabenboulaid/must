#include "GoNAudioLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNAudio);

class FGoNAudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNAudio"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNAudio, Log, TEXT("GoN Audio: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNAudio"));
	}
};

IMPLEMENT_MODULE(FGoNAudioModule, GoNAudio);

static FAutoConsoleCommand GGoNAudioStatusCommand(
	TEXT("gon.audio.status"),
	TEXT("GoN Audio: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNAudio, Display, TEXT("GoN Audio: skeleton module; systems arrive in their milestone (M6)."));
	}));
