#include "GoNCharacterMovementComponent.h"

#include "GoNCharacter.h"

float UGoNCharacterMovementComponent::GetMaxSpeed() const
{
	const AGoNCharacter* GoNOwner = Cast<AGoNCharacter>(CharacterOwner);
	if (GoNOwner != nullptr && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking))
	{
		return GoNOwner->GetDesiredMaxSpeed();
	}
	return Super::GetMaxSpeed();
}

void UGoNCharacterMovementComponent::StartTraversal(const FVector& StartLocation, const FVector& Forward, const std::vector<GoN::Character::FPathKey>& Path, float Duration)
{
	TraversalPath = Path;
	TraversalStart = StartLocation;
	TraversalForward = Forward.GetSafeNormal2D();
	TraversalDuration = FMath::Max(Duration, 0.05f);
	TraversalTime = 0.0f;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EGoNCustomMovement::Traversal));
}

bool UGoNCharacterMovementComponent::IsTraversing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EGoNCustomMovement::Traversal);
}

float UGoNCharacterMovementComponent::GetTraversalAlpha() const
{
	return IsTraversing() ? FMath::Clamp(TraversalTime / TraversalDuration, 0.0f, 1.0f) : 0.0f;
}

void UGoNCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode != static_cast<uint8>(EGoNCustomMovement::Traversal) || UpdatedComponent == nullptr)
	{
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}
	TraversalTime += DeltaTime;
	const float Alpha = FMath::Clamp(TraversalTime / TraversalDuration, 0.0f, 1.0f);
	const GoN::Character::FPathKey Key = GoN::Character::EvaluateTraversePath(TraversalPath, Alpha);
	const FVector Target = TraversalStart + TraversalForward * Key.Forward + FVector::UpVector * Key.Up;
	const FVector Current = UpdatedComponent->GetComponentLocation();
	Velocity = DeltaTime > UE_SMALL_NUMBER ? (Target - Current) / DeltaTime : FVector::ZeroVector;
	// Room along the path was checked by the character's traces; the capsule follows the path
	// without sweeping so it can pass close over the obstacle's edge.
	UpdatedComponent->SetWorldLocation(Target, false, nullptr, ETeleportType::None);
	if (Alpha >= 1.0f)
	{
		Velocity = TraversalForward * 60.0;
		SetMovementMode(MOVE_Walking);
	}
}
