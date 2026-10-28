#include "GoNCharacterLog.h"

#include "GoNBuildInfo.h"
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

// gon.char.debug is a console variable in GoNCharacter.cpp; gon.char.metrics lives in GoNBlockout
// (it compares against the gym metrics, and GoNBlockout depends on GoNCharacter).
