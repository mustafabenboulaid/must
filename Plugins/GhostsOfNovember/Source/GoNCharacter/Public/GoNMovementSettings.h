#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Logic/GoNAnimLogic.h"
#include "Logic/GoNMovementLogic.h"

#include "GoNMovementSettings.generated.h"

class UAnimSequence;

// Editable mirror of GoN::Character::FMovementMetrics. Defaults match the gym metrics;
// `gon.char.metrics` compares them.
USTRUCT(BlueprintType)
struct GONCHARACTER_API FGoNMovementMetrics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capsule", meta = (ClampMin = "10"))
	float CapsuleRadius = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capsule", meta = (ClampMin = "20"))
	float StandHalfHeight = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capsule", meta = (ClampMin = "10"))
	float CrouchHalfHeight = 60.0f;

	// UE clamps a capsule half height to at least its radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capsule", meta = (ClampMin = "10"))
	float ProneHalfHeight = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float WalkSpeed = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float JogSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float SprintSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float CrouchSpeed = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float ProneSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float CarryWalkSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0"))
	float CarryJogSpeed = 190.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0"))
	float MaxStepHeight = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0", ClampMax = "89"))
	float WalkableSlopeDeg = 44.765f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal", meta = (ClampMin = "10"))
	float ReachDistance = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MaxVaultHeight = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MaxVaultDepth = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MinMantleDepth = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MaxClimbHeight = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float LowVaultMax = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float MidVaultMax = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float VaultClearance = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float VaultBaseSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float VaultSecondsPerMetre = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float ClimbBaseSeconds = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traversal")
	float ClimbSecondsPerMetre = 0.45f;

	GoN::Character::FMovementMetrics ToPure() const;
};

// Editable mirror of GoN::Character::FCameraProfile.
USTRUCT(BlueprintType)
struct GONCHARACTER_API FGoNCameraProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float ArmLength = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float SprintArmLength = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float CoverArmLength = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float ProneArmLength = 230.0f;

	// Side offset over the shoulder; the shoulder swap mirrors it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float ShoulderOffset = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arm")
	float CoverShoulderOffset = 80.0f;

	// Camera pivot above the feet per stance (crouch/prone offsets).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pivot")
	float PivotHeightStand = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pivot")
	float PivotHeightCrouch = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pivot")
	float PivotHeightProne = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float Fov = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lens")
	float SprintFov = 86.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoothing")
	float InterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoothing")
	float ShoulderSwapSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoothing")
	float LagSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	float MinPitch = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	float MaxPitch = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	bool bInvertLookY = false;

	// How far ahead a crouched/standing character looks for low/high cover (camera cover offset).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	float CoverProbeDistance = 90.0f;

	GoN::Character::FCameraProfile ToPure() const;
};

// Animation clips and tuning for UGoNAnimInstance. Defaults point at the project's
// Manny/Quinn assets listed in ASSET_MANIFEST.md. Empty slots use a procedural pose.
USTRUCT(BlueprintType)
struct GONCHARACTER_API FGoNAnimSettings
{
	GENERATED_BODY()

	FGoNAnimSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TSoftObjectPtr<UAnimSequence> Idle;

	// 8 directions each, order Fwd, Fwd_Right, Right, Bwd_Right, Bwd, Bwd_Left, Left, Fwd_Left.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", EditFixedSize)
	TArray<TSoftObjectPtr<UAnimSequence>> Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", EditFixedSize)
	TArray<TSoftObjectPtr<UAnimSequence>> Jog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	TSoftObjectPtr<UAnimSequence> JumpStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	TSoftObjectPtr<UAnimSequence> FallLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	TSoftObjectPtr<UAnimSequence> Land;

	// ---- Not in the project yet: procedural fallback until imported (see CHANGELOG). ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> ProneIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> ProneCrawl;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> CarryIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> CarryWalk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> VaultLow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> VaultMid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> VaultHigh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missing clips")
	TSoftObjectPtr<UAnimSequence> Climb;

	// Pose of the wounded comrade while carried / lying (default: the end of a death clip).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wounded")
	TSoftObjectPtr<UAnimSequence> WoundedPose;

	// "The same walk, only lower": hips drop, legs re-solved onto the clip's own feet.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	float CrouchPelvisDrop = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	float SprintMaxPlayRate = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	float WalkJogMaxPlayRate = 1.22f;

	// Body yaw away from the camera (idle) before the character turns in place.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	float TurnInPlaceThreshold = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	float TurnInPlaceSpeed = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	bool bHeadLookAtCamera = true;
};

// One data asset per character type (Content Browser > Miscellaneous > Data Asset >
// GoN Movement Settings). Characters without one use these class defaults.
UCLASS(BlueprintType)
class GONCHARACTER_API UGoNMovementSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Movement")
	FGoNMovementMetrics Metrics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Camera")
	FGoNCameraProfile Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Animation")
	FGoNAnimSettings Animation;

	// Wounded comrade on the carrier's shoulders, relative to the carrier's mesh bone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Carry")
	FName CarryBone = TEXT("spine_05");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Carry")
	FTransform CarryOffset = FTransform(FRotator(0.0, 0.0, 90.0), FVector(0.0, 10.0, -30.0));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Carry")
	float InteractDistance = 160.0f;
};
