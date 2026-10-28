#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "GoNPlayerController.generated.h"

// Player controller of GoN maps. Guarantees Enhanced Input (UEnhancedPlayerInput) even when
// the project's input settings still name the legacy classes; the character adds its
// mapping context on possession.
UCLASS()
class GONCHARACTER_API AGoNPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void InitInputSystem() override;
};
