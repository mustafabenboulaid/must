#include "GoNCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GoNAnimInstance.h"
#include "GoNCharacterLog.h"
#include "GoNCharacterMovementComponent.h"
#include "GoNInputSet.h"
#include "GoNMovementSettings.h"
#include "GoNStringConv.h"
#include "GoNWoundedComrade.h"
#include "HAL/IConsoleManager.h"
#include "InputActionValue.h"
#include "Logic/GoNAnimLogic.h"
#include "UObject/ConstructorHelpers.h"

namespace GoNCharacterDetail
{
	TAutoConsoleVariable<int32> CVarCharDebug(
		TEXT("gon.char.debug"),
		0,
		TEXT("GoN Character: 1 = show stance, speed, capsule and the vault/climb probe (text + traces)."));

	GoN::Character::EStance ToPure(EGoNStance Stance)
	{
		switch (Stance)
		{
		case EGoNStance::Stand: return GoN::Character::EStance::Stand;
		case EGoNStance::Crouch: return GoN::Character::EStance::Crouch;
		case EGoNStance::Prone: return GoN::Character::EStance::Prone;
		}
		return GoN::Character::EStance::Stand;
	}

	EGoNStance FromPure(GoN::Character::EStance Stance)
	{
		switch (Stance)
		{
		case GoN::Character::EStance::Stand: return EGoNStance::Stand;
		case GoN::Character::EStance::Crouch: return EGoNStance::Crouch;
		case GoN::Character::EStance::Prone: return EGoNStance::Prone;
		}
		return EGoNStance::Stand;
	}

	FCollisionQueryParams QueryParams(const AGoNCharacter& Self, const AActor* Carried)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GoNTraverse), false, &Self);
		if (Carried != nullptr)
		{
			Params.AddIgnoredActor(Carried);
		}
		return Params;
	}

	// Traversal tops and landings must be walkable floors.
	bool IsFloorHit(const FHitResult& Hit)
	{
		return Hit.bBlockingHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > 0.7;
	}
}

AGoNCharacter::AGoNCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGoNCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0, 540.0, 0.0);
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->bCanWalkOffLedgesWhenCrouching = true;
	Move->JumpZVelocity = 420.0f;
	Move->AirControl = 0.25f;
	Move->BrakingDecelerationWalking = 1400.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;
	CameraBoom->ProbeSize = 12.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// ASSET_MANIFEST.md: UE5 Manny on SK_Mannequin.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MannyFinder.Object);
	}
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -96.0), FRotator(0.0, -90.0, 0.0));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UGoNAnimInstance::StaticClass());
}

const UGoNMovementSettings& AGoNCharacter::GetMovementSettings() const
{
	return MovementSettings != nullptr ? *MovementSettings : *GetDefault<UGoNMovementSettings>();
}

GoN::Character::FMovementMetrics AGoNCharacter::GetMetrics() const
{
	return GetMovementSettings().Metrics.ToPure();
}

void AGoNCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyMetricsToComponents();
	DebugDrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &AGoNCharacter::DrawDebugOverlay));
	for (const std::string& Issue : GoN::Character::ValidateMetrics(GetMetrics()))
	{
		UE_LOG(LogGoNCharacter, Warning, TEXT("GoN Character: %s: movement settings: %s"), *GetName(), *GoN::ToFString(Issue));
	}
}

void AGoNCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDebugDrawService::Unregister(DebugDrawHandle);
	if (CarriedComrade != nullptr)
	{
		CarriedComrade->SetCarried(nullptr);
		CarriedComrade = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AGoNCharacter::ApplyMetricsToComponents()
{
	const FGoNMovementMetrics& Metrics = GetMovementSettings().Metrics;
	const float MeshDrop = Metrics.StandHalfHeight;
	GetCapsuleComponent()->SetCapsuleSize(Metrics.CapsuleRadius, Metrics.StandHalfHeight);
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -MeshDrop));
	// ACharacter offsets the mesh from this when crouching.
	BaseTranslationOffset = GetMesh()->GetRelativeLocation();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxStepHeight = Metrics.MaxStepHeight;
	Move->SetWalkableFloorAngle(Metrics.WalkableSlopeDeg);
	Move->SetCrouchedHalfHeight(Metrics.CrouchHalfHeight);
	Move->MaxWalkSpeed = Metrics.JogSpeed;
	Move->MaxWalkSpeedCrouched = Metrics.CrouchSpeed;

	const FGoNCameraProfile& Camera = GetMovementSettings().Camera;
	CameraBoom->CameraLagSpeed = Camera.LagSpeed;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (PlayerController->PlayerCameraManager != nullptr)
		{
			PlayerController->PlayerCameraManager->ViewPitchMin = Camera.MinPitch;
			PlayerController->PlayerCameraManager->ViewPitchMax = Camera.MaxPitch;
		}
	}
}

// ---- Input -----------------------------------------------------------------------------------

UInputComponent* AGoNCharacter::CreatePlayerInputComponent()
{
	// Enhanced Input regardless of the project's Default Input Component Class.
	return NewObject<UEnhancedInputComponent>(this, UEnhancedInputComponent::StaticClass(), TEXT("PawnInputComponent0"));
}

UGoNInputSet* AGoNCharacter::GetInputSet()
{
	if (InputSet != nullptr)
	{
		return InputSet;
	}
	if (RuntimeInputSet == nullptr)
	{
		RuntimeInputSet = UGoNInputSet::CreateRuntimeDefault(this);
	}
	return RuntimeInputSet;
}

void AGoNCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyMetricsToComponents();
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	UGoNInputSet* Set = GetInputSet();
	if (InputSubsystem != nullptr && Set != nullptr && Set->MappingContext != nullptr)
	{
		InputSubsystem->AddMappingContext(Set->MappingContext, 0);
	}
}

void AGoNCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UGoNInputSet* Set = GetInputSet();
	if (Input == nullptr || Set == nullptr)
	{
		UE_LOG(LogGoNCharacter, Error, TEXT("GoN Character: no Enhanced Input component; enable the Enhanced Input plugin"));
		return;
	}
	const auto Bind = [Input](const UInputAction* Action, ETriggerEvent Event, AGoNCharacter* Self, void (AGoNCharacter::*Handler)())
	{
		if (Action != nullptr)
		{
			Input->BindAction(Action, Event, Self, Handler);
		}
	};
	if (Set->Move != nullptr)
	{
		Input->BindAction(Set->Move, ETriggerEvent::Triggered, this, &AGoNCharacter::InputMove);
		Input->BindAction(Set->Move, ETriggerEvent::Completed, this, &AGoNCharacter::InputMove);
	}
	if (Set->Look != nullptr)
	{
		Input->BindAction(Set->Look, ETriggerEvent::Triggered, this, &AGoNCharacter::InputLook);
	}
	Bind(Set->Jump, ETriggerEvent::Started, this, &AGoNCharacter::InputJump);
	Bind(Set->Sprint, ETriggerEvent::Started, this, &AGoNCharacter::InputSprintStarted);
	Bind(Set->Sprint, ETriggerEvent::Completed, this, &AGoNCharacter::InputSprintEnded);
	Bind(Set->Walk, ETriggerEvent::Started, this, &AGoNCharacter::InputWalkStarted);
	Bind(Set->Walk, ETriggerEvent::Completed, this, &AGoNCharacter::InputWalkEnded);
	Bind(Set->Crouch, ETriggerEvent::Started, this, &AGoNCharacter::InputCrouch);
	Bind(Set->Prone, ETriggerEvent::Started, this, &AGoNCharacter::InputProne);
	Bind(Set->Interact, ETriggerEvent::Started, this, &AGoNCharacter::InputInteract);
	Bind(Set->ShoulderSwap, ETriggerEvent::Started, this, &AGoNCharacter::SwapShoulder);
}

void AGoNCharacter::InputMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	LastMoveInput = Axis;
	if (Controller == nullptr || IsTraversing() || Axis.IsNearlyZero())
	{
		return;
	}
	const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void AGoNCharacter::InputLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(GetMovementSettings().Camera.bInvertLookY ? -Axis.Y : Axis.Y);
}

void AGoNCharacter::InputJump()
{
	JumpOrTraverse();
}

void AGoNCharacter::InputSprintStarted()
{
	bWantsSprint = true;
}

void AGoNCharacter::InputSprintEnded()
{
	bWantsSprint = false;
}

void AGoNCharacter::InputWalkStarted()
{
	bWantsWalk = true;
}

void AGoNCharacter::InputWalkEnded()
{
	bWantsWalk = false;
}

void AGoNCharacter::InputCrouch()
{
	SetStance(Stance == EGoNStance::Crouch ? EGoNStance::Stand : EGoNStance::Crouch);
}

void AGoNCharacter::InputProne()
{
	SetStance(Stance == EGoNStance::Prone ? EGoNStance::Crouch : EGoNStance::Prone);
}

void AGoNCharacter::InputInteract()
{
	if (CarriedComrade != nullptr)
	{
		PutDownComrade();
		return;
	}
	if (AGoNWoundedComrade* Comrade = FindComradeInReach())
	{
		PickUpComrade(Comrade);
	}
}

void AGoNCharacter::SwapShoulder()
{
	ShoulderSide = -ShoulderSide;
}

// ---- Stance and speed ------------------------------------------------------------------------

bool AGoNCharacter::SetStance(EGoNStance NewStance)
{
	if (NewStance == Stance || IsTraversing())
	{
		return NewStance == Stance;
	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FGoNMovementMetrics& Metrics = GetMovementSettings().Metrics;
	const float OldHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (NewStance == EGoNStance::Stand)
	{
		Move->bWantsToCrouch = false;
		Move->UnCrouch(false);
		if (bIsCrouched)
		{
			Move->bWantsToCrouch = true;
			UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: no room to stand up"));
			return false;
		}
	}
	else
	{
		if (NewStance == EGoNStance::Prone && CarriedComrade != nullptr)
		{
			UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: cannot go prone while carrying"));
			return false;
		}
		// The movement component resizes from the current capsule (crouch <-> prone included).
		const float TargetHalfHeight = NewStance == EGoNStance::Prone ? Metrics.ProneHalfHeight : Metrics.CrouchHalfHeight;
		Move->SetCrouchedHalfHeight(TargetHalfHeight);
		Move->bWantsToCrouch = true;
		Move->Crouch(false);
		const float NewHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		const float Expected = FMath::Max(TargetHalfHeight, GetCapsuleComponent()->GetUnscaledCapsuleRadius());
		if (!bIsCrouched || !FMath::IsNearlyEqual(NewHalfHeight, Expected, 0.5f))
		{
			// Blocked (e.g. prone -> crouch under a low ceiling): restore the previous height.
			Move->SetCrouchedHalfHeight(OldHalfHeight);
			UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: no room for %s"), NewStance == EGoNStance::Prone ? TEXT("prone") : TEXT("crouch"));
			return false;
		}
	}
	Stance = NewStance;
	return true;
}

EGoNGait AGoNCharacter::GetGait() const
{
	const GoN::Character::EGait Requested = bWantsSprint ? GoN::Character::EGait::Sprint : (bWantsWalk ? GoN::Character::EGait::Walk : GoN::Character::EGait::Jog);
	switch (GoN::Character::ResolveGait(Requested, GoNCharacterDetail::ToPure(Stance), IsCarrying()))
	{
	case GoN::Character::EGait::Walk: return EGoNGait::Walk;
	case GoN::Character::EGait::Jog: return EGoNGait::Jog;
	case GoN::Character::EGait::Sprint: return EGoNGait::Sprint;
	}
	return EGoNGait::Jog;
}

float AGoNCharacter::GetDesiredMaxSpeed() const
{
	const GoN::Character::EGait Requested = bWantsSprint ? GoN::Character::EGait::Sprint : (bWantsWalk ? GoN::Character::EGait::Walk : GoN::Character::EGait::Jog);
	return static_cast<float>(GoN::Character::MaxSpeed(GetMetrics(), GoNCharacterDetail::ToPure(Stance), Requested, IsCarrying()));
}

bool AGoNCharacter::CanJumpInternal_Implementation() const
{
	return Stance == EGoNStance::Stand && !IsCarrying() && !IsTraversing() && Super::CanJumpInternal_Implementation();
}

void AGoNCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AGoNCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	Stance = EGoNStance::Stand;
}

// ---- Vault / climb -----------------------------------------------------------------------------

bool AGoNCharacter::IsTraversing() const
{
	const UGoNCharacterMovementComponent* Move = Cast<UGoNCharacterMovementComponent>(GetCharacterMovement());
	return Move != nullptr && Move->IsTraversing();
}

float AGoNCharacter::GetTraversalAlpha() const
{
	const UGoNCharacterMovementComponent* Move = Cast<UGoNCharacterMovementComponent>(GetCharacterMovement());
	return Move != nullptr ? Move->GetTraversalAlpha() : 0.0f;
}

FVector AGoNCharacter::GetFeetLocation() const
{
	return GetActorLocation() - FVector(0.0, 0.0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

GoN::Character::FTraverseDecision AGoNCharacter::ProbeTraverse(GoN::Character::FTraverseProbe& OutProbe, FGoNTraverseDebug& OutDebug) const
{
	using namespace GoN::Character;
	const FMovementMetrics Metrics = GetMetrics();
	UWorld* World = GetWorld();
	OutProbe = FTraverseProbe();
	OutDebug = FGoNTraverseDebug();
	OutProbe.Stance = GoNCharacterDetail::ToPure(Stance);
	OutProbe.bCarrying = IsCarrying();
	OutProbe.bFalling = GetCharacterMovement()->IsFalling();
	if (World == nullptr)
	{
		return DecideTraverse(OutProbe, Metrics);
	}

	// Direction: the stick if pushed, else where the character faces.
	FVector Forward = GetActorForwardVector();
	if (!LastMoveInput.IsNearlyZero() && Controller != nullptr)
	{
		const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
		Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * LastMoveInput.Y + FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * LastMoveInput.X;
	}
	Forward = Forward.GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = GetActorForwardVector().GetSafeNormal2D();
	}

	const FCollisionQueryParams Params = GoNCharacterDetail::QueryParams(*this, CarriedComrade);
	const FVector Feet = GetFeetLocation();
	const FVector Centre = GetActorLocation();
	const double Radius = Metrics.CapsuleRadius;
	const double Reach = Metrics.ReachDistance + Radius;
	const double HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// 1) Forward line traces every 10 cm from just above a step to above max climb: the lowest
	//    hit is the face; the first clear trace above the blocked run bounds the top from above.
	const double ScanFrom = Metrics.MaxStepHeight + 5.0;
	const double ScanTo = Metrics.MaxClimbHeight + 60.0;
	bool bBlockedRun = false;
	double ClearAbove = -1.0;
	FHitResult FaceHit;
	for (double Height = ScanFrom; Height <= ScanTo; Height += 10.0)
	{
		const FVector Start(Centre.X, Centre.Y, Feet.Z + Height);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, Start + Forward * Reach, ECC_Visibility, Params) && FMath::Abs(Hit.ImpactNormal.Z) < 0.5;
		if (bHit && !bBlockedRun)
		{
			bBlockedRun = true;
			FaceHit = Hit;
		}
		else if (!bHit && bBlockedRun)
		{
			ClearAbove = Height;
			break;
		}
	}
	if (!bBlockedRun)
	{
		OutDebug.Decision = UTF8_TO_TCHAR(DecideTraverse(OutProbe, Metrics).ToString().c_str());
		return DecideTraverse(OutProbe, Metrics);
	}
	OutProbe.bObstacle = true;
	OutProbe.ObstacleDistance = FMath::Max(0.0, FVector::Dist2D(Centre, FaceHit.ImpactPoint) - Radius);
	OutDebug.FacePoint = FaceHit.ImpactPoint;
	const FVector FaceXY(FaceHit.ImpactPoint.X, FaceHit.ImpactPoint.Y, Feet.Z);

	// 2) Top: down from just above the first clear height (brief rule 9: traces start close
	//    above the point). Nothing clear up to the scan limit = taller than climbable.
	if (ClearAbove < 0.0)
	{
		OutProbe.bTopFound = true;
		OutProbe.TopHeight = ScanTo;
	}
	else
	{
		FHitResult TopHit;
		const FVector TopStart = FaceXY + Forward * 6.0 + FVector(0.0, 0.0, ClearAbove + 5.0);
		if (World->LineTraceSingleByChannel(TopHit, TopStart, FaceXY + Forward * 6.0 + FVector(0.0, 0.0, Metrics.MaxStepHeight), ECC_Visibility, Params) && GoNCharacterDetail::IsFloorHit(TopHit))
		{
			OutProbe.bTopFound = true;
			OutProbe.TopHeight = TopHit.ImpactPoint.Z - Feet.Z;
			OutDebug.TopPoint = TopHit.ImpactPoint;
		}
	}
	OutDebug.Height = static_cast<float>(OutProbe.TopHeight);
	if (!OutProbe.bTopFound || OutProbe.TopHeight > Metrics.MaxClimbHeight || OutProbe.TopHeight <= Metrics.MaxStepHeight)
	{
		OutProbe.bHeadroomClear = true;
		const FTraverseDecision Early = DecideTraverse(OutProbe, Metrics);
		OutDebug.Decision = UTF8_TO_TCHAR(Early.ToString().c_str());
		OutDebug.bValid = true;
		return Early;
	}
	const double TopZ = Feet.Z + OutProbe.TopHeight;

	// 3) Depth: is there still top surface beyond the vault depth? If not, find the far face.
	{
		FHitResult Beyond;
		const FVector BeyondXY = FaceXY + Forward * (Metrics.MaxVaultDepth + 10.0);
		const bool bTopContinues = World->LineTraceSingleByChannel(Beyond, FVector(BeyondXY.X, BeyondXY.Y, TopZ + 5.0), FVector(BeyondXY.X, BeyondXY.Y, TopZ - 5.0), ECC_Visibility, Params);
		if (bTopContinues)
		{
			OutProbe.Depth = 1.0e6;
		}
		else
		{
			FHitResult FarFace;
			const FVector BackStart = FVector(BeyondXY.X, BeyondXY.Y, TopZ - 5.0) + Forward * 30.0;
			if (World->LineTraceSingleByChannel(FarFace, BackStart, FVector(FaceXY.X, FaceXY.Y, TopZ - 5.0), ECC_Visibility, Params))
			{
				OutProbe.Depth = FVector::Dist2D(FarFace.ImpactPoint, FaceXY);
			}
			else
			{
				OutProbe.Depth = Metrics.MaxVaultDepth;
			}
		}
		OutDebug.Depth = static_cast<float>(FMath::Min(OutProbe.Depth, 9999.0));
	}

	const FCollisionShape StandShape = FCollisionShape::MakeCapsule(Radius, Metrics.StandHalfHeight);
	const FCollisionShape CrouchShape = FCollisionShape::MakeCapsule(Radius, HalfHeightFor(Metrics, EStance::Crouch));

	// 4) Landing behind a thin obstacle.
	if (OutProbe.Depth <= Metrics.MaxVaultDepth)
	{
		const FVector LandXY = FaceXY + Forward * (OutProbe.Depth + Radius + 10.0);
		FHitResult Landing;
		if (World->LineTraceSingleByChannel(Landing, FVector(LandXY.X, LandXY.Y, TopZ + 5.0), FVector(LandXY.X, LandXY.Y, Feet.Z - 150.0), ECC_Visibility, Params) && GoNCharacterDetail::IsFloorHit(Landing))
		{
			OutProbe.bLandingFound = true;
			OutProbe.LandingHeight = Landing.ImpactPoint.Z - Feet.Z;
			OutDebug.LandingPoint = Landing.ImpactPoint;
			OutProbe.bLandingClear = !World->OverlapBlockingTestByChannel(Landing.ImpactPoint + FVector(0.0, 0.0, HalfHeight + 2.0), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
		}
	}

	// 5) Room on top and headroom along the way up.
	const FVector OnTopFeet = FVector(FaceXY.X, FaceXY.Y, TopZ) + Forward * (Radius + 5.0);
	OutProbe.bRoomStandingOnTop = !World->OverlapBlockingTestByChannel(OnTopFeet + FVector(0.0, 0.0, Metrics.StandHalfHeight + 2.0), FQuat::Identity, ECC_Pawn, StandShape, Params);
	OutProbe.bRoomCrouchedOnTop = OutProbe.bRoomStandingOnTop || !World->OverlapBlockingTestByChannel(OnTopFeet + FVector(0.0, 0.0, HalfHeightFor(Metrics, EStance::Crouch) + 2.0), FQuat::Identity, ECC_Pawn, CrouchShape, Params);
	{
		const FCollisionShape Probe = FCollisionShape::MakeSphere(Radius * 0.8);
		const FVector Raised(Centre.X, Centre.Y, TopZ + Metrics.VaultClearance + HalfHeight);
		FHitResult Blocker;
		const bool bUpBlocked = World->SweepSingleByChannel(Blocker, Centre, Raised, FQuat::Identity, ECC_Pawn, Probe, Params);
		const bool bOverBlocked = !bUpBlocked && World->SweepSingleByChannel(Blocker, Raised, Raised + Forward * (OutProbe.ObstacleDistance + Radius), FQuat::Identity, ECC_Pawn, Probe, Params);
		OutProbe.bHeadroomClear = !bUpBlocked && !bOverBlocked;
	}

	const FTraverseDecision Decision = DecideTraverse(OutProbe, Metrics);
	OutDebug.Decision = UTF8_TO_TCHAR(Decision.ToString().c_str());
	OutDebug.bValid = true;
	return Decision;
}

bool AGoNCharacter::TryStartTraversal()
{
	using namespace GoN::Character;
	FTraverseProbe Probe;
	const FTraverseDecision Decision = ProbeTraverse(Probe, LastTraverseDebug);
	if (Decision.Action == ETraverseAction::Refused)
	{
		UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: %s"), *LastTraverseDebug.Decision);
		return true; // consumed: do not jump into a wall we refused to climb
	}
	if (Decision.Action == ETraverseAction::None)
	{
		return false;
	}
	UGoNCharacterMovementComponent* Move = Cast<UGoNCharacterMovementComponent>(GetCharacterMovement());
	if (Move == nullptr)
	{
		return false;
	}
	const FVector Forward = LastTraverseDebug.FacePoint.IsZero() ? GetActorForwardVector() : (LastTraverseDebug.FacePoint - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(Forward.Rotation());
	ActiveTraversal = Decision;
	PendingStanceAfterTraversal = GoNCharacterDetail::FromPure(Decision.Action == ETraverseAction::Mantle ? Decision.EndStance : Probe.Stance);
	Move->StartTraversal(GetActorLocation(), Forward, BuildTraversePath(Decision, Probe, GetMetrics()), static_cast<float>(Decision.Duration));
	UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: %s"), *LastTraverseDebug.Decision);
	return true;
}

void AGoNCharacter::FinishTraversalIfDone()
{
	if (ActiveTraversal.Action == GoN::Character::ETraverseAction::None || IsTraversing())
	{
		return;
	}
	ActiveTraversal = GoN::Character::FTraverseDecision();
	if (PendingStanceAfterTraversal != Stance)
	{
		SetStance(PendingStanceAfterTraversal);
	}
}

void AGoNCharacter::JumpOrTraverse()
{
	if (IsTraversing())
	{
		return;
	}
	if (TryStartTraversal())
	{
		return;
	}
	switch (Stance)
	{
	case EGoNStance::Stand:
		Jump();
		break;
	case EGoNStance::Crouch:
		SetStance(EGoNStance::Stand);
		break;
	case EGoNStance::Prone:
		SetStance(EGoNStance::Crouch);
		break;
	}
}

// ---- Carrying -----------------------------------------------------------------------------------

AGoNWoundedComrade* AGoNCharacter::FindComradeInReach() const
{
	AGoNWoundedComrade* Best = nullptr;
	double BestDistance = GetMovementSettings().InteractDistance;
	for (TActorIterator<AGoNWoundedComrade> It(GetWorld()); It; ++It)
	{
		if (It->GetCarrier() != nullptr)
		{
			continue;
		}
		const double Distance = FVector::Dist(It->GetActorLocation(), GetFeetLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

bool AGoNCharacter::PickUpComrade(AGoNWoundedComrade* Comrade)
{
	if (Comrade == nullptr || CarriedComrade != nullptr || Comrade->GetCarrier() != nullptr || IsTraversing())
	{
		return false;
	}
	if (Stance != EGoNStance::Stand && !SetStance(EGoNStance::Stand))
	{
		return false;
	}
	CarriedComrade = Comrade;
	Comrade->SetCarried(this);
	UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: carrying %s (no sprint, no vault/climb)"), *Comrade->GetName());
	return true;
}

bool AGoNCharacter::PutDownComrade()
{
	if (CarriedComrade == nullptr)
	{
		return false;
	}
	AGoNWoundedComrade* Comrade = CarriedComrade;
	CarriedComrade = nullptr;
	Comrade->SetCarried(nullptr);
	Comrade->PlaceOnGroundNear(GetFeetLocation() + GetActorForwardVector() * 90.0, GetActorRotation().Yaw + 90.0, this);
	UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: put down %s"), *Comrade->GetName());
	return true;
}

// ---- Per frame ----------------------------------------------------------------------------------

FRotator AGoNCharacter::GetLookRotation() const
{
	return Controller != nullptr ? Controller->GetControlRotation() : GetActorRotation();
}

void AGoNCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FinishTraversalIfDone();
	UpdateCover();
	UpdateTurnInPlace(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	if (GoNCharacterDetail::CVarCharDebug.GetValueOnGameThread() > 0 && IsPlayerControlled())
	{
		if (!IsTraversing())
		{
			GoN::Character::FTraverseProbe Probe;
			ProbeTraverse(Probe, LastTraverseDebug);
		}
		DrawDebugWorld();
	}
}

void AGoNCharacter::UpdateCover()
{
	// Low cover while crouched, high cover while standing, right in front: camera cover offset.
	UWorld* World = GetWorld();
	if (World == nullptr || Stance == EGoNStance::Prone || IsTraversing())
	{
		bInCover = false;
		return;
	}
	const FGoNCameraProfile& Camera = GetMovementSettings().Camera;
	const FCollisionQueryParams Params = GoNCharacterDetail::QueryParams(*this, CarriedComrade);
	const FVector Feet = GetFeetLocation();
	const FVector Forward = GetActorForwardVector();
	const double Reach = Camera.CoverProbeDistance + GetCapsuleComponent()->GetScaledCapsuleRadius();
	const auto Blocked = [World, &Params, &Feet, &Forward, Reach](double Height)
	{
		FHitResult Hit;
		const FVector Start = Feet + FVector(0.0, 0.0, Height);
		return World->LineTraceSingleByChannel(Hit, Start, Start + Forward * Reach, ECC_Visibility, Params);
	};
	bInCover = Stance == EGoNStance::Crouch ? (Blocked(60.0) && !Blocked(150.0)) : Blocked(170.0);
}

void AGoNCharacter::UpdateTurnInPlace(float DeltaSeconds)
{
	// Idle and the camera far behind the body's facing: turn the capsule (feet step, anim).
	if (Controller == nullptr || IsTraversing() || GetVelocity().Size2D() > 10.0)
	{
		bTurningInPlace = false;
		return;
	}
	const FGoNAnimSettings& Anim = GetMovementSettings().Animation;
	const float Delta = FRotator::NormalizeAxis(GetLookRotation().Yaw - GetActorRotation().Yaw);
	if (FMath::Abs(Delta) > Anim.TurnInPlaceThreshold)
	{
		bTurningInPlace = true;
	}
	if (!bTurningInPlace)
	{
		return;
	}
	const float Step = FMath::Clamp(Delta, -Anim.TurnInPlaceSpeed * DeltaSeconds, Anim.TurnInPlaceSpeed * DeltaSeconds);
	AddActorWorldRotation(FRotator(0.0, Step, 0.0));
	bTurningInPlace = FMath::Abs(Delta - Step) > 5.0f;
}

void AGoNCharacter::UpdateCamera(float DeltaSeconds)
{
	const FGoNCameraProfile& Profile = GetMovementSettings().Camera;
	const GoN::Character::FCameraProfile Pure = Profile.ToPure();
	ShoulderSideSmoothed = static_cast<float>(GoN::Character::InterpTo(ShoulderSideSmoothed, ShoulderSide, DeltaSeconds, Pure.ShoulderSwapSpeed));
	const bool bSprinting = GetGait() == EGoNGait::Sprint && GetVelocity().Size2D() > GetMetrics().JogSpeed;
	const GoN::Character::FCameraRig Rig = GoN::Character::ComputeCameraRig(Pure, GoNCharacterDetail::ToPure(Stance), bInCover, bSprinting, ShoulderSideSmoothed);

	// Pivot above the feet, so the capsule resize on crouch/prone does not pop the camera.
	if (PivotHeightSmoothed < 0.0f)
	{
		PivotHeightSmoothed = static_cast<float>(Rig.PivotHeight);
	}
	PivotHeightSmoothed = static_cast<float>(GoN::Character::InterpTo(PivotHeightSmoothed, Rig.PivotHeight, DeltaSeconds, Pure.InterpSpeed));
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	CameraBoom->TargetOffset = FVector(0.0, 0.0, PivotHeightSmoothed - HalfHeight);
	CameraBoom->SocketOffset = FVector(0.0, Rig.SideOffset, 0.0);
	CameraBoom->TargetArmLength = static_cast<float>(GoN::Character::InterpTo(CameraBoom->TargetArmLength, Rig.ArmLength, DeltaSeconds, Pure.InterpSpeed));
	FollowCamera->SetFieldOfView(static_cast<float>(GoN::Character::InterpTo(FollowCamera->FieldOfView, Rig.Fov, DeltaSeconds, Pure.InterpSpeed)));
}

// ---- Debug ---------------------------------------------------------------------------------------

FString AGoNCharacter::DescribeState() const
{
	static const TCHAR* const StanceNames[] = {TEXT("stand"), TEXT("crouch"), TEXT("prone")};
	static const TCHAR* const GaitNames[] = {TEXT("walk"), TEXT("jog"), TEXT("sprint")};
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	return FString::Printf(TEXT("%s %s%s%s%s | speed %.0f / max %.0f cm/s | capsule r %.0f hh %.0f | %s"),
		StanceNames[static_cast<int32>(Stance)],
		GaitNames[static_cast<int32>(GetGait())],
		IsCarrying() ? TEXT(" carrying") : TEXT(""),
		bInCover ? TEXT(" in-cover") : TEXT(""),
		IsTraversing() ? TEXT(" traversing") : (GetCharacterMovement()->IsFalling() ? TEXT(" falling") : TEXT("")),
		GetVelocity().Size2D(), GetDesiredMaxSpeed(),
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
		*GetCharacterMovement()->GetMovementName());
}

void AGoNCharacter::DrawDebugOverlay(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (Canvas == nullptr || GoNCharacterDetail::CVarCharDebug.GetValueOnGameThread() <= 0 || !IsPlayerControlled() || (PlayerController != nullptr && PlayerController->GetPawn() != this))
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	Canvas->SetDrawColor(FColor::Cyan);
	Canvas->DrawText(Font, TEXT("GoN Character: ") + DescribeState(), 20.0f, 80.0f);
	const FGoNTraverseDebug& Probe = LastTraverseDebug;
	Canvas->DrawText(Font, FString::Printf(TEXT("  probe: %s | height %.0f depth %.0f"), *Probe.Decision, Probe.Height, Probe.Depth), 20.0f, 96.0f);
}

void AGoNCharacter::DrawDebugWorld() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	DrawDebugCapsule(World, GetActorLocation(), Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(), FQuat::Identity, bInCover ? FColor::Green : FColor::White, false, -1.0f, 0, 1.0f);
	const FGoNTraverseDebug& Probe = LastTraverseDebug;
	if (!Probe.bValid && Probe.FacePoint.IsZero())
	{
		return;
	}
	const FColor Colour = Probe.Decision.StartsWith(TEXT("vault")) ? FColor::Green : (Probe.Decision.StartsWith(TEXT("climb")) ? FColor::Cyan : FColor::Red);
	DrawDebugPoint(World, Probe.FacePoint, 10.0f, Colour, false, -1.0f);
	DrawDebugLine(World, GetFeetLocation() + FVector(0.0, 0.0, 5.0), Probe.FacePoint, Colour, false, -1.0f, 0, 1.0f);
	if (!Probe.TopPoint.IsZero())
	{
		DrawDebugLine(World, Probe.TopPoint + FVector(0.0, 0.0, 40.0), Probe.TopPoint, Colour, false, -1.0f, 0, 2.0f);
		DrawDebugString(World, Probe.TopPoint + FVector(0.0, 0.0, 50.0), Probe.Decision, nullptr, Colour, 0.0f, true);
	}
	if (!Probe.LandingPoint.IsZero())
	{
		DrawDebugSphere(World, Probe.LandingPoint, 12.0f, 8, Colour, false, -1.0f);
	}
}
