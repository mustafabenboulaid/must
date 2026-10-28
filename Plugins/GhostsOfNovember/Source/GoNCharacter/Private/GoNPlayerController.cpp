#include "GoNPlayerController.h"

#include "EnhancedPlayerInput.h"

void AGoNPlayerController::InitInputSystem()
{
	if (PlayerInput == nullptr)
	{
		PlayerInput = NewObject<UEnhancedPlayerInput>(this);
	}
	Super::InitInputSystem();
}
