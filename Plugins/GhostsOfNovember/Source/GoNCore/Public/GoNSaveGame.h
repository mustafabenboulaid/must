#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "GoNSaveGame.generated.h"

// Checkpoint save. The campaign state is an opaque JSON payload owned by the pure
// narrative core (GoN::Narrative::FCampaignState), so its format can evolve without
// header changes here.
UCLASS()
class GONCORE_API UGoNSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 SaveVersion = 1;

	UPROPERTY(SaveGame)
	FString CampaignStateJson;

	UPROPERTY(SaveGame)
	FString MissionId;

	UPROPERTY(SaveGame)
	FString CheckpointId;

	UPROPERTY(SaveGame)
	FDateTime SavedAtUtc;
};
