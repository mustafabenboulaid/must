#include "GoNCharacterLog.h"

#include "GoNBuildInfo.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGoNCharacter);

class FGoNCharacterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GoN::Core::RegisterModuleBuild(TEXT("GoNCharacter"), __DATE__ " " __TIME__);
		UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: module started (compiled %s)"), ANSI_TO_TCHAR(__DATE__ " " __TIME__));
	}

	virtual void ShutdownModule() override
	{
		GoN::Core::UnregisterModuleBuild(TEXT("GoNCharacter"));
	}
};

IMPLEMENT_MODULE(FGoNCharacterModule, GoNCharacter);

static FAutoConsoleCommand GGoNCharacterStatusCommand(
	TEXT("gon.character.status"),
	TEXT("GoN Character: print module status."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogGoNCharacter, Display, TEXT("GoN Character: skeleton module; systems arrive in their milestone (M2)."));
	}));
