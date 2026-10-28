#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logic/GoNMovementLogic.h"

#include "GoNCharacter.generated.h"

class AGoNWoundedComrade;
class APlayerController;
class UCameraComponent;
class UCanvas;
class UGoNInputSet;
class UGoNMovementSettings;
class USpringArmComponent;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EGoNStance : uint8
{
	Stand,
	Crouch,
	Prone
};

UENUM(BlueprintType)
enum class EGoNGait : uint8
{
	Walk,
	Jog,
	Sprint
};

// What the last vault/climb probe found (shown by `gon.char.debug`).
USTRUCT(BlueprintType)
struct GONCHARACTER_API FGoNTraverseDebug
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	FString Decision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	FVector FacePoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	FVector TopPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	FVector LandingPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	float Height = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	float Depth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traverse")
	bool bValid = false;
};

// Player / NPC character: third-person camera with shoulder swap and stance/cover offsets,
// walk/jog/sprint, crouch ("the same walk, only lower"), prone, vault, climb, carrying a
// wounded comrade. Tuning lives in UGoNMovementSettings; the rules in Logic/GoNMovementLogic.
UCLASS(Config = Game)
class GONCHARACTER_API AGoNCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGoNCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void PawnClientRestart() override;

	const UGoNMovementSettings& GetMovementSettings() const;
	GoN::Character::FMovementMetrics GetMetrics() const;

	UFUNCTION(BlueprintCallable, Category = "GoN|Character")
	bool SetStance(EGoNStance NewStance);

	UFUNCTION(BlueprintPure, Category = "GoN|Character")
	EGoNStance GetStance() const { return Stance; }

	UFUNCTION(BlueprintPure, Category = "GoN|Character")
	EGoNGait GetGait() const;

	UFUNCTION(BlueprintPure, Category = "GoN|Character")
	bool IsCarrying() const { return CarriedComrade != nullptr; }

	UFUNCTION(BlueprintPure, Category = "GoN|Character")
	bool IsInCover() const { return bInCover; }

	UFUNCTION(BlueprintPure, Category = "GoN|Character")
	bool IsTraversing() const;

	// Space: vault / climb when possible, otherwise jump (standing) or rise one stance.
	UFUNCTION(BlueprintCallable, Category = "GoN|Character")
	void JumpOrTraverse();

	// Runs the vault/climb probe without acting (debug, AI).
	GoN::Character::FTraverseDecision ProbeTraverse(GoN::Character::FTraverseProbe& OutProbe, FGoNTraverseDebug& OutDebug) const;

	UFUNCTION(BlueprintCallable, Category = "GoN|Character")
	bool PickUpComrade(AGoNWoundedComrade* Comrade);

	UFUNCTION(BlueprintCallable, Category = "GoN|Character")
	bool PutDownComrade();

	UFUNCTION(BlueprintCallable, Category = "GoN|Character")
	void SwapShoulder();

	float GetDesiredMaxSpeed() const;
	float GetTraversalAlpha() const;
	GoN::Character::FTraverseDecision GetActiveTraversal() const { return ActiveTraversal; }
	const FGoNTraverseDebug& GetLastTraverseDebug() const { return LastTraverseDebug; }
	FVector GetFeetLocation() const;
	FRotator GetLookRotation() const;
	FString DescribeState() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// Empty = class defaults of UGoNMovementSettings.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Character")
	TObjectPtr<UGoNMovementSettings> MovementSettings;

	// Empty = bindings built in C++ (UGoNInputSet::CreateRuntimeDefault).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Input")
	TObjectPtr<UGoNInputSet> InputSet;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UInputComponent* CreatePlayerInputComponent() override;
	virtual bool CanJumpInternal_Implementation() const override;

private:
	UGoNInputSet* GetInputSet();
	void ApplyMetricsToComponents();
	void UpdateCamera(float DeltaSeconds);
	void UpdateCover();
	void UpdateTurnInPlace(float DeltaSeconds);
	void FinishTraversalIfDone();
	bool TryStartTraversal();
	AGoNWoundedComrade* FindComradeInReach() const;
	void DrawDebugOverlay(UCanvas* Canvas, APlayerController* PlayerController);
	void DrawDebugWorld() const;

	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputJump();
	void InputSprintStarted();
	void InputSprintEnded();
	void InputWalkStarted();
	void InputWalkEnded();
	void InputCrouch();
	void InputProne();
	void InputInteract();

	UPROPERTY(Transient)
	TObjectPtr<UGoNInputSet> RuntimeInputSet;

	UPROPERTY(Transient)
	TObjectPtr<AGoNWoundedComrade> CarriedComrade;

	EGoNStance Stance = EGoNStance::Stand;
	bool bWantsSprint = false;
	bool bWantsWalk = false;
	bool bInCover = false;
	bool bTurningInPlace = false;
	float ShoulderSide = 1.0f;
	float ShoulderSideSmoothed = 1.0f;
	float PivotHeightSmoothed = -1.0f;
	FVector2D LastMoveInput = FVector2D::ZeroVector;
	GoN::Character::FTraverseDecision ActiveTraversal;
	EGoNStance PendingStanceAfterTraversal = EGoNStance::Stand;
	FGoNTraverseDebug LastTraverseDebug;
	FDelegateHandle DebugDrawHandle;
};
