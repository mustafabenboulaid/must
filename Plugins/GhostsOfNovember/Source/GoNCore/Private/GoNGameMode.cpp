#include "GoNGameMode.h"

#include "GoNCoreLog.h"

void AGoNGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	UE_LOG(LogGoNCore, Log, TEXT("GoN Core: %s on map %s"), *GetClass()->GetName(), *MapName);
}
