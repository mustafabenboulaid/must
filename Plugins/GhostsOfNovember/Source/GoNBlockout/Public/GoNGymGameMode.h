#pragma once

#include "CoreMinimal.h"
#include "GoNGameMode.h"

#include "GoNGymGameMode.generated.h"

// Game mode of the test gym: on any (empty) map it spawns AGoNGymBuilder, lights, a player
// start, a wounded comrade to carry and a narrative trigger at the low cover, unless the level
// already has them. The player is an AGoNCharacter (or Project Settings > Gym Pawn Class) and
// the gym mission (Gym Auto Start Mission) starts on load. `gon.gym.open` loads it on the map
// from Project Settings > Ghosts of November > Gym Map Path.
UCLASS()
class GONBLOCKOUT_API AGoNGymGameMode : public AGoNGameMode
{
	GENERATED_BODY()

public:
	AGoNGymGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
};
