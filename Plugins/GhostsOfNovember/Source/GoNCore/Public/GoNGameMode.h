#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "GoNGameMode.generated.h"

// Base game mode for every GoN map. Chapter flow lands here in M6.
UCLASS()
class GONCORE_API AGoNGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
