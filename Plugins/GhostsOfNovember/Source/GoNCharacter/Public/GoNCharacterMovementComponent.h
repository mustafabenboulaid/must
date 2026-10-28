#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logic/GoNMovementLogic.h"

#include "GoNCharacterMovementComponent.generated.h"

UENUM()
enum class EGoNCustomMovement : uint8
{
	None = 0,
	Traversal = 1 // vault / climb along a precomputed path
};

// Speeds come from the owning AGoNCharacter (stance, gait, carrying). Vault and climb run as
// a custom movement mode that follows GoN::Character::BuildTraversePath.
UCLASS()
class GONCHARACTER_API UGoNCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;

	// Start world location = capsule centre; Forward is horizontal.
	void StartTraversal(const FVector& StartLocation, const FVector& Forward, const std::vector<GoN::Character::FPathKey>& Path, float Duration);
	bool IsTraversing() const;
	float GetTraversalAlpha() const;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	std::vector<GoN::Character::FPathKey> TraversalPath;
	FVector TraversalStart = FVector::ZeroVector;
	FVector TraversalForward = FVector::ForwardVector;
	float TraversalDuration = 0.0f;
	float TraversalTime = 0.0f;
};
