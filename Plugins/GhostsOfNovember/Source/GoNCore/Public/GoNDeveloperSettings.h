#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "Templates/SubclassOf.h"

#include "GoNDeveloperSettings.generated.h"

class APawn;

// Project Settings > Game > Ghosts of November. Saved to Config/DefaultGame.ini.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Ghosts of November"))
class GONCORE_API UGoNDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGoNDeveloperSettings();

	static const UGoNDeveloperSettings* Get() { return GetDefault<UGoNDeveloperSettings>(); }

	// Empty map that `gon.gym.open` loads before the gym game mode generates the course.
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	FString GymMapPath = TEXT("/Engine/Maps/Entry");

	// Pawn used in the gym. Empty = AGoNCharacter. Set your character Blueprint here to override.
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	TSoftClassPtr<APawn> GymPawnClass;

	// Mission the gym starts on load (its narrative trigger completes the first beat). None = none.
	UPROPERTY(Config, EditAnywhere, Category = "Gym")
	FName GymAutoStartMission = TEXT("gym_test_mission");

	// Overrides Plugins/GhostsOfNovember/Data/Narrative (absolute path). Leave empty normally.
	UPROPERTY(Config, EditAnywhere, Category = "Narrative")
	FDirectoryPath NarrativeDataDirectoryOverride;

	UPROPERTY(Config, EditAnywhere, Category = "Narrative")
	FString CheckpointSlotName = TEXT("GoN_Checkpoint");

	UPROPERTY(Config, EditAnywhere, Category = "Narrative")
	bool bSaveAtCheckpoints = true;

	// Log validation warnings (not only errors) when narrative data loads.
	UPROPERTY(Config, EditAnywhere, Category = "Narrative")
	bool bLogValidationWarnings = true;
};
