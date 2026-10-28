// AAA City Weapons - Tactical Squad Member Character Implementation
#include "AAASquadMemberCharacter.h"
#include "AAARifleAnimInstance.h"
#include "AAASquadManagerComponent.h"
#include "AAAWeaponComponent.h"
#include "AAAPoliceOfficer.h"
#include "AAATrainingTarget.h"
#include "AAAShapes.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float SQUAD_CHEST = 35.f;
	constexpr float SQUAD_EYES = 62.f;
}

AAAASquadMemberCharacter::AAAASquadMemberCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* CM = GetCharacterMovement())
	{
		CM->bOrientRotationToMovement = false;
		CM->bUseControllerDesiredRotation = false;
		CM->MaxWalkSpeed = MoveRunSpeed;
		CM->MaxAcceleration = 1800.f;
		CM->BrakingDecelerationWalking = 1600.f;
		CM->bRunPhysicsWithNoController = true;
	}

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -92.0), FRotator(0.0, -90.0, 0.0));
}

void AAAASquadMemberCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	FacingYaw = GetActorRotation().Yaw;
	BornTimer = 0.f;
	bGearBuilt = false;

	// Load realistic gunshot sound
	USoundBase* LoadedShot = LoadObject<USoundBase>(nullptr, TEXT("/Game/AAACity/Audio/SFX_GunShot_Real.SFX_GunShot_Real"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!LoadedShot)
	{
		LoadedShot = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VRSandbox/Audio/SFX_Laser_01.SFX_Laser_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	ShotSound.Reset(LoadedShot);

	// Load Mannequin mesh
	USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!M) M = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (M)
	{
		GetMesh()->SetSkeletalMeshAsset(M);
		UPhysicsAsset* PA = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (PA) GetMesh()->SetPhysicsAsset(PA, true);
		GetMesh()->SetAnimInstanceClass(UAAARifleAnimInstance::StaticClass());
		Anim = Cast<UAAARifleAnimInstance>(GetMesh()->GetAnimInstance());
		// performance: off-screen men only tick montages (no pose evaluation / IK), far away ones update the pose at a
		// lower rate (URO, interpolated). The ragdoll switches back to AlwaysTickPoseAndRefreshBones.
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		GetMesh()->bEnableUpdateRateOptimizations = true;
		GetMesh()->RefreshBoneTransforms();
		GetMesh()->UpdateComponentToWorld();
	}

	// Configure role-specific parameters & distinctive uniform tint
	FLinearColor UniformColor(0.08f, 0.08f, 0.09f);
	switch (SquadRole)
	{
	case EAAASquadRole::Assault:
		Callsign = TEXT("Bravo-1 (Assault)");
		MaxHealth = 140.f;
		BulletDamage = 26.f;
		FireRate = 10.5f;
		MagazineSize = 30;
		MoveRunSpeed = 530.f;
		MoveWalkSpeed = 230.f;
		UniformColor = FLinearColor(0.06f, 0.09f, 0.14f); // Tactical Navy Spec-Ops
		break;
	case EAAASquadRole::Support:
		Callsign = TEXT("Bravo-2 (Heavy LMG)");
		MaxHealth = 175.f;
		BulletDamage = 24.f;
		FireRate = 13.0f; // Heavy suppression
		MagazineSize = 100;
		MoveRunSpeed = 480.f;
		MoveWalkSpeed = 215.f;
		UniformColor = FLinearColor(0.14f, 0.11f, 0.06f); // Desert Coyote Heavy Armor
		break;
	case EAAASquadRole::Marksman:
		Callsign = TEXT("Bravo-3 (Marksman)");
		MaxHealth = 125.f;
		BulletDamage = 55.f; // Precision DMR power
		FireRate = 3.8f;
		MagazineSize = 15;
		MoveRunSpeed = 510.f;
		MoveWalkSpeed = 225.f;
		UniformColor = FLinearColor(0.05f, 0.11f, 0.06f); // Ranger Olive Spec-Ops
		break;
	default:
		break;
	}

	Health = MaxHealth;
	CurrentAmmo = MagazineSize;
	if (UCharacterMovementComponent* CM = GetCharacterMovement())
	{
		CM->MaxWalkSpeed = MoveRunSpeed;
	}

	// Apply uniform tint to character materials
	for (const TCHAR* P : { TEXT("Tint"), TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Paint Tint"), TEXT("Albedo Tint") })
	{
		GetMesh()->SetVectorParameterValueOnMaterials(FName(P), FVector(UniformColor.R, UniformColor.G, UniformColor.B));
	}
}

/* ============================================================================================ Calibrated Gear & Weapon */

void AAAASquadMemberCharacter::BuildGear()
{
	if (bGearBuilt || !Anim || !Anim->bCalibrated) return;
	bGearBuilt = true;

	USkeletalMeshComponent* M = GetMesh();
	if (!M) return;

	Rifle = NewObject<USceneComponent>(this, TEXT("SquadRifle"));
	Rifle->SetMobility(EComponentMobility::Movable);
	Rifle->SetupAttachment(M, TEXT("hand_r"));
	Rifle->SetRelativeTransform(Anim->GripRel);
	Rifle->RegisterComponent();

	UMaterialInstanceDynamic* Metal = AAAShapes::Mat(this, FLinearColor(0.03f, 0.032f, 0.038f));
	UMaterialInstanceDynamic* Poly = AAAShapes::Mat(this, (SquadRole == EAAASquadRole::Support)
		? FLinearColor(0.08f, 0.07f, 0.045f)
		: ((SquadRole == EAAASquadRole::Marksman) ? FLinearColor(0.045f, 0.065f, 0.045f) : FLinearColor(0.05f, 0.05f, 0.055f)));
	UMaterialInstanceDynamic* OpticGlass = AAAShapes::Mat(this, FLinearColor(0.1f, 0.85f, 0.95f) * 3.0f, true);

	// Calibrated hand-to-hand distance matching UAAARifleAnimInstance::LeftDist
	const double D = FMath::Clamp((double)Anim->LeftDist, 22.0, 48.0);
	const double HgB = D + 10.0;

	auto AddPart = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* Mt) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* P = AAAShapes::Make(this, bCyl, Size, Mt, false, true);
		P->SetMobility(EComponentMobility::Movable);
		P->SetupAttachment(Rifle);
		P->SetRelativeLocationAndRotation(Loc, Rot);
		P->RegisterComponent();
		RifleParts.Add(P);
		return P;
	};

	// Pistol Grip (calibrated at origin of GripRel inside right palm)
	AddPart(false, FVector(-1.0, 0.0, -2.8), FRotator(14.0, 0.0, 0.0), FVector(3.2, 2.8, 10.0), Poly);
	// Buttstock
	AddPart(false, FVector(-14.5, 0.0, 5.5), FRotator::ZeroRotator, FVector(19.0, 3.4, 6.2), Poly);

	if (SquadRole == EAAASquadRole::Support)
	{
		// Heavy M249 Squad Automatic Weapon (SAW)
		AddPart(false, FVector(6.5, 0.0, 6.5), FRotator::ZeroRotator, FVector(28.0, 5.2, 8.8), Metal); // Heavy receiver
		AddPart(false, FVector((18.0 + HgB) * 0.5, 0.0, 6.4), FRotator::ZeroRotator, FVector(HgB - 16.0, 5.8, 6.2), Poly); // Heatshield handguard
		AddPart(true, FVector(HgB + 10.0, 0.0, 6.8), FRotator(90.0, 0.0, 0.0), FVector(2.4, 2.4, 20.0), Metal); // Heavy barrel
		AddPart(true, FVector(HgB + 6.0, 0.0, 4.4), FRotator(90.0, 0.0, 0.0), FVector(1.8, 1.8, 14.0), Metal); // Gas cylinder
		// 100-round green belt box magazine
		UMaterialInstanceDynamic* BoxMat = AAAShapes::Mat(this, FLinearColor(0.04f, 0.09f, 0.04f));
		AddPart(false, FVector(12.0, 0.0, -3.8), FRotator::ZeroRotator, FVector(9.5, 11.5, 11.0), BoxMat);
		// Folded bipod legs
		AddPart(true, FVector(HgB - 4.0, 2.6, 2.8), FRotator(90.0, 0.0, 0.0), FVector(1.1, 1.1, 14.0), Metal);
		AddPart(true, FVector(HgB - 4.0, -2.6, 2.8), FRotator(90.0, 0.0, 0.0), FVector(1.1, 1.1, 14.0), Metal);
		// Holographic sight
		AddPart(false, FVector(4.5, 0.0, 11.8), FRotator::ZeroRotator, FVector(6.5, 3.4, 3.2), Metal);
		MuzzleLocal = FVector(HgB + 21.0, 0.0, 6.8);
	}
	else if (SquadRole == EAAASquadRole::Marksman)
	{
		// Kar98k bolt-action sniper rifle: walnut stock, long blued barrel, 8x scope and a bolt that cycles with the fire clip
		UMaterialInstanceDynamic* Walnut = AAAShapes::Mat(this, FLinearColor(0.16f, 0.085f, 0.04f));
		UMaterialInstanceDynamic* Blued  = AAAShapes::Mat(this, FLinearColor(0.028f, 0.032f, 0.042f));
		UMaterialInstanceDynamic* Steel  = AAAShapes::Mat(this, FLinearColor(0.11f, 0.115f, 0.125f));
		const FRotator Along(90.0, 0.0, 0.0);
		AddPart(false, FVector(-29.0, 0.0, 2.6), FRotator(-5.0, 0.0, 0.0), FVector(24.0, 3.7, 10.5), Walnut);
		AddPart(false, FVector(30.0, 0.0, 5.6), FRotator::ZeroRotator, FVector(38.0, 3.6, 5.2), Walnut);
		AddPart(false, FVector(3.5, 0.0, 8.2), FRotator::ZeroRotator, FVector(19.0, 3.4, 4.0), Blued);
		AddPart(true, FVector(37.0, 0.0, 8.6), Along, FVector(1.9, 1.9, 62.0), Blued);
		AddPart(true, FVector(65.0, 0.0, 8.6), Along, FVector(2.7, 2.7, 3.0), Blued);
		AddPart(true, FVector(5.0, 0.0, 12.9), Along, FVector(3.0, 3.0, 28.0), Blued);
		AddPart(true, FVector(18.5, 0.0, 12.9), Along, FVector(4.5, 4.5, 8.0), Blued);
		AddPart(true, FVector(-9.5, 0.0, 12.9), Along, FVector(3.8, 3.8, 5.0), Blued);
		AddPart(true, FVector(22.6, 0.0, 12.9), Along, FVector(4.0, 4.0, 0.4), OpticGlass);
		AddPart(false, FVector(0.0, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		AddPart(false, FVector(11.5, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		USceneComponent* Bolt = NewObject<USceneComponent>(this, TEXT("SquadKarBolt"));
		Bolt->SetMobility(EComponentMobility::Movable);
		Bolt->SetupAttachment(Rifle);
		Bolt->SetRelativeLocation(FVector(1.0, 0.0, 8.6));
		Bolt->RegisterComponent();
		KarBolt = Bolt;
		auto AddBolt = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* Mt)
		{
			UStaticMeshComponent* P = AddPart(bCyl, Loc, Rot, Size, Mt);
			P->AttachToComponent(Bolt, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
			P->SetRelativeLocationAndRotation(Loc, Rot);
		};
		AddBolt(true, FVector(-4.0, 0.0, 0.0), Along, FVector(1.8, 1.8, 13.0), Steel);
		AddBolt(true, FVector(-9.5, 0.0, 0.0), Along, FVector(2.3, 2.3, 4.0), Blued);
		AddBolt(false, FVector(-2.0, 2.6, -0.9), FRotator::ZeroRotator, FVector(1.1, 4.4, 1.0), Steel);
		AddBolt(false, FVector(-2.0, 5.4, -1.8), FRotator::ZeroRotator, FVector(2.0, 2.0, 2.0), Steel);
		MuzzleLocal = FVector(67.5, 0.0, 8.6);
	}
	else
	{
		// Assault M4A1 Tactical Carbine
		AddPart(false, FVector(6.0, 0.0, 6.5), FRotator::ZeroRotator, FVector(26.0, 4.0, 8.0), Metal);
		AddPart(false, FVector((18.0 + HgB) * 0.5, 0.0, 6.6), FRotator::ZeroRotator, FVector(HgB - 18.0, 4.8, 5.4), Poly);
		AddPart(true, FVector(HgB + 8.0, 0.0, 6.6), FRotator(90.0, 0.0, 0.0), FVector(2.0, 2.0, 16.0), Metal);
		AddPart(false, FVector(12.5, 0.0, -4.6), FRotator(-12.0, 0.0, 0.0), FVector(3.6, 2.6, 14.0), Metal); // 30-rd STANAG mag
		// Red-Dot Optic + PEQ laser box
		AddPart(false, FVector(5.0, 0.0, 11.6), FRotator::ZeroRotator, FVector(6.0, 3.2, 3.0), Metal);
		AddPart(false, FVector(HgB - 5.0, 0.0, 10.0), FRotator::ZeroRotator, FVector(6.0, 3.4, 2.0), Poly);
		MuzzleLocal = FVector(HgB + 17.0, 0.0, 6.6);
	}

	// Muzzle Flash & Dynamic Light
	Flash = AAAShapes::Make(this, false, FVector(13.0, 5.2, 5.2), AAAShapes::Mat(this, FLinearColor(1.f, 0.72f, 0.28f) * 9.f, true), false, false);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetupAttachment(Rifle);
	Flash->SetRelativeLocation(MuzzleLocal + FVector(6.0, 0.0, 0.0));
	Flash->RegisterComponent();
	Flash->SetVisibility(false);

	FlashLight = NewObject<UPointLightComponent>(this);
	FlashLight->SetMobility(EComponentMobility::Movable);
	FlashLight->SetupAttachment(Rifle);
	FlashLight->SetRelativeLocation(MuzzleLocal + FVector(10.0, 0.0, 0.0));
	FlashLight->SetIntensity(0.f);
	FlashLight->SetAttenuationRadius(650.f);
	FlashLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.42f));
	FlashLight->SetCastShadows(false);
	FlashLight->RegisterComponent();

	// Tactical Ops-Core Helmet + Role Visor/Strobe on Head Socket
	const FAttachmentTransformRules NoWeldWorld(EAttachmentRule::KeepWorld, false);
	const FVector HeadLoc = M->GetSocketLocation(TEXT("head"));
	const FLinearColor HelmetCol = (SquadRole == EAAASquadRole::Support)
		? FLinearColor(0.12f, 0.10f, 0.06f)
		: ((SquadRole == EAAASquadRole::Marksman) ? FLinearColor(0.04f, 0.08f, 0.04f) : FLinearColor(0.04f, 0.05f, 0.07f));
	const FLinearColor RoleBadgeCol = (SquadRole == EAAASquadRole::Assault)
		? FLinearColor(0.15f, 0.65f, 1.0f) * 3.f
		: ((SquadRole == EAAASquadRole::Support) ? FLinearColor(1.0f, 0.65f, 0.1f) * 3.f : FLinearColor(0.2f, 1.0f, 0.4f) * 3.f);

	TacticalHelmet = AAAShapes::Make(this, true, FVector(22.5, 22.5, 10.5), AAAShapes::Mat(this, HelmetCol), false, true);
	TacticalHelmet->SetMobility(EComponentMobility::Movable);
	TacticalHelmet->SetWorldLocationAndRotation(HeadLoc + FVector(0.0, 0.0, 12.5), FRotator(0.0, GetActorRotation().Yaw, 0.0));
	TacticalHelmet->RegisterComponent();
	TacticalHelmet->AttachToComponent(M, NoWeldWorld, TEXT("head"));

	// Glowing IFF (Identification Friend or Foe) Tactical Strobe Patch on helmet so leader always spots squadmates
	UStaticMeshComponent* Strobe = AAAShapes::Make(this, false, FVector(6.0, 8.0, 2.2), AAAShapes::Mat(this, RoleBadgeCol, true), false, false);
	Strobe->SetMobility(EComponentMobility::Movable);
	Strobe->SetWorldLocationAndRotation(HeadLoc + FVector(0.0, 0.0, 18.2), FRotator(0.0, GetActorRotation().Yaw, 0.0));
	Strobe->RegisterComponent();
	Strobe->AttachToComponent(M, NoWeldWorld, TEXT("head"));
}

ACharacter* AAAASquadMemberCharacter::PlayerLeader() const
{
	return UGameplayStatics::GetPlayerCharacter(this, 0);
}

FVector AAAASquadMemberCharacter::GetMuzzleLocation() const
{
	if (Rifle && bGearBuilt)
	{
		return (FTransform(MuzzleLocal) * Rifle->GetComponentTransform()).GetLocation();
	}
	return GetActorLocation() + GetActorForwardVector() * 65.f + FVector(0.f, 0.f, SQUAD_CHEST + 10.f);
}

void AAAASquadMemberCharacter::RadioCallout(const FString& Message)
{
	const FString Formatted = FString::Printf(TEXT("[%s] %s"), *Callsign, *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor(100, 220, 255), Formatted);
	}
}

/* ============================================================================================ Commands */

void AAAASquadMemberCharacter::OrderFollowFormation(const FVector& Offset)
{
	if (bIsDowned) return;
	FormationOffset = Offset;
	if (!bHoldOrdered && !bMoveOrdered && CurrentState != EAAASquadState::Reviving && CurrentState != EAAASquadState::Suppressing)
	{
		if (!CurrentTargetActor.IsValid())
		{
			CurrentState = EAAASquadState::Following;
		}
	}
}

void AAAASquadMemberCharacter::OrderMoveTo(const FVector& InTargetLocation, bool bTakeCover)
{
	if (bIsDowned) return;
	bMoveOrdered = true;
	bHoldOrdered = false;
	TargetLocation = InTargetLocation;
	GroundTrace(TargetLocation, TargetLocation);

	const FVector ThreatOrigin = CurrentTargetActor.IsValid()
		? CurrentTargetActor->GetActorLocation()
		: (TargetLocation + GetActorForwardVector() * 1200.f);

	if (bTakeCover)
	{
		FVector FoundCover, FoundPeek;
		if (SampleCoverAround(TargetLocation, ThreatOrigin, FoundCover, FoundPeek))
		{
			CoverPoint = FoundCover;
			PeekPoint = FoundPeek;
			TargetLocation = FoundCover;
			bHasCover = true;
		}
		else
		{
			bHasCover = false;
		}
	}
	else
	{
		bHasCover = false;
	}

	CurrentState = EAAASquadState::MovingToPoint;
	RadioCallout(TEXT("Moving to tactical position!"));
}

void AAAASquadMemberCharacter::OrderAttack(AActor* TargetActor)
{
	if (bIsDowned || !TargetActor) return;
	PlayerOrderedTarget = TargetActor;
	CurrentTargetActor = TargetActor;
	CurrentState = EAAASquadState::Engaging;
	BurstLeft = (SquadRole == EAAASquadRole::Support) ? 14 : ((SquadRole == EAAASquadRole::Marksman) ? 2 : 5);
	BurstPause = 0.f;
	RadioCallout(TEXT("Target designated! Opening fire!"));
}

void AAAASquadMemberCharacter::OrderSuppress(const FVector& TargetArea)
{
	if (bIsDowned) return;
	TargetLocation = TargetArea;
	CurrentState = EAAASquadState::Suppressing;
	SuppressionBurstLeft = (SquadRole == EAAASquadRole::Support) ? 6.0f : 4.0f;
	BurstLeft = 20;
	BurstPause = 0.f;
	RadioCallout(TEXT("Laying heavy suppressive fire!"));
}

void AAAASquadMemberCharacter::OrderHoldPosition()
{
	if (bIsDowned) return;
	bHoldOrdered = true;
	bMoveOrdered = false;
	TargetLocation = GetActorLocation();
	CurrentState = EAAASquadState::Holding;
	if (UCharacterMovementComponent* CM = GetCharacterMovement())
	{
		CM->StopMovementImmediately();
	}
	RadioCallout(TEXT("Holding position & covering sector!"));
}

void AAAASquadMemberCharacter::OrderRegroup()
{
	if (bIsDowned) return;
	bHoldOrdered = false;
	bMoveOrdered = false;
	bHasCover = false;
	PlayerOrderedTarget = nullptr;
	CurrentTargetActor = nullptr;
	CurrentState = EAAASquadState::Following;
	RadioCallout(TEXT("Forming up on you, Lead!"));
}

void AAAASquadMemberCharacter::OrderRevive(AAAASquadMemberCharacter* DownedAlly)
{
	if (bIsDowned || !DownedAlly) return;
	bHoldOrdered = false;
	bMoveOrdered = false;
	AllyToRevive = DownedAlly;
	TargetLocation = DownedAlly->GetActorLocation();
	CurrentState = EAAASquadState::Reviving;
	ReviveProgress = 0.f;
	RadioCallout(FString::Printf(TEXT("Moving to revive %s! Cover me!"), *DownedAlly->Callsign));
}

void AAAASquadMemberCharacter::ReviveFromDowned()
{
	if (!bIsDowned) return;
	bIsDowned = false;
	Health = MaxHealth * 0.70f;
	CurrentState = EAAASquadState::Following;
	if (UCharacterMovementComponent* CM = GetCharacterMovement())
	{
		CM->SetMovementMode(MOVE_Walking);
		CM->MaxWalkSpeed = MoveRunSpeed;
	}
	RadioCallout(TEXT("Back in the fight! Thanks, team!"));
}

/* ============================================================================================ Combat & Damage */

float AAAASquadMemberCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Ignore friendly fire from player or fellow squad members
	if (DamageCauser && (DamageCauser == PlayerLeader() || DamageCauser->IsA(AAAASquadMemberCharacter::StaticClass())))
	{
		return 0.f;
	}

	float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (bIsDowned) return Actual;

	HitTimer = 0.f;
	HitIdx = FMath::RandRange(0, 3);
	if (DamageCauser)
	{
		HitDirDeg = UAAARifleAnimInstance::DirFrom(this, DamageCauser->GetActorLocation());
		// Automatically return fire at hostile attacker if we don't have a target
		if (!CurrentTargetActor.IsValid() && IsValidHostileTarget(DamageCauser))
		{
			CurrentTargetActor = DamageCauser;
		}
	}
	HitStr = FMath::Clamp(Actual / 45.f, 0.2f, 0.9f);

	Health = FMath::Clamp(Health - Actual, 0.f, MaxHealth);
	if (Health <= 0.f)
	{
		bIsDowned = true;
		CurrentState = EAAASquadState::Downed;
		DownedTimer = 60.f;
		if (UCharacterMovementComponent* CM = GetCharacterMovement())
		{
			CM->StopMovementImmediately();
			CM->DisableMovement();
		}
		RadioCallout(TEXT("MAN DOWN! Need immediate revive!"));
	}
	return Actual;
}

/* ============================================================================================ Tick & Animation */

void AAAASquadMemberCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.05f);
	BornTimer += Dt;

	if (!bGearBuilt)
	{
		BuildGear();
	}

	StepTracers(Dt);

	// Recoil spring & muzzle flash decay
	KickV += (-Kick * 260.f - KickV * 26.f) * Dt;
	Kick = FMath::Max(0.f, Kick + KickV * Dt);
	FlashTimer -= Dt;
	const bool bShowFlash = FlashTimer > 0.f && !bIsDowned;
	if (Flash && Flash->IsVisible() != bShowFlash) Flash->SetVisibility(bShowFlash);
	if (FlashLight && (FlashLight->Intensity > 0.f) != bShowFlash) FlashLight->SetIntensity(bShowFlash ? 5000.f : 0.f);
	if (HitTimer >= 0.f)
	{
		HitTimer += Dt;
		if (HitTimer > 1.2f) HitTimer = -1.f;
	}

	if (bIsDowned)
	{
		DownedTimer -= Dt;
		if (Anim)
		{
			Anim->Inputs.LocalVel = FVector2D::ZeroVector;
			Anim->Inputs.bAiming = false;
			Anim->Inputs.AimPitch = -25.f;
		}
		if (DownedTimer <= 0.f)
		{
			if (GetCapsuleComponent())
			{
				GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			for (UStaticMeshComponent* Part : RifleParts)
			{
				if (Part) Part->DestroyComponent(false);
			}
			RifleParts.Empty();
			if (FlashLight) { FlashLight->DestroyComponent(false); FlashLight = nullptr; }
			if (Flash) { Flash->DestroyComponent(false); Flash = nullptr; }
			if (Rifle) { Rifle->DestroyComponent(false); Rifle = nullptr; }
			if (TacticalHelmet) { TacticalHelmet->DestroyComponent(false); TacticalHelmet = nullptr; }

			if (USkeletalMeshComponent* M = GetMesh())
			{
				if (!M->GetPhysicsAsset())
				{
					if (UPhysicsAsset* PA = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"), nullptr, LOAD_NoWarn | LOAD_Quiet))
					{
						M->SetPhysicsAsset(PA, true);
					}
				}
				M->RefreshBoneTransforms();
				M->UpdateComponentToWorld();
				M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				M->SetCollisionProfileName(TEXT("Ragdoll"));
				M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
				M->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				if (!M->GetBodyInstance() || !M->GetBodyInstance()->IsValidBodyInstance()) M->RecreatePhysicsState();
				M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;   // (ragdoll: bones from physics even off-screen)
				M->bEnableUpdateRateOptimizations = false;
				M->SetAllBodiesSimulatePhysics(true);
				M->SetSimulatePhysics(true);
				for (FBodyInstance* BI : M->Bodies) if (BI) { BI->LinearDamping = 0.18f; BI->AngularDamping = 1.4f; BI->UpdateDampingProperties(); }
				M->WakeAllRigidBodies();
			}
			PrimaryActorTick.bCanEverTick = false;
		}
		return;
	}

	ThinkAI(Dt);
	ActMovement(Dt);
	ActCombat(Dt);

	// Update UAAARifleAnimInstance inputs so both hands hold the rifle properly at all times
	if (Anim)
	{
		FAAARifleInputs& I = Anim->Inputs;
		const FVector V = GetVelocity();
		I.LocalVel = FVector2D(FVector::DotProduct(V, GetActorForwardVector()), FVector::DotProduct(V, GetActorRightVector()));

		const bool bActivelyAiming = (CurrentState == EAAASquadState::Engaging || CurrentState == EAAASquadState::Suppressing || CurrentTargetActor.IsValid());
		if (bActivelyAiming)
		{
			const FVector AimAt = (CurrentState == EAAASquadState::Suppressing)
				? TargetLocation
				: (CurrentTargetActor.IsValid() ? GetTargetAimPoint(CurrentTargetActor.Get()) : (GetActorLocation() + GetActorForwardVector() * 500.f));
			const FVector ToAim = AimAt - (GetActorLocation() + FVector(0.f, 0.f, SQUAD_CHEST));
			I.AimPitch = FMath::Clamp(ToAim.Rotation().Pitch, -40.f, 40.f);
		}
		else
		{
			// Tactical low-ready posture while moving/following so both hands grip the calibrated rifle cleanly
			I.AimPitch = -8.f;
		}

		const float EL = Anim->GetEquipLength();
		I.EquipTime = (BornTimer < EL) ? BornTimer : -1.f;
		I.ReloadTime = ReloadTimer;
		I.Kick = Kick;
		if (SniperT >= 0.f)
		{
			SniperT += Dt;
			if (SniperT >= 2.00f) SniperT = -1.f;
		}
		I.SniperFireTime = (SquadRole == EAAASquadRole::Marksman && Rifle && ReloadTimer < 0.f) ? SniperT : -1.f;
		I.bKarRifle = (SquadRole == EAAASquadRole::Marksman);
		if (USceneComponent* Kb = KarBolt.Get())
		{
			const float T = SniperT;
			float Roll = 0.f, Back = 0.f;
			if (T >= 0.f)
			{
				const float Up = FMath::SmoothStep(0.37f, 0.57f, T) * (1.f - FMath::SmoothStep(1.02f, 1.15f, T));
				Roll = -70.f * Up;
				Back = -11.f * FMath::SmoothStep(0.57f, 0.80f, T) * (1.f - FMath::SmoothStep(0.80f, 1.02f, T));
			}
			Kb->SetRelativeLocationAndRotation(FVector(1.0 + Back, 0.0, 8.6), FRotator(0.0, 0.0, Roll));
		}
		I.KickSide = ShotSide;
		I.HitTime = HitTimer;
		I.HitIndex = HitIdx;
		I.HitDir = HitDirDeg;
		I.HitStrength = HitStr;
		I.DeathTime = -1.f;
		I.bAiming = true;       // Keeps rifle posture active in UAAARifleAnimInstance
		I.bLeftHandIK = true;   // Locks left hand onto the handguard
		I.bIsPistol = false;
		I.bUnarmed = false;
		I.bFalling = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	}
}

/* ============================================================================================ Target & Brain Logic */

bool AAAASquadMemberCharacter::IsValidHostileTarget(AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == this || Candidate == PlayerLeader())
	{
		return false;
	}
	if (Candidate->IsA(AAAASquadMemberCharacter::StaticClass()))
	{
		return false;
	}

	// 1. Live Police Officer
	if (const AAAAPoliceOfficer* Officer = Cast<AAAAPoliceOfficer>(Candidate))
	{
		if (Officer->IsDead()) return false;
		// Valid if explicitly ordered by player, or if officer is engaged/hostile
		if (PlayerOrderedTarget.Get() == Candidate) return true;
		return Officer->IsEngaged() || Officer->SeesPlayer();
	}

	// 2. Pop-up Training Target (standing up)
	if (const AAAATrainingTarget* TrainTarget = Cast<AAAATrainingTarget>(Candidate))
	{
		if (TrainTarget->Hinge)
		{
			const float Tilt = FMath::Abs(TrainTarget->Hinge->GetRelativeRotation().Pitch);
			return Tilt < 40.f; // Only target standing plates, not ones already knocked flat
		}
		return true;
	}

	// 3. Explicitly designated target by Player Attack Order
	if (PlayerOrderedTarget.Get() == Candidate)
	{
		if (const ACharacter* CharTarget = Cast<ACharacter>(Candidate))
		{
			return !CharTarget->IsHidden() && (!CharTarget->GetMesh() || !CharTarget->GetMesh()->IsSimulatingPhysics());
		}
		return true;
	}

	return false;
}

FVector AAAASquadMemberCharacter::GetTargetAimPoint(AActor* Target) const
{
	if (!Target) return GetActorLocation() + GetActorForwardVector() * 500.f;
	if (const AAAATrainingTarget* TT = Cast<AAAATrainingTarget>(Target))
	{
		// Aim at center of the target plate (135cm above Hinge)
		return TT->Hinge ? (TT->Hinge->GetComponentLocation() + TT->Hinge->GetUpVector() * 135.f) : (TT->GetActorLocation() + FVector(0.f, 0.f, 130.f));
	}
	if (Target->IsA(ACharacter::StaticClass()))
	{
		// Marksman aims slightly higher (upper chest / neck), Support/Assault aim center mass
		const float HeightOff = (SquadRole == EAAASquadRole::Marksman) ? 48.f : SQUAD_CHEST;
		return Target->GetActorLocation() + FVector(0.f, 0.f, HeightOff);
	}
	return Target->GetActorLocation() + FVector(0.f, 0.f, 35.f);
}

void AAAASquadMemberCharacter::ThinkAI(float Dt)
{
	ShotCD -= Dt;
	BurstPause -= Dt;
	PeekTimer -= Dt;

	// Handle weapon reload completion
	if (ReloadTimer >= 0.f)
	{
		const float RL = Anim ? FMath::Max(Anim->GetReloadLength(), 1.8f) : 2.2f;
		ReloadTimer += Dt;
		if (ReloadTimer >= RL)
		{
			CurrentAmmo = MagazineSize;
			ReloadTimer = -1.f;
		}
	}

	// Validate current target (clear immediately if dead or knocked down)
	if (CurrentTargetActor.IsValid() && !IsValidHostileTarget(CurrentTargetActor.Get()))
	{
		CurrentTargetActor = nullptr;
		if (PlayerOrderedTarget.IsValid() && !IsValidHostileTarget(PlayerOrderedTarget.Get()))
		{
			PlayerOrderedTarget = nullptr;
		}
	}

	// Throttle target scanning to 4 times per second
	ThinkTimer -= Dt;
	if (ThinkTimer > 0.f) return;
	ThinkTimer = FMath::FRandRange(0.20f, 0.28f);

	const EAAASquadRoE RoE = SquadManager.IsValid() ? SquadManager->CurrentRoE : EAAASquadRoE::FireAtWill;
	if (RoE == EAAASquadRoE::HoldFire && !PlayerOrderedTarget.IsValid())
	{
		CurrentTargetActor = nullptr;
		return;
	}

	// Scan for best hostile target if none is locked
	if (!CurrentTargetActor.IsValid() && CurrentState != EAAASquadState::Reviving)
	{
		const FVector EyeLoc = GetActorLocation() + FVector(0.f, 0.f, SQUAD_EYES);
		AActor* BestTarget = nullptr;
		float BestDist = 6000.f;

		// 1. Scan hostile Police Officers
		for (TActorIterator<AAAAPoliceOfficer> It(GetWorld()); It; ++It)
		{
			AAAAPoliceOfficer* Officer = *It;
			if (!IsValidHostileTarget(Officer)) continue;
			const float D = FVector::Dist(GetActorLocation(), Officer->GetActorLocation());
			if (D < BestDist && LineOfSight(EyeLoc, GetTargetAimPoint(Officer), Officer))
			{
				BestDist = D;
				BestTarget = Officer;
			}
		}

		// 2. Scan standing Training Targets (if no live officer is closer)
		if (!BestTarget && RoE == EAAASquadRoE::FireAtWill)
		{
			for (TActorIterator<AAAATrainingTarget> It(GetWorld()); It; ++It)
			{
				AAAATrainingTarget* TT = *It;
				if (!IsValidHostileTarget(TT)) continue;
				const float D = FVector::Dist(GetActorLocation(), TT->GetActorLocation());
				if (D < 3800.f && D < BestDist && LineOfSight(EyeLoc, GetTargetAimPoint(TT), TT))
				{
					BestDist = D;
					BestTarget = TT;
				}
			}
		}

		if (BestTarget)
		{
			CurrentTargetActor = BestTarget;
			BurstLeft = (SquadRole == EAAASquadRole::Support) ? FMath::RandRange(8, 15) : ((SquadRole == EAAASquadRole::Marksman) ? FMath::RandRange(1, 3) : FMath::RandRange(3, 6));
			BurstPause = 0.05f;
		}
	}

	// Update state transitions based on orders & target presence
	if (CurrentState == EAAASquadState::Suppressing)
	{
		if (SuppressionBurstLeft <= 0.f)
		{
			CurrentState = bHoldOrdered ? EAAASquadState::Holding : (bHasCover ? EAAASquadState::InCover : EAAASquadState::Following);
		}
	}
	else if (CurrentState != EAAASquadState::Reviving && CurrentState != EAAASquadState::MovingToPoint)
	{
		if (CurrentTargetActor.IsValid())
		{
			CurrentState = bHasCover ? EAAASquadState::InCover : EAAASquadState::Engaging;
		}
		else if (bHoldOrdered)
		{
			CurrentState = EAAASquadState::Holding;
		}
		else if (bMoveOrdered)
		{
			CurrentState = bHasCover ? EAAASquadState::InCover : EAAASquadState::Holding;
		}
		else
		{
			CurrentState = EAAASquadState::Following;
		}
	}
}

/* ============================================================================================ Movement & Feeler Steering */

void AAAASquadMemberCharacter::ActMovement(float Dt)
{
	ACharacter* Leader = PlayerLeader();
	UCharacterMovementComponent* CM = GetCharacterMovement();
	if (!Leader || !CM) return;

	const FVector MyLoc = GetActorLocation();
	const FVector LeaderLoc = Leader->GetActorLocation();
	FVector DesiredLoc = MyLoc;
	bool bWantsToMove = false;

	if (CurrentState == EAAASquadState::Reviving)
	{
		if (!AllyToRevive.IsValid() || !AllyToRevive->IsDowned())
		{
			AllyToRevive = nullptr;
			CurrentState = EAAASquadState::Following;
			return;
		}
		DesiredLoc = AllyToRevive->GetActorLocation();
		const float DistToAlly = FVector::Dist2D(MyLoc, DesiredLoc);
		if (DistToAlly > 110.f)
		{
			bWantsToMove = true;
		}
		else
		{
			ReviveProgress += Dt;
			if (ReviveProgress >= 2.2f)
			{
				AllyToRevive->ReviveFromDowned();
				AllyToRevive = nullptr;
				CurrentState = EAAASquadState::Following;
			}
		}
	}
	else if (CurrentState == EAAASquadState::MovingToPoint)
	{
		DesiredLoc = TargetLocation;
		if (FVector::Dist2D(MyLoc, DesiredLoc) > 75.f)
		{
			bWantsToMove = true;
		}
		else
		{
			CurrentState = bHasCover ? EAAASquadState::InCover : EAAASquadState::Holding;
		}
	}
	else if (CurrentState == EAAASquadState::InCover && bHasCover)
	{
		// Peek out to shoot when ready, duck back behind cover when reloading or between bursts
		DesiredLoc = (bPeeking && ReloadTimer < 0.f && CurrentTargetActor.IsValid()) ? PeekPoint : CoverPoint;
		if (FVector::Dist2D(MyLoc, DesiredLoc) > 45.f)
		{
			bWantsToMove = true;
		}
	}
	else if (CurrentState == EAAASquadState::Following || (!bHoldOrdered && !bMoveOrdered))
	{
		// Calculate formation world slot relative to Leader's horizontal orientation
		const FRotator LeaderYawRot(0.f, Leader->GetActorRotation().Yaw, 0.f);
		const FVector Fw = LeaderYawRot.Vector();
		const FVector Rt = FRotationMatrix(LeaderYawRot).GetScaledAxis(EAxis::Y);
		DesiredLoc = LeaderLoc + Fw * FormationOffset.X + Rt * FormationOffset.Y;
		GroundTrace(DesiredLoc, DesiredLoc);

		const float DistToSlot = FVector::Dist2D(MyLoc, DesiredLoc);
		if (DistToSlot > 85.f)
		{
			bWantsToMove = true;
		}
	}

	// Personal-space push away from Player Leader (never crowd or block the player!)
	FVector SeparationVec = FVector::ZeroVector;
	const FVector AwayFromLeader = MyLoc - LeaderLoc;
	const float DistToLeader2D = AwayFromLeader.Size2D();
	if (DistToLeader2D < 165.f && DistToLeader2D > 1.f)
	{
		SeparationVec += AwayFromLeader.GetSafeNormal2D() * ((165.f - DistToLeader2D) / 165.f) * 2.2f;
		if (!bWantsToMove && DistToLeader2D < 130.f)
		{
			DesiredLoc = MyLoc + AwayFromLeader.GetSafeNormal2D() * 140.f;
			bWantsToMove = true;
		}
	}

	// Separation between squad members so they never stack on each other
	if (SquadManager.IsValid())
	{
		for (AAAASquadMemberCharacter* Other : SquadManager->SquadMembers)
		{
			if (!Other || Other == this) continue;
			const FVector Diff = MyLoc - Other->GetActorLocation();
			const float D2 = Diff.Size2D();
			if (D2 < 145.f && D2 > 1.f)
			{
				SeparationVec += Diff.GetSafeNormal2D() * ((145.f - D2) / 145.f) * 1.65f;
			}
		}
	}

	if (bWantsToMove)
	{
		const FVector ToGoal = DesiredLoc - MyLoc;
		const float GoalDist = ToGoal.Size2D();
		const FVector GoalDir = ToGoal.GetSafeNormal2D();

		// Speed adaptation: walk smoothly when near slot/cover, run when catching up, sprint if far behind
		float TargetSpeed = MoveRunSpeed;
		if (CurrentState == EAAASquadState::InCover)
		{
			TargetSpeed = MoveWalkSpeed;
		}
		else if (!bMoveOrdered && GoalDist < 240.f)
		{
			const float LeaderSpd = Leader->GetVelocity().Size2D();
			TargetSpeed = FMath::Max(MoveWalkSpeed, LeaderSpd);
		}
		else if (GoalDist > 650.f)
		{
			TargetSpeed = MoveRunSpeed * 1.2f; // Catch up quickly if far behind
		}
		CM->MaxWalkSpeed = FMath::FInterpTo(CM->MaxWalkSpeed, TargetSpeed, Dt, 8.f);

		// 7-angle volumetric sphere-sweep feeler fan for obstacle/vehicle/wall avoidance
		FCollisionQueryParams Q(TEXT("SquadSteer"), false, this);
		Q.AddIgnoredActor(Leader);
		if (SquadManager.IsValid())
		{
			for (AAAASquadMemberCharacter* Other : SquadManager->SquadMembers)
			{
				if (Other) Q.AddIgnoredActor(Other);
			}
		}

		const FVector Waist = MyLoc + FVector(0.f, 0.f, 15.f);
		const float FeelerDist = FMath::Min(GoalDist, 230.f);
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(26.f);
		const float Angles[7] = { 0.f, 28.f, -28.f, 55.f, -55.f, 82.f, -82.f };

		FVector ChosenDir = GoalDir;
		for (int32 i = 0; i < 7; ++i)
		{
			const FVector Cand = GoalDir.RotateAngleAxis(Angles[i], FVector::UpVector);
			FHitResult Hit;
			if (!GetWorld()->SweepSingleByChannel(Hit, Waist, Waist + Cand * FeelerDist, FQuat::Identity, ECC_Visibility, Sphere, Q))
			{
				ChosenDir = Cand;
				break;
			}
			if (i == 0 && Hit.bBlockingHit)
			{
				const FVector Slide = FVector::VectorPlaneProject(GoalDir, Hit.ImpactNormal).GetSafeNormal2D();
				if (!Slide.IsNearlyZero()) ChosenDir = Slide;
			}
		}

		const FVector RawSteer = (ChosenDir + SeparationVec).GetSafeNormal2D();
		if (SmoothMoveDir.IsNearlyZero()) SmoothMoveDir = RawSteer;
		else SmoothMoveDir = FMath::VInterpNormalRotationTo(SmoothMoveDir, RawSteer, Dt, 380.f);

		AddMovementInput(SmoothMoveDir, 1.0f);

		if (GetVelocity().Size2D() < 25.f)
		{
			StuckTimer += Dt;
			if (StuckTimer > 1.1f)
			{
				// Unstick side-step
				SmoothMoveDir = FVector(-GoalDir.Y, GoalDir.X, 0.f);
				StuckTimer = 0.f;
			}
		}
		else
		{
			StuckTimer = 0.f;
		}
	}
	else
	{
		SmoothMoveDir = FVector::ZeroVector;
	}

	// Smooth Tactical Yaw Rotation:
	// - When engaging or suppressing: face the target / suppression zone
	// - When moving fast without an immediate shot: face movement direction
	// - When standing in formation: face the same direction the Squad Leader is looking
	float DesiredYaw = FacingYaw;
	if (CurrentState == EAAASquadState::Suppressing)
	{
		DesiredYaw = (TargetLocation - MyLoc).Rotation().Yaw;
	}
	else if (CurrentTargetActor.IsValid())
	{
		DesiredYaw = (GetTargetAimPoint(CurrentTargetActor.Get()) - MyLoc).Rotation().Yaw;
	}
	else if (GetVelocity().Size2D() > 45.f)
	{
		DesiredYaw = GetVelocity().Rotation().Yaw;
	}
	else if (Leader)
	{
		DesiredYaw = Leader->GetControlRotation().Yaw;
	}

	FacingYaw = FMath::FixedTurn(FacingYaw, DesiredYaw, 360.f * Dt);
	SetActorRotation(FRotator(0.f, FacingYaw, 0.f));
}

/* ============================================================================================ Combat, Tracers & Shooting */

void AAAASquadMemberCharacter::ActCombat(float Dt)
{
	if (ReloadTimer >= 0.f) return;

	if (CurrentAmmo <= 0)
	{
		ReloadTimer = 0.f;
		bPeeking = false;
		return;
	}

	// 1. Area Suppression Order
	if (CurrentState == EAAASquadState::Suppressing && SuppressionBurstLeft > 0.f)
	{
		SuppressionBurstLeft -= Dt;
		if (ShotCD <= 0.f)
		{
			const FVector Jitter(FMath::FRandRange(-75.f, 75.f), FMath::FRandRange(-75.f, 75.f), FMath::FRandRange(-25.f, 35.f));
			FireBulletAt(TargetLocation + Jitter);
			ShotCD = (SquadRole == EAAASquadRole::Marksman) ? 2.05f : 1.0f / FMath::Max(FireRate, 1.f);   // Kar98k: bolt cycle (2.00 s clip) before the next round
		}
		return;
	}

	// 2. Active Target Engagement
	if (!CurrentTargetActor.IsValid()) return;

	AActor* Target = CurrentTargetActor.Get();
	const FVector EyeLoc = GetActorLocation() + FVector(0.f, 0.f, SQUAD_EYES);
	const FVector AimPoint = GetTargetAimPoint(Target);
	const bool bCanSee = LineOfSight(EyeLoc, AimPoint, Target);

	// Cover peeking cycle
	if (bHasCover && CurrentState == EAAASquadState::InCover)
	{
		if (PeekTimer <= 0.f)
		{
			bPeeking = !bPeeking;
			PeekTimer = bPeeking ? FMath::FRandRange(1.6f, 2.8f) : FMath::FRandRange(0.6f, 1.1f);
		}
		if (!bPeeking && !bCanSee) return;
	}

	if (!bCanSee) return;

	// Ensure character is roughly facing the target before pulling the trigger
	const float AimYawDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, (AimPoint - GetActorLocation()).Rotation().Yaw));
	if (AimYawDiff > 28.f) return;

	// Burst cadence control
	if (BurstLeft <= 0)
	{
		if (BurstPause <= 0.f)
		{
			BurstLeft = (SquadRole == EAAASquadRole::Support)
				? FMath::RandRange(9, 16)
				: ((SquadRole == EAAASquadRole::Marksman) ? FMath::RandRange(1, 2) : FMath::RandRange(3, 6));
			BurstPause = (SquadRole == EAAASquadRole::Marksman) ? FMath::FRandRange(0.45f, 0.75f) : FMath::FRandRange(0.25f, 0.50f);
		}
		return;
	}

	if (ShotCD <= 0.f && BurstPause <= 0.f)
	{
		// Role-calibrated accuracy cone
		const float SpreadDeg = (SquadRole == EAAASquadRole::Marksman) ? 0.35f : ((SquadRole == EAAASquadRole::Support) ? 2.2f : 1.15f);
		const FVector From = GetMuzzleLocation();
		const FVector PerfectDir = (AimPoint - From).GetSafeNormal();
		const FVector ShotDir = FMath::VRandCone(PerfectDir, FMath::DegreesToRadians(SpreadDeg));
		const float Dist = FVector::Dist(From, AimPoint);

		FireBulletAt(From + ShotDir * (Dist + 200.f));
		--BurstLeft;
		ShotCD = (SquadRole == EAAASquadRole::Marksman) ? 2.05f : 1.0f / FMath::Max(FireRate, 1.f);   // Kar98k: bolt cycle (2.00 s clip) before the next round
	}
}

void AAAASquadMemberCharacter::FireBulletAt(const FVector& TargetPos)
{
	--CurrentAmmo;
	const FVector From = GetMuzzleLocation();
	const FVector Dir = (TargetPos - From).GetSafeNormal();
	const FVector TraceEnd = From + Dir * 9000.f;

	// Visual muzzle flash & recoil kick
	FlashTimer = 0.042f;
	Kick = FMath::Min(1.f, Kick + 0.65f);
	KickV += 7.0f;
	if (SquadRole == EAAASquadRole::Marksman) SniperT = 0.f;   // start the Kar98k fire + bolt-cycle clip
	ShotSide = FMath::FRandRange(-1.f, 1.f);

	// 3D Gunshot Audio
	if (ShotSound.IsValid())
	{
		const float Pitch = (SquadRole == EAAASquadRole::Support) ? FMath::FRandRange(0.84f, 0.90f) : ((SquadRole == EAAASquadRole::Marksman) ? FMath::FRandRange(0.76f, 0.82f) : FMath::FRandRange(0.96f, 1.04f));
		UGameplayStatics::PlaySoundAtLocation(this, ShotSound.Get(), From, 0.65f, Pitch);
	}

	// Line trace ignoring Leader and friendly Squad Members
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("SquadBullet"), true, this);
	if (ACharacter* Leader = PlayerLeader()) Q.AddIgnoredActor(Leader);
	if (SquadManager.IsValid())
	{
		for (AAAASquadMemberCharacter* Ally : SquadManager->SquadMembers)
		{
			if (Ally) Q.AddIgnoredActor(Ally);
		}
	}

	FVector ImpactEnd = TraceEnd;
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, TraceEnd, ECC_Visibility, Q))
	{
		ImpactEnd = Hit.ImpactPoint;
		if (AActor* HitActor = Hit.GetActor())
		{
			if (AAAATrainingTarget* TT = Cast<AAAATrainingTarget>(HitActor))
			{
				TT->OnHit(Hit.ImpactPoint, Dir);
			}
			else if (HitActor != this && HitActor != PlayerLeader() && !HitActor->IsA(AAAASquadMemberCharacter::StaticClass()))
			{
				FPointDamageEvent Dmg(BulletDamage, Hit, Dir, nullptr);
				HitActor->TakeDamage(BulletDamage, Dmg, GetController(), this);

				// Apply heavy tactical suppression to enemy Police Officers
				if (AAAAPoliceOfficer* Officer = Cast<AAAAPoliceOfficer>(HitActor))
				{
					Officer->Suppress(SquadRole == EAAASquadRole::Support ? 0.55f : 0.30f);
				}
			}
		}
	}

	// Suppress nearby hostile officers whose heads the bullet whizzes past
	for (TActorIterator<AAAAPoliceOfficer> It(GetWorld()); It; ++It)
	{
		AAAAPoliceOfficer* Off = *It;
		if (Off && !Off->IsDead())
		{
			const float PassDist = FMath::PointDistToSegment(Off->GetActorLocation() + FVector(0.f, 0.f, 50.f), From, ImpactEnd);
			if (PassDist < 220.f)
			{
				Off->Suppress(SquadRole == EAAASquadRole::Support ? 0.35f : 0.18f);
			}
		}
	}

	SpawnTracer(From, ImpactEnd);
}

void AAAASquadMemberCharacter::SpawnTracer(const FVector& Start, const FVector& End)
{
	const float TotalDist = FVector::Dist(Start, End);
	if (TotalDist < 60.f) return;

	const FVector Dir = (End - Start).GetSafeNormal();
	const FLinearColor TracerColor = (SquadRole == EAAASquadRole::Support)
		? FLinearColor(1.0f, 0.55f, 0.12f) * 14.f
		: ((SquadRole == EAAASquadRole::Marksman) ? FLinearColor(0.35f, 1.0f, 0.45f) * 14.f : FLinearColor(1.0f, 0.82f, 0.25f) * 12.f);

	UStaticMeshComponent* TracerMesh = AAAShapes::Make(this, false, FVector(55.0, 1.6, 1.6), AAAShapes::Mat(this, TracerColor, true), false, false);
	if (!TracerMesh) return;
	TracerMesh->SetMobility(EComponentMobility::Movable);
	TracerMesh->SetWorldLocationAndRotation(Start + Dir * 30.f, Dir.Rotation());
	TracerMesh->RegisterComponent();

	FSquadBulletTracer T;
	T.Pos = Start;
	T.EndPos = End;
	T.Dir = Dir;
	T.Speed = 24000.f;
	T.DistLeft = TotalDist;
	T.Mesh = TracerMesh;
	ActiveTracers.Add(T);
}

void AAAASquadMemberCharacter::StepTracers(float Dt)
{
	for (int32 i = ActiveTracers.Num() - 1; i >= 0; --i)
	{
		FSquadBulletTracer& T = ActiveTracers[i];
		const float Step = T.Speed * Dt;
		T.Pos += T.Dir * Step;
		T.DistLeft -= Step;

		if (T.DistLeft <= 0.f || !T.Mesh)
		{
			if (T.Mesh) T.Mesh->DestroyComponent();
			ActiveTracers.RemoveAtSwap(i);
		}
		else
		{
			T.Mesh->SetWorldLocation(T.Pos);
		}
	}
}

/* ============================================================================================ Geometry & Cover */

bool AAAASquadMemberCharacter::GroundTrace(const FVector& InP, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("SquadGround"), false, this);
	if (ACharacter* Leader = PlayerLeader()) Q.AddIgnoredActor(Leader);
	if (GetWorld()->LineTraceSingleByChannel(Hit, InP + FVector(0, 0, 120), InP - FVector(0, 0, 420), ECC_WorldStatic, Q))
	{
		OutGround = Hit.Location + FVector(0, 0, 92.f);
		return true;
	}
	OutGround = InP;
	return false;
}

bool AAAASquadMemberCharacter::LineOfSight(const FVector& From, const FVector& To, AActor* IgnoreTarget) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("SquadLoS"), false, this);
	if (ACharacter* Leader = PlayerLeader()) Q.AddIgnoredActor(Leader);
	if (IgnoreTarget) Q.AddIgnoredActor(IgnoreTarget);
	if (SquadManager.IsValid())
	{
		for (AAAASquadMemberCharacter* Ally : SquadManager->SquadMembers)
		{
			if (Ally) Q.AddIgnoredActor(Ally);
		}
	}
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q);
}

bool AAAASquadMemberCharacter::SampleCoverAround(const FVector& SearchOrigin, const FVector& ThreatLoc, FVector& OutCover, FVector& OutPeek)
{
	FCollisionQueryParams Q(TEXT("SquadCover"), false, this);
	if (ACharacter* Leader = PlayerLeader()) Q.AddIgnoredActor(Leader);

	// Radiate 12 directions around SearchOrigin to find chest-high or corner cover against ThreatLoc
	for (int32 i = 0; i < 12; ++i)
	{
		const float Angle = (float)i * 30.f;
		const FVector Rad = FRotator(0.f, Angle, 0.f).Vector();
		FHitResult LowHit, HighHit;

		const FVector EyeStart = SearchOrigin + FVector(0.f, 0.f, SQUAD_EYES);
		const FVector LowStart = SearchOrigin + FVector(0.f, 0.f, SQUAD_CHEST);

		const bool bLowBlocked = GetWorld()->LineTraceSingleByChannel(LowHit, LowStart, LowStart + Rad * 320.f, ECC_WorldStatic, Q);
		const bool bHighClear = !GetWorld()->LineTraceSingleByChannel(HighHit, EyeStart, EyeStart + Rad * 320.f, ECC_WorldStatic, Q);

		if (bLowBlocked)
		{
			OutCover = LowHit.Location - Rad * 62.f;
			GroundTrace(OutCover, OutCover);

			if (bHighClear)
			{
				// Low chest-high cover: stand/peek slightly forward
				OutPeek = OutCover + Rad * 22.f;
			}
			else
			{
				// High wall/vehicle cover: side-step 75cm laterally to peek around corner
				const FVector RightTang(-Rad.Y, Rad.X, 0.f);
				OutPeek = OutCover + RightTang * 75.f;
			}
			GroundTrace(OutPeek, OutPeek);
			return true;
		}
	}
	return false;
}
