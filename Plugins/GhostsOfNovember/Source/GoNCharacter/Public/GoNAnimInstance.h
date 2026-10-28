#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Logic/GoNAnimLogic.h"

#include "GoNAnimInstance.generated.h"

class UAnimSequence;

// Game thread -> animation proxy, once per frame (filled in NativeUpdateAnimation).
struct FGoNAnimInputs
{
	FVector LocalVelocity = FVector::ZeroVector; // relative to the actor's facing
	bool bFalling = false;
	float VelocityZ = 0.0f;
	float CrouchTarget = 0.0f;
	float ProneTarget = 0.0f;
	bool bCarrying = false;
	bool bSprinting = false;

	bool bTraversing = false;
	float TraversalAlpha = 0.0f;
	int32 TraversalKind = 0; // 0 none, 1 vault low, 2 vault mid, 3 vault high, 4 climb
	FVector LedgeWorld = FVector::ZeroVector;
	FVector TraverseForwardWorld = FVector::ForwardVector;

	bool bLookAt = false;
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;

	bool bGroundHitL = false;
	bool bGroundHitR = false;
	float GroundHitL = 0.0f; // relative to the capsule bottom
	float GroundHitR = 0.0f;
	FVector GroundNormalL = FVector::UpVector;
	FVector GroundNormalR = FVector::UpVector;
};

// Clips + tuning copied into the proxy (resolved on the game thread).
struct FGoNAnimAssets
{
	const UAnimSequence* Idle = nullptr;
	const UAnimSequence* Walk[GoN::Character::LocomotionDirections] = {};
	const UAnimSequence* Jog[GoN::Character::LocomotionDirections] = {};
	const UAnimSequence* FallLoop = nullptr;
	const UAnimSequence* ProneIdle = nullptr;
	const UAnimSequence* ProneCrawl = nullptr;
	const UAnimSequence* CarryIdle = nullptr;
	const UAnimSequence* CarryWalk = nullptr;
	const UAnimSequence* Vault[3] = {};
	const UAnimSequence* Climb = nullptr;
	float CrouchPelvisDrop = 30.0f;
	float WalkJogMaxPlayRate = 1.22f;
	float SprintMaxPlayRate = 1.6f;
};

// Native animation proxy, no Anim Blueprint (ported from the prototype's FAAARifleProxy):
// idle <-> 8-way walk/jog on one synced gait phase with play rates measured from the clips'
// planted foot; crouch = the same walk with the hips lowered and the legs re-solved; planted
// feet + turn in place; foot/pelvis IK on steps and slopes; IK/FK-guarded hand IK (ledge,
// carry); head look-at; procedural prone / vault / climb / carry poses while clips are missing.
struct FGoNAnimInstanceProxy : public FAnimInstanceProxy
{
	FGoNAnimInstanceProxy() {}
	explicit FGoNAnimInstanceProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void Analyze(FPoseContext& Output);
	void ApplyBodyModifiers(FPoseContext& Output);

	FGoNAnimInputs In;
	FGoNAnimAssets Assets;

	// Measured once from the clips (Analyze).
	bool bAnalyzed = false;
	GoN::Character::FDirectionValues WalkSpeeds{};
	GoN::Character::FDirectionValues JogSpeeds{};
	GoN::Character::FDirectionValues WalkLengths{};
	GoN::Character::FDirectionValues JogLengths{};

	// Anim-thread state.
	float Dt = 0.0f;
	float IdleTime = 0.0f;
	float FallTime = 0.0f;
	double SmoothSpeed = 0.0;
	double SmoothAngle = 0.0;
	double Phase = 0.0;
	double CrawlPhase = 0.0;
	double CrouchWeight = 0.0;
	double CrouchVelocity = 0.0;
	float MoveWeight = 0.0f;
	float FallWeight = 0.0f;
	float ProneWeight = 0.0f;
	float CarryWeight = 0.0f;
	float TraverseWeight = 0.0f;
	float PlantWeight = 0.0f;
	float GroundWeight = 0.0f;
	float GroundOffsetL = 0.0f;
	float GroundOffsetR = 0.0f;
	float GroundPelvis = 0.0f;
	float LookWeight = 0.0f;
	float LookYaw = 0.0f;
	float LookPitch = 0.0f;
	GoN::Character::FGaitBlend Gait;
	GoN::Character::FTurnInPlace TurnInPlace;
	GoN::Character::FFootPlanter Planter;
};

UCLASS(Transient)
class GONCHARACTER_API UGoNAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Clips the settings name but the project lacks (logged once with the expected path).
	TArray<FString> GetMissingClips() const { return MissingClips; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FGoNAnimInstanceProxy;

	void UpdateGroundTraces();

	FGoNAnimInputs Inputs;
	FGoNAnimAssets ResolvedAssets;
	TArray<FString> MissingClips;

	// Keeps the loaded clips alive for the proxy.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> LoadedClips;
};
