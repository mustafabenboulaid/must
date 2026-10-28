#pragma once

#include "CoreMinimal.h"
#include "GoNGameMode.h"

#include "GoNGymGameMode.generated.h"

// Game mode of the test gym: on any (empty) map it spawns AGoNGymBuilder, lights and a
// player start unless the level already has them. `gon.gym.open` loads it on the
// map from Project Settings > Ghosts of November > Gym Map Path.
UCLASS()
class GONBLOCKOUT_API AGoNGymGameMode : public AGoNGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
