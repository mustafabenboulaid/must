#include "AAAPoliceOfficer.h"
#include "AAAGrenade.h"
#include "AAARifleAnimInstance.h"
#include "AAAWeaponComponent.h"
#include "AAAWeaponsSubsystem.h"
#include "AAAShapes.h"
#include "TrafficAIManager.h"

#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float CHEST = 35.f, EYES = 62.f;   // above the capsule centre (cm)
	constexpr float BLEED_TIME = 45.f;           // a downed man of the leader's team bleeds out after this
	constexpr float GETUP_TIME = 1.1f;           // the death clip played backwards: getting up after first aid
}

AAAAPoliceOfficer::AAAAPoliceOfficer()
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
		CM->MaxWalkSpeed = 470.f;
		CM->BrakingDecelerationWalking = 1600.f;
		CM->bRunPhysicsWithNoController = true;
		CM->GetNavAgentPropertiesRef().bCanCrouch = true;
		CM->SetCrouchedHalfHeight(58.f);
		CM->MaxWalkSpeedCrouched = 170.f;
	}
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -92.0), FRotator(0.0, -90.0, 0.0));
}

bool AAAAPoliceOfficer::Hostile(uint8 A, uint8 B, int32 WantedStars)
{
	if (A == B) return false;
	const bool bPolicePlayer = (A == AAATeam::Police && B == AAATeam::Player) || (A == AAATeam::Player && B == AAATeam::Police);
	if (bPolicePlayer) return WantedStars > 0;
	const bool bArmyPlayer = (A == AAATeam::Enemy && B == AAATeam::Player) || (A == AAATeam::Player && B == AAATeam::Enemy);
	return bArmyPlayer;   // police and army leave each other alone
}

void AAAAPoliceOfficer::ApplyTrade()
{
	// the trade of the man (player's men: by slot; army: the same three trades in every group)
	const int32 I = FMath::Max(SquadIndex, 0) % 3;
	Trade = I == 1 ? ETrade::Marksman : (I == 2 ? ETrade::Support : ETrade::Rifleman);
	switch (Trade)
	{
	case ETrade::Rifleman: PrefMin = 900.f;  PrefMax = 2800.f; Aggression = 0.75f; Courage = 0.7f; break;
	case ETrade::Marksman: PrefMin = 2200.f; PrefMax = 5600.f; Aggression = 0.25f; Courage = 0.5f; Skill *= 1.25f; SightRange = 9000.f; break;
	case ETrade::Support:  PrefMin = 1300.f; PrefMax = 3600.f; Aggression = 0.45f; Courage = 0.85f; break;
	}
	if (Team == AAATeam::Enemy) { Aggression *= 0.85f; Courage *= 0.8f; }
	// a little of each man's own temper
	Aggression = FMath::Clamp(Aggression + FMath::FRandRange(-0.1f, 0.1f), 0.05f, 0.95f);
	Courage = FMath::Clamp(Courage + FMath::FRandRange(-0.1f, 0.1f), 0.1f, 1.f);
}

void AAAAPoliceOfficer::BeginPlay()
{
	// the side decides the man: the player's men are tougher and faster (they keep up with a sprinting leader),
	// army soldiers a little less accurate than the police marksmen
	if (Team == AAATeam::Player)
	{
		MaxHealth = 150.f; Skill = 1.1f; RunSpeed = 500.f; WalkSpeed = 220.f; SightRange = 8000.f; Grenades = 2;
		State = EState::Idle;
		ApplyTrade();
	}
	else if (Team == AAATeam::Enemy)
	{
		MaxHealth = 100.f; Skill = 0.9f; RunSpeed = 480.f; WalkSpeed = 160.f; Grenades = 1;
		State = EState::Patrol;
		ApplyTrade();
	}
	Super::BeginPlay();
	Health = MaxHealth;
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BornT = Now = (float)GetWorld()->GetTimeSeconds();
	FacingYaw = GetActorRotation().Yaw;
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
		// uniform (whatever the mannequin material calls its colour): police dark blue, army khaki,
		// the player's men the leader's body in their own colour (dark, so it still reads as cloth)
		FLinearColor Cloth(0.035f, 0.05f, 0.12f);
		if (Team == AAATeam::Enemy) Cloth = FLinearColor(0.16f, 0.15f, 0.075f);
		else if (Team == AAATeam::Player) Cloth = FMath::Lerp(FLinearColor(0.09f, 0.085f, 0.06f), FAAASquadInfo::Color(SquadIndex), 0.42f);
		for (const TCHAR* P : { TEXT("Tint"), TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Paint Tint"), TEXT("Albedo Tint") })
			GetMesh()->SetVectorParameterValueOnMaterials(FName(P), FVector(Cloth.R, Cloth.G, Cloth.B));
	}
	Ammo = 30;
	ShotCD = FMath::FRandRange(0.4f, 1.2f);
	if (Team == AAATeam::Player) { OrderPos = GetActorLocation(); LeashC = GetActorLocation(); }
	if (UAAAWeaponsSubsystem* S = Squad()) S->Register(this);
}

ACharacter* AAAAPoliceOfficer::Player() const { return UGameplayStatics::GetPlayerCharacter(const_cast<AAAAPoliceOfficer*>(this), 0); }

UAAAWeaponComponent* AAAAPoliceOfficer::PlayerWeapon() const
{
	const ACharacter* P = Player();
	return P ? P->FindComponentByClass<UAAAWeaponComponent>() : nullptr;
}

UAAAWeaponsSubsystem* AAAAPoliceOfficer::Squad() const { return GetWorld() ? GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>() : nullptr; }

bool AAAAPoliceOfficer::IsOutChar(const ACharacter* C)
{
	if (!C || !IsValid(C)) return true;
	if (const AAAAPoliceOfficer* O = Cast<AAAAPoliceOfficer>(C)) return O->IsOut();
	if (const UAAAWeaponComponent* W = C->FindComponentByClass<UAAAWeaponComponent>()) return W->IsDead();
	return false;
}

bool AAAAPoliceOfficer::FoeDead() const { return IsOutChar(FoeActor.Get()); }

FVector AAAAPoliceOfficer::ThreatPos() const
{
	if (bSees && FoeActor.IsValid()) return FoeActor->GetActorLocation();
	return LastKnown;
}

/* ============================================================================================ gear */

void AAAAPoliceOfficer::BuildGear()
{
	if (bGear || !Anim || !Anim->bCalibrated) return;
	bGear = true;
	USkeletalMeshComponent* M = GetMesh();
	Rifle = NewObject<USceneComponent>(this, TEXT("OfficerRifle"));
	Rifle->SetMobility(EComponentMobility::Movable);
	Rifle->SetupAttachment(M, TEXT("hand_r"));
	Rifle->SetRelativeTransform(Anim->GripRel);
	Rifle->RegisterComponent();
	UMaterialInstanceDynamic* Metal = AAAShapes::Mat(this, FLinearColor(0.03f, 0.03f, 0.035f));
	UMaterialInstanceDynamic* Poly = AAAShapes::Mat(this, Team == AAATeam::Police ? FLinearColor(0.05f, 0.05f, 0.05f) : FLinearColor(0.16f, 0.09f, 0.04f));   // army / partisans: wooden furniture
	const double D = FMath::Clamp((double)Anim->LeftDist, 22.0, 48.0), HgB = D + 10.0;
	auto Add = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* Mt)
	{
		UStaticMeshComponent* P = AAAShapes::Make(this, bCyl, Size, Mt, false, true);
		P->SetupAttachment(Rifle);
		P->SetRelativeLocationAndRotation(Loc, Rot);
		P->RegisterComponent();
	};
	const bool bKar = UsesKar();   // the marksman carries a Kar98k: walnut stock, long barrel, 8x scope, moving bolt
	if (bKar)
	{
		Ammo = FMath::Min(Ammo, 5);
		UMaterialInstanceDynamic* Walnut = AAAShapes::Mat(this, FLinearColor(0.16f, 0.085f, 0.04f));
		UMaterialInstanceDynamic* Blued  = AAAShapes::Mat(this, FLinearColor(0.028f, 0.032f, 0.042f));
		UMaterialInstanceDynamic* Steel  = AAAShapes::Mat(this, FLinearColor(0.11f, 0.115f, 0.125f));
		UMaterialInstanceDynamic* Glass  = AAAShapes::Mat(this, FLinearColor(0.02f, 0.12f, 0.28f) * 2.0f, true);
		const FRotator Along(90.0, 0.0, 0.0);
		Add(false, FVector(-22.5, 0.0, 2.6), FRotator(-5.0, 0.0, 0.0), FVector(20.0, 3.7, 10.5), Walnut);   // butt: real length of pull, seated in the shoulder by the anim
		Add(false, FVector(-11.0, 0.0, 4.2), FRotator::ZeroRotator, FVector(14.0, 3.9, 6.8), Walnut);
		Add(false, FVector(-1.5, 0.0, 1.0), FRotator(14.0, 0.0, 0.0), FVector(3.8, 3.2, 9.0), Walnut);
		Add(false, FVector(30.0, 0.0, 5.6), FRotator::ZeroRotator, FVector(38.0, 3.6, 5.2), Walnut);
		Add(false, FVector(3.5, 0.0, 8.2), FRotator::ZeroRotator, FVector(19.0, 3.4, 4.0), Blued);
		Add(true, FVector(37.0, 0.0, 8.6), Along, FVector(1.9, 1.9, 62.0), Blued);
		Add(true, FVector(65.0, 0.0, 8.6), Along, FVector(2.7, 2.7, 3.0), Blued);
		Add(false, FVector(2.5, 0.0, -1.6), FRotator::ZeroRotator, FVector(10.0, 2.2, 0.9), Blued);
		Add(true, FVector(5.0, 0.0, 12.9), Along, FVector(3.0, 3.0, 28.0), Blued);
		Add(true, FVector(18.5, 0.0, 12.9), Along, FVector(4.5, 4.5, 8.0), Blued);
		Add(true, FVector(-9.5, 0.0, 12.9), Along, FVector(3.8, 3.8, 5.0), Blued);
		Add(true, FVector(22.6, 0.0, 12.9), Along, FVector(4.0, 4.0, 0.4), Glass);
		Add(false, FVector(0.0, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		Add(false, FVector(11.5, 0.0, 10.6), FRotator::ZeroRotator, FVector(2.2, 2.6, 3.4), Steel);
		USceneComponent* Bolt = NewObject<USceneComponent>(this, TEXT("OfficerKarBolt"));
		Bolt->SetMobility(EComponentMobility::Movable);
		Bolt->SetupAttachment(Rifle);
		Bolt->SetRelativeLocation(FVector(1.0, 0.0, 8.6));
		Bolt->RegisterComponent();
		KarBolt = Bolt;
		auto AddBolt = [&](bool bCyl, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInstanceDynamic* Mt)
		{
			UStaticMeshComponent* P = AAAShapes::Make(this, bCyl, Size, Mt, false, true);
			P->SetupAttachment(Bolt);
			P->SetRelativeLocationAndRotation(Loc, Rot);
			P->RegisterComponent();
		};
		AddBolt(true, FVector(-4.0, 0.0, 0.0), Along, FVector(1.8, 1.8, 13.0), Steel);
		AddBolt(true, FVector(-9.5, 0.0, 0.0), Along, FVector(2.3, 2.3, 4.0), Blued);
		AddBolt(false, FVector(-2.0, 2.6, -0.9), FRotator::ZeroRotator, FVector(1.1, 4.4, 1.0), Steel);
		AddBolt(false, FVector(-2.0, 5.4, -1.8), FRotator::ZeroRotator, FVector(2.0, 2.0, 2.0), Steel);
	}
	else
	{
	Add(false, FVector(-1.0, 0.0, -2.8), FRotator(14.0, 0.0, 0.0), FVector(3.2, 2.8, 10.0), Poly);
	Add(false, FVector(6.0, 0.0, 6.5), FRotator::ZeroRotator, FVector(26.0, 4.0, 8.0), Metal);
	Add(false, FVector((18.0 + HgB) * 0.5, 0.0, 6.6), FRotator::ZeroRotator, FVector(HgB - 18.0, 4.8, 5.4), Poly);
	// the marksman's longer barrel and scope, the support gunner's bipod legs
	const bool bTacGear = Team != AAATeam::Police;
	const double Barrel = (bTacGear && Trade == ETrade::Marksman) ? 26.0 : 16.0;
	Add(true, FVector(HgB + Barrel * 0.5, 0.0, 6.6), FRotator(90.0, 0.0, 0.0), FVector(2.0, 2.0, Barrel), Metal);
	if (bTacGear && Trade == ETrade::Marksman) Add(true, FVector(8.0, 0.0, 13.0), FRotator(90.0, 0.0, 0.0), FVector(3.6, 3.6, 22.0), Metal);
	if (bTacGear && Trade == ETrade::Support)
	{
		Add(true, FVector(HgB + 2.0, 2.2, 1.0), FRotator(70.0, 0.0, 20.0), FVector(1.2, 1.2, 14.0), Metal);
		Add(true, FVector(HgB + 2.0, -2.2, 1.0), FRotator(70.0, 0.0, -20.0), FVector(1.2, 1.2, 14.0), Metal);
	}
	Add(false, FVector(-14.5, 0.0, 5.5), FRotator::ZeroRotator, FVector(19.0, 3.4, 6.2), Poly);
	Add(false, FVector(12.5, 0.0, -4.6), FRotator(-12.0, 0.0, 0.0), FVector(3.6, 2.6, 14.0), Metal);
	MuzzleLocal = FVector(HgB + Barrel + 1.0, 0.0, 6.6);
	}
	if (bKar) MuzzleLocal = FVector(67.5, 0.0, 8.6);
	Flash = AAAShapes::Make(this, false, FVector(12.0, 5.0, 5.0), AAAShapes::Mat(this, FLinearColor(1.f, 0.65f, 0.25f) * 8.f, true), false, false);
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
	FlashLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
	FlashLight->SetCastShadows(false);
	FlashLight->RegisterComponent();
	// head gear (upright, on the head bone - never weld to simulated physics bodies!):
	// police cap, army helmet, the player's men a beret in their colour
	const FAttachmentTransformRules NoWeldWorld(EAttachmentRule::KeepWorld, false);
	const FVector Head = M->GetSocketLocation(TEXT("head"));
	const float Yaw = GetActorRotation().Yaw;
	if (Team == AAATeam::Enemy)
	{
		const FLinearColor Olive(0.10f, 0.11f, 0.06f);
		Cap = AAAShapes::Make(this, true, FVector(25.0, 25.0, 11.0), AAAShapes::Mat(this, Olive), false, true);
		Cap->SetWorldLocationAndRotation(Head + FVector(0.0, 0.0, 12.0), FRotator(0.0, Yaw, 0.0));
		Brim = AAAShapes::Make(this, true, FVector(29.0, 29.0, 1.6), AAAShapes::Mat(this, Olive * 0.8f), false, true);
		Brim->SetWorldLocationAndRotation(Head + FVector(0.0, 0.0, 7.5), FRotator(0.0, Yaw, 0.0));
	}
	else if (Team == AAATeam::Player)
	{
		const FLinearColor Beret = FAAASquadInfo::Color(SquadIndex) * 0.55f;
		Cap = AAAShapes::Make(this, true, FVector(23.0, 23.0, 5.0), AAAShapes::Mat(this, Beret), false, true);
		Cap->SetWorldLocationAndRotation(Head + FVector(0.0, 0.0, 15.0) + FRotator(0.0, Yaw, 0.0).RotateVector(FVector(0.0, 2.5, 0.0)), FRotator(0.0, Yaw, 12.0));
	}
	else
	{
		Cap = AAAShapes::Make(this, true, FVector(21.0, 21.0, 9.0), AAAShapes::Mat(this, FLinearColor(0.02f, 0.03f, 0.08f)), false, true);
		Cap->SetWorldLocationAndRotation(Head + FVector(0.0, 0.0, 14.0), FRotator(0.0, Yaw, 0.0));
		Brim = AAAShapes::Make(this, true, FVector(24.0, 24.0, 1.2), AAAShapes::Mat(this, FLinearColor(0.01f, 0.01f, 0.02f)), false, true);
		Brim->SetWorldLocationAndRotation(Head + FVector(0.0, 0.0, 10.0) + GetActorForwardVector() * 4.0, FRotator(0.0, Yaw, 0.0));
	}
	if (Cap) { Cap->RegisterComponent(); Cap->AttachToComponent(M, NoWeldWorld, TEXT("head")); }
	if (Brim) { Brim->RegisterComponent(); Brim->AttachToComponent(M, NoWeldWorld, TEXT("head")); }
}

FVector AAAAPoliceOfficer::Muzzle() const
{
	if (Rifle) return (FTransform(MuzzleLocal) * Rifle->GetComponentTransform()).GetLocation();
	return GetActorLocation() + GetActorForwardVector() * 70.0 + FVector(0.0, 0.0, 40.0);
}

/* ============================================================================================ helpers */

bool AAAAPoliceOfficer::LineClear(const FVector& A, const FVector& B) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAOfficerSight), false, this);
	if (const ACharacter* P = Player()) Q.AddIgnoredActor(P);
	if (ACharacter* F = FoeActor.Get()) Q.AddIgnoredActor(F);
	FHitResult H;
	return !GetWorld()->LineTraceSingleByChannel(H, A, B, ECC_Visibility, Q);
}

bool AAAAPoliceOfficer::PathClear(const FVector& From, const FVector& To) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAOfficerPath), false, this);
	FHitResult H;
	const FCollisionShape S = FCollisionShape::MakeCapsule(30.f, 60.f);
	return !GetWorld()->SweepSingleByChannel(H, From + FVector(0.0, 0.0, 15.0), To + FVector(0.0, 0.0, 15.0), FQuat::Identity, ECC_Pawn, S, Q);
}

bool AAAAPoliceOfficer::GroundAt(const FVector& P, FVector& Out) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAOfficerGround), false, this);
	FHitResult H;
	if (!GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.0, 0.0, 150.0), P - FVector(0.0, 0.0, 400.0), ECC_Visibility, Q)) return false;
	if (H.ImpactNormal.Z < 0.75) return false;
	Out = H.ImpactPoint + FVector(0.0, 0.0, 92.0);   // capsule centre
	// room for a person
	FCollisionQueryParams Q2(SCENE_QUERY_STAT(AAAOfficerRoom), false, this);
	return !GetWorld()->OverlapAnyTestByChannel(Out + FVector(0.0, 0.0, 5.0), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(32.f, 80.f), Q2);
}

void AAAAPoliceOfficer::MoveTo(const FVector& P, bool bRun)
{
	// If direct line is blocked by a car, pole, or building corner, pick an intermediate bypass waypoint
	const FVector Me = GetActorLocation();
	FVector Goal = P;
	if (FVector::Dist2D(Me, P) > 180.f && !PathClear(Me, P))
	{
		const FVector ToDir = (P - Me).GetSafeNormal2D();
		float BestScore = -1e9f;
		const float Angles[] = { -35.f, 35.f, -65.f, 65.f, -95.f, 95.f };
		const float Dists[] = { 380.f, 680.f };
		for (float Dist : Dists)
		{
			for (float Ang : Angles)
			{
				const FVector Cand = Me + FRotator(0.0, Ang, 0.0).RotateVector(ToDir) * Dist;
				FVector GroundCand;
				if (!GroundAt(Cand, GroundCand)) continue;
				if (!PathClear(Me, GroundCand)) continue;
				float Score = -(float)FVector::Dist2D(GroundCand, P);
				if (PathClear(GroundCand, P)) Score += 500.f; // clears the corner directly to target
				if (Score > BestScore)
				{
					BestScore = Score;
					Goal = GroundCand;
				}
			}
		}
	}
	MoveTarget = Goal; bMoving = true; bRunning = bRun; StuckT = 0.f;
}

bool AAAAPoliceOfficer::CanDespawn() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) return true;
	const FVector C = PC->PlayerCameraManager->GetCameraLocation();
	const FVector D = GetActorLocation() - C;
	const double Dist = D.Size();
	if (Dist > 9000.0) return true;
	if (FVector::DotProduct(D / FMath::Max(Dist, 1.0), PC->PlayerCameraManager->GetCameraRotation().Vector()) < 0.25) return true;   // behind the camera
	return !LineClear(C, GetActorLocation());   // hidden behind something
}

void AAAAPoliceOfficer::Say(const FString& Text, float MinGap)
{
	if (Team != AAATeam::Player || bDead) return;
	if (Now - LastSayT < MinGap || (Text == LastSaid && Now - LastSayT < 8.f)) return;
	LastSayT = Now; LastSaid = Text;
	if (UAAAWeaponsSubsystem* S = Squad()) S->Say(this, Text);
}

FString AAAAPoliceOfficer::Bearing(const FVector& Where) const
{
	// compass words (yaw 0 = north on the HUD compass, 90 = east) and the range in metres
	static const TCHAR* const Dir8[8] = { TEXT("شمال"), TEXT("شمال شرق"), TEXT("شرق"), TEXT("جنوب شرق"), TEXT("جنوب"), TEXT("جنوب غرب"), TEXT("غرب"), TEXT("شمال غرب") };
	const FVector To = Where - GetActorLocation();
	const float Yaw = FMath::Fmod((float)To.Rotation().Yaw + 360.f, 360.f);
	const int32 K = FMath::RoundToInt(Yaw / 45.f) % 8;
	const int32 M = FMath::Max(5, FMath::RoundToInt((float)To.Size2D() / 100.f / 5.f) * 5);
	return FString::Printf(TEXT("%s، %d متر"), Dir8[K], M);
}

/* ============================================================================================ senses */

void AAAAPoliceOfficer::HearShot(const FVector& Where, float Loudness)
{
	if (IsOut() || bStandDown) return;
	const float D = (float)FVector::Dist(Where, GetActorLocation());
	if (D > 9000.f * Loudness) return;
	// a rough position: the further away, the vaguer
	LastKnown = Where + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * (D * 0.06f);
	LastHeard = Now;
	bHasIntel = true;
	Awareness = FMath::Max(Awareness, D < 2500.f ? 0.9f : 0.6f);
	if (Team != AAATeam::Police && Known.Num() == 0) Learn(nullptr, LastKnown, false, false);
}

void AAAAPoliceOfficer::Suppress(float Amount)
{
	if (IsOut()) return;
	// the brave shrug it off a little
	const float K = Team == AAATeam::Police ? 1.f : FMath::Lerp(1.25f, 0.7f, Courage);
	Suppression = FMath::Min(1.f, Suppression + Amount * K);
	if (bPeeking && Suppression > 0.55f) { bPeeking = false; PeekT = FMath::FRandRange(1.5f, 3.f); }   // duck!
}

void AAAAPoliceOfficer::BeginExit(const FVector& Out)
{
	ExitDir = Out.GetSafeNormal2D();
	State = EState::Exit;
	BornT = Now = GetWorld() ? (float)GetWorld()->GetTimeSeconds() : Now;
	Awareness = FMath::Max(Awareness, 0.95f);   // the crew knows where the suspect is: no "who, me?" pause
	ShotCD = 0.55f + FMath::FRandRange(0.15f, 0.45f);   // first burst right after stepping out (rifle up)
	ThinkT = 0.46f;                                     // first decision the moment the exit ends
}

void AAAAPoliceOfficer::RadioIntel(const FVector& Where)
{
	if (IsOut() || bStandDown || bSees) return;
	LastKnown = Where;
	LastHeard = Now;
	bHasIntel = true;
	Awareness = FMath::Max(Awareness, 0.7f);
}

void AAAAPoliceOfficer::RadioContact(ACharacter* Who, const FVector& Where)
{
	if (IsOut() || bStandDown) return;
	Learn(Who, Where, false, true);
	if (!bSees) { LastKnown = Where; LastHeard = Now; bHasIntel = true; Awareness = FMath::Max(Awareness, 0.75f); }
}

void AAAAPoliceOfficer::RequestCoverFire(const FVector& Where, float Seconds)
{
	if (IsOut() || bHoldFire) return;
	CoverFirePos = Where;
	CoverFireT = FMath::Max(CoverFireT, Seconds);
}

AAAAPoliceOfficer::FContact* AAAAPoliceOfficer::FindContact(const ACharacter* Who)
{
	for (FContact& K : Known) if (K.Who.Get() == Who) return &K;
	return nullptr;
}

void AAAAPoliceOfficer::Learn(ACharacter* Who, const FVector& Where, bool bSeen, bool bFromRadio)
{
	if (Who && IsOutChar(Who)) return;
	FContact* K = Who ? FindContact(Who) : nullptr;
	if (!K && !Who)
	{
		// an unknown shooter "over there": merge with a nameless contact near it
		for (FContact& X : Known) if (!X.Who.IsValid() && FVector::Dist2D(X.Pos, Where) < 1500.f) { K = &X; break; }
	}
	if (!K)
	{
		if (Known.Num() >= 8)
		{
			// forget the oldest
			int32 Old = 0;
			for (int32 I = 1; I < Known.Num(); ++I) if (Known[I].InfoT < Known[Old].InfoT) Old = I;
			Known.RemoveAtSwap(Old);
		}
		FContact N;
		N.Who = Who;
		Known.Add(N);
		K = &Known.Last();
		// a new contact with my own eyes: tell the others
		if (bSeen && Team == AAATeam::Player) Say(FString::Printf(TEXT("تماس! %s"), *Bearing(Where)), 0.8f);
	}
	// the radio is a little late; my own eyes are not
	if (bSeen || !bFromRadio || Now - K->InfoT > 1.f) K->Pos = Where;
	K->InfoT = Now;
	if (bSeen) { K->SeenT = Now; K->bVisible = true; }
	if (bSeen && !bFromRadio)
		if (UAAAWeaponsSubsystem* S = Squad()) S->ShareContact(this, Team, Who, Where);
}

void AAAAPoliceOfficer::UpdateContacts(float Dt)
{
	// forget the dead and the stale; check one contact's line of sight per frame (cheap, and like real eyes)
	const float T = Now;
	Known.RemoveAll([T](const FContact& K) { return (K.Who.IsValid() && IsOutChar(K.Who.Get())) || K.Who.IsStale() || T - K.InfoT > 25.f; });
	if (!Known.Num()) return;
	KnownScan = (KnownScan + 1) % Known.Num();
	FContact& K = Known[KnownScan];
	ACharacter* W = K.Who.Get();
	if (!W) { K.bVisible = false; return; }
	const FVector Eye = GetActorLocation() + FVector(0.0, 0.0, EYES);
	const FVector Tgt = W->GetActorLocation();
	const float D = (float)FVector::Dist(Eye, Tgt);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAContactLOS), false, this);
	Q.AddIgnoredActor(W);
	if (const ACharacter* P = Player()) Q.AddIgnoredActor(P);
	FHitResult H;
	const bool bVis = D < SightRange && !W->IsHidden()
		&& (!GetWorld()->LineTraceSingleByChannel(H, Eye, Tgt + FVector(0.0, 0.0, 60.0), ECC_Visibility, Q)
			|| !GetWorld()->LineTraceSingleByChannel(H, Eye, Tgt + FVector(0.0, 0.0, 15.0), ECC_Visibility, Q));
	K.bVisible = bVis;
	if (bVis) { K.Pos = Tgt; K.SeenT = Now; K.InfoT = Now; }
}

float AAAAPoliceOfficer::ThreatOf(const FContact& K) const
{
	const FVector Me = GetActorLocation();
	const float D = (float)FVector::Dist(K.Pos, Me);
	float S = 3000.f / FMath::Max(D, 300.f);
	if (K.bVisible) S += 1.2f;
	const ACharacter* W = K.Who.Get();
	if (const AAAAPoliceOfficer* O = Cast<AAAAPoliceOfficer>(W))
	{
		if (O->Foe() == this) S += 1.6f;                                    // he is on me
		else if (O->Foe() && O->Foe() == Player()) S += 1.f;               // he is on the leader
		else if (O->Foe()) S += 0.5f;                                      // he is on one of us
		if (O->InCombat()) S += 0.4f;
		if (O->IsReloading()) S += 0.5f;                                   // a chance
		S *= 1.f - 0.4f * O->GetSuppression();                             // head down: less of a threat now
	}
	else if (W && W == Player()) S += 1.f;                                 // (the police / army against the leader himself)
	if (W && W == OrderTarget.Get()) S += 2.5f;                            // the leader marked him
	const float Age = Now - K.InfoT;
	S *= FMath::Clamp(1.f - Age / 12.f, 0.1f, 1.f);
	if (const UAAAWeaponsSubsystem* Sq = Squad())
	{
		const int32 Claims = Sq->TargetClaims(this, W);
		if (W != FoeActor.Get() && D > 1200.f) S -= 0.55f * Claims;       // spread the fire over them
	}
	return S;
}

void AAAAPoliceOfficer::PickTarget()
{
	const FContact* Best = nullptr;
	float BestS = -1e9f;
	for (const FContact& K : Known)
	{
		if (!K.Who.IsValid()) continue;
		float S = ThreatOf(K);
		if (K.Who.Get() == FoeActor.Get()) S += 0.6f;   // keep on him (no flicking)
		if (S > BestS) { BestS = S; Best = &K; }
	}
	if (Best)
	{
		if (Best->Who.Get() != FoeActor.Get()) { FoeActor = Best->Who; TrackT *= 0.3f; }
		bHasIntel = true;
		if (!Best->bVisible) { LastKnown = Best->Pos; LastHeard = FMath::Max(LastHeard, Best->InfoT); }
	}
	else if (Known.Num())
	{
		// only nameless contacts (heard shots): look there
		const FContact& K = Known.Last();
		LastKnown = K.Pos; LastHeard = FMath::Max(LastHeard, K.InfoT); bHasIntel = Now - K.InfoT < 12.f;
	}
}

int32 AAAAPoliceOfficer::FreshThreats(TArray<FVector>& OutPos, float MaxAge) const
{
	OutPos.Reset();
	for (const FContact& K : Known) if (Now - K.InfoT < MaxAge) OutPos.Add(K.Pos);
	return OutPos.Num();
}

void AAAAPoliceOfficer::GetContacts(TArray<FVector>& OutPos, TArray<float>& OutFresh) const
{
	for (const FContact& K : Known)
	{
		if (!K.Who.IsValid() || Now - K.InfoT > 10.f) continue;
		OutPos.Add(K.Pos);
		OutFresh.Add(FMath::Clamp(1.f - (Now - K.InfoT) / 10.f, 0.f, 1.f));
	}
}

void AAAAPoliceOfficer::ScanFoes()
{
	// fresh eyes: new foes I can see (any hostile side); the tactical sides choose their target in PickTarget
	UAAAWeaponsSubsystem* S = Squad();
	if (!S) return;
	TArray<ACharacter*> Cands;
	S->GetFoes(this, Cands);
	const FVector Me = GetActorLocation();
	Cands.Sort([Me](const ACharacter& A, const ACharacter& B) { return FVector::DistSquared(A.GetActorLocation(), Me) < FVector::DistSquared(B.GetActorLocation(), Me); });
	const FVector Eye = Me + FVector(0.0, 0.0, EYES);
	const FVector Fwd = FRotator(0.0, FacingYaw, 0.0).Vector();
	const bool bTac = Team != AAATeam::Police;
	ACharacter* Best = nullptr;
	float BestSc = -1e9f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAScanFoes), false, this);
	if (const ACharacter* P = Player()) Q.AddIgnoredActor(P);
	for (int32 I = 0; I < FMath::Min(Cands.Num(), 6); ++I)
	{
		ACharacter* C = Cands[I];
		const FVector To = C->GetActorLocation() - Me;
		const float D = (float)To.Size();
		if (D > SightRange) break;
		const float Cone = FVector::DotProduct(To / FMath::Max(D, 1.f), Fwd);
		// eyes: a cone forward, all round close by, wider once alert; the leader's men watch their own sector
		if (!(Cone > 0.3f || D < 900.f || (bHasIntel && Cone > -0.3f) || (Team == AAATeam::Player && Cone > -0.5f))) continue;
		if (bTac && FindContact(C)) continue;   // already tracked (UpdateContacts keeps it fresh)
		FCollisionQueryParams Q2 = Q;
		Q2.AddIgnoredActor(C);
		FHitResult H;
		const bool bVis = !C->IsHidden() && (!GetWorld()->LineTraceSingleByChannel(H, Eye, C->GetActorLocation() + FVector(0.0, 0.0, 60.0), ECC_Visibility, Q2)
			|| !GetWorld()->LineTraceSingleByChannel(H, Eye, C->GetActorLocation() + FVector(0.0, 0.0, 15.0), ECC_Visibility, Q2));
		if (!bVis) continue;
		if (bTac) { Learn(C, C->GetActorLocation(), true, false); continue; }
		float Sc = -D * 0.001f + Cone * 0.4f;
		if (C == FoeActor.Get()) Sc += 1.2f;                        // don't flick between targets
		if (const AAAAPoliceOfficer* O = Cast<AAAAPoliceOfficer>(C)) if (O->Foe() == this) Sc += 0.8f;   // he is shooting at me
		if (Sc > BestSc) { BestSc = Sc; Best = C; }
	}
	if (bTac) { PickTarget(); return; }
	if (Best && Best != FoeActor.Get())
	{
		FoeActor = Best;
		TrackT *= 0.3f;
		if (Awareness < 0.5f) Awareness = 0.5f;
	}
	else if (!Best && FoeDead())
	{
		// my foe is down (or gone): the player is the police's first business
		ACharacter* P = Player();
		FoeActor = (P && !IsOutChar(P) && Hostile(Team, AAATeam::Player, S->WantedStars())) ? P : nullptr;
		if (!FoeActor.IsValid()) { bSees = false; if (Now - LastHeard > 3.f) bHasIntel = false; }
	}
}

void AAAAPoliceOfficer::Perceive(float Dt)
{
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	const bool bWasSeeing = bSees;
	bSees = false; bHeadOnly = false;
	if (P && !P->IsHidden())
	{
		const FVector Eye = GetActorLocation() + FVector(0.0, 0.0, EYES);
		const FVector Head = P->GetActorLocation() + FVector(0.0, 0.0, 65.0), Chest = P->GetActorLocation() + FVector(0.0, 0.0, 20.0);
		const FVector To = Head - Eye;
		const float D = (float)To.Size();
		const float Cone = FVector::DotProduct(To / FMath::Max(D, 1.f), FRotator(0.0, FacingYaw, 0.0).Vector());
		if (D < SightRange && (Cone > 0.35f || D < 700.f || (bHasIntel && Cone > -0.2f) || Team == AAATeam::Player))
		{
			const bool bH = LineClear(Eye, Head), bC = LineClear(Eye, Chest);
			if (bH || bC)
			{
				// awareness builds faster close up, when the target moves or shoots
				float Loud = 1.f;
				if (P == Player()) { const UAAAWeaponComponent* W = PlayerWeapon(); Loud = (W && W->GetSinceShot() < 1.f) ? 3.f : 1.f; }
				else if (const AAAAPoliceOfficer* O = Cast<AAAAPoliceOfficer>(P)) Loud = O->InCombat() ? 2.f : 1.f;
				const float Moving = P->GetVelocity().Size() > 150.f ? 1.6f : 1.f;
				const float Alert = Team == AAATeam::Player ? 2.f : 1.f;   // the player's men are expecting trouble
				Awareness += Dt * Loud * Moving * Alert * FMath::Clamp(2500.f / FMath::Max(D, 300.f), 0.25f, 6.f) * (bHasIntel ? 3.f : 1.f);
				if (Awareness >= 1.f)
				{
					Awareness = 1.f;
					bSees = true; bHeadOnly = bH && !bC;
					LastKnown = P->GetActorLocation(); LastSeen = Now; bHasIntel = true;
				}
			}
		}
	}
	TrackT = bSees ? TrackT + Dt : FMath::Max(0.f, TrackT - 2.f * Dt);
	if (!bSees) Awareness = FMath::Max(bHasIntel ? 0.5f : 0.f, Awareness - Dt * 0.15f);
	if (bSees && !bWasSeeing)
	{
		if (Team == AAATeam::Police) { if (UAAAWeaponsSubsystem* S = Squad()) S->ShareIntel(this, LastKnown); }   // "I have him!"
		else Learn(P, LastKnown, true, false);
	}
	if (Team != AAATeam::Police) UpdateContacts(Dt);
	Suppression = FMath::Max(0.f, Suppression - Dt * 0.45f);
	CoverFireT = FMath::Max(0.f, CoverFireT - Dt);
}

/* ============================================================================================ cover */

bool AAAAPoliceOfficer::FindCover(bool bFlank, bool bAway)
{
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	const FVector Me = GetActorLocation();
	const FVector Target = P ? P->GetActorLocation() : LastKnown;
	const FVector ThreatHead = Target + FVector(0.0, 0.0, 65.0);
	UAAAWeaponsSubsystem* S = Squad();
	float Best = -1e9f;
	bool BestLowP = false;
	FVector BestC = FVector::ZeroVector, BestPeek = FVector::ZeroVector;
	const FVector Axis = (Me - Target).GetSafeNormal2D();
	for (int32 K = 0; K < 30; ++K)
	{
		FVector C;
		if (bFlank)
		{
			// around the target, 60..110 degrees off the line to me, 9..20 m out
			const float Side = (K % 2) ? 1.f : -1.f;
			const float A = FMath::DegreesToRadians(FMath::FRandRange(60.f, 110.f)) * Side;
			const FVector Dir = Axis.RotateAngleAxisRad(A, FVector::UpVector);
			C = Target + Dir * FMath::FRandRange(900.f, 2000.f);
		}
		else
		{
			const float R = bAway ? FMath::FRandRange(900.f, 2200.f) : FMath::FRandRange(250.f, 1400.f);
			const FVector Dir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			C = Me + Dir * R;
		}
		FVector G;
		if (!GroundAt(C, G)) continue;
		const FVector Chest = G + FVector(0.0, 0.0, CHEST);
		bool bLowP = false;
		if (LineClear(Chest, ThreatHead))
		{
			// low cover: crouched behind it his chest and face are hidden
			if (LineClear(G + FVector(0.0, 0.0, CHEST - 52.0), ThreatHead) || LineClear(G + FVector(0.0, 0.0, EYES - 56.0), ThreatHead)) continue;
			bLowP = true;
		}
		const float Dt = (float)FVector::Dist2D(G, Target);
		if (Dt < 600.f || (!bAway && Dt > 3500.f)) continue;
		// a peek spot beside it with a clear shot
		const FVector Side = FVector::CrossProduct((Target - G).GetSafeNormal2D(), FVector::UpVector);
		FVector Pk = FVector::ZeroVector;
		bool bPeek = false;
		for (const float Off : { 85.f, -85.f, 130.f, -130.f })
		{
			const FVector Q = G + Side * Off;
			if (LineClear(Q + FVector(0.0, 0.0, CHEST), ThreatHead) && PathClear(G, Q)) { Pk = Q; bPeek = true; break; }
		}
		const bool bOverP = bLowP && LineClear(G + FVector(0.0, 0.0, EYES + 10.0), ThreatHead);   // fire over the top
		if (!bPeek && !bOverP && !bAway) continue;
		if (!PathClear(Me, G)) continue;                                  // must reach it in a straight line
		if (S && S->CoverTaken(this, G)) continue;
		// score: close to me, at a fighting distance, behind the line (retreat), off-axis (flank)
		float Sc = -(float)FVector::Dist2D(G, Me) * 0.002f - FMath::Abs(Dt - 1700.f) * 0.0012f;
		if (bAway) Sc += Dt * 0.002f;
		if (bFlank) Sc += 1.f - FMath::Abs(FVector::DotProduct((G - Target).GetSafeNormal2D(), Axis));
		if (bPeek) Sc += 0.6f;
		if (bOverP) Sc += 0.4f;
		if (Sc > Best) { Best = Sc; BestC = G; BestPeek = bPeek ? Pk : G; BestLowP = bLowP; }
	}
	if (Best <= -1e8f) return false;
	Cover = BestC; Peek = BestPeek; PeekB = BestPeek; bHasCover = true; bPeeking = false; PeekT = FMath::FRandRange(0.6f, 1.4f);
	bCoverLow = BestLowP;
	MoveTo(Cover, true);
	return true;
}

bool AAAAPoliceOfficer::FindTacticalCover(int32 Mode)
{
	// Mode 0: a fighting position near here, 1: the next bound towards the enemy, 2: flank, 3: fall back
	TArray<FVector> Threats;
	FreshThreats(Threats, 10.f);
	const FVector Me = GetActorLocation();
	const FVector Primary = (bSees && FoeActor.IsValid()) ? FoeActor->GetActorLocation() : LastKnown;
	Threats.Insert(Primary, 0);
	if (Threats.Num() > 4) Threats.SetNum(4);
	const FVector PrimHead = Primary + FVector(0.0, 0.0, 65.0);
	UAAAWeaponsSubsystem* S = Squad();
	TArray<AAAAPoliceOfficer*> Mates;
	if (S) S->GetMates(this, Mates);
	const ACharacter* Lead = Team == AAATeam::Player ? Player() : nullptr;
	const bool bLeash = LeashR > 1.f;
	const FVector Axis = (Me - Primary).GetSafeNormal2D();
	const float DNow = (float)FVector::Dist2D(Me, Primary);

	float Best = -1e9f;
	bool BestLow = false;
	FVector BestC = FVector::ZeroVector, BestA = FVector::ZeroVector, BestB = FVector::ZeroVector;
	const int32 Samples = Mode == 2 ? 36 : 28;
	for (int32 K = 0; K < Samples; ++K)
	{
		FVector C;
		if (Mode == 2)
		{
			// a spot off the enemy's line, on either side
			const float Side = (K % 2) ? 1.f : -1.f;
			const float A = FMath::DegreesToRadians(FMath::FRandRange(55.f, 105.f)) * Side;
			C = Primary + Axis.RotateAngleAxisRad(A, FVector::UpVector) * FMath::FRandRange(FMath::Max(800.f, PrefMin * 0.8f), FMath::Max(1600.f, PrefMax * 0.7f));
		}
		else if (Mode == 1)
		{
			// 6..16 m on towards him
			const FVector Fw = -Axis;
			const FVector Sd = FVector::CrossProduct(Fw, FVector::UpVector);
			C = Me + Fw * FMath::FRandRange(600.f, 1600.f) + Sd * FMath::FRandRange(-700.f, 700.f);
		}
		else if (Mode == 3)
		{
			const FVector Back = Lead ? (Lead->GetActorLocation() - Primary).GetSafeNormal2D() : Axis;
			C = Me + (Back * FMath::FRandRange(700.f, 2000.f)) + FVector::CrossProduct(Back, FVector::UpVector) * FMath::FRandRange(-600.f, 600.f);
		}
		else
		{
			const FVector Dir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			C = (bLeash ? LeashC : Me) + Dir * FMath::FRandRange(150.f, bLeash ? LeashR : 1400.f);
		}
		FVector G;
		if (!GroundAt(C, G)) continue;
		if (bLeash && Mode != 3 && FVector::Dist2D(G, LeashC) > LeashR) continue;
		const FVector Chest = G + FVector(0.0, 0.0, CHEST);
		// protection: from the main threat it must be; from the others it is a bonus. Standing cover (a wall) or
		// low cover he crouches behind (a low wall, a car, a planter: his crouched chest and face hidden)
		bool bLow = false;
		if (LineClear(Chest, PrimHead))
		{
			if (LineClear(G + FVector(0.0, 0.0, CHEST - 52.0), PrimHead) || LineClear(G + FVector(0.0, 0.0, EYES - 56.0), PrimHead)) continue;
			bLow = true;
		}
		const FVector ProtPt = bLow ? G + FVector(0.0, 0.0, EYES - 56.0) : Chest;
		float Prot = 1.f;
		for (int32 T = 1; T < Threats.Num(); ++T) Prot += LineClear(ProtPt, Threats[T] + FVector(0.0, 0.0, 65.0)) ? -0.5f : 0.6f;
		const float D = (float)FVector::Dist2D(G, Primary);
		if (D < 500.f) continue;
		// peek spots each side with a clear shot at him
		const FVector Side = FVector::CrossProduct((Primary - G).GetSafeNormal2D(), FVector::UpVector);
		FVector PA = FVector::ZeroVector, PB = FVector::ZeroVector;
		bool bA = false, bB = false;
		for (const float Off : { 85.f, 130.f })
		{
			if (!bA) { const FVector Q = G + Side * Off; if (LineClear(Q + FVector(0.0, 0.0, CHEST), PrimHead) && PathClear(G, Q)) { PA = Q; bA = true; } }
			if (!bB) { const FVector Q = G - Side * Off; if (LineClear(Q + FVector(0.0, 0.0, CHEST), PrimHead) && PathClear(G, Q)) { PB = Q; bB = true; } }
		}
		// standing up over low cover also works (a wall, a car bonnet)
		const bool bOver = LineClear(G + FVector(0.0, 0.0, EYES + 10.0), PrimHead);
		if (!bA && !bB && !bOver && Mode != 3) continue;
		if (!PathClear(Me, G)) continue;
		if (S && S->CoverTaken(this, G)) continue;
		// score
		float Sc = Prot * 1.2f;
		Sc += (bA ? 0.45f : 0.f) + (bB ? 0.45f : 0.f) + (bOver ? 0.3f : 0.f);
		// my trade's range band
		if (D < PrefMin) Sc -= (PrefMin - D) * 0.0012f;
		else if (D > PrefMax) Sc -= (D - PrefMax) * 0.0009f;
		// the run there: short, and not across his sights (the middle of the run seen by him = bad)
		const float Run = (float)FVector::Dist2D(G, Me);
		Sc -= Run * 0.0011f;
		const FVector Mid = (Me + G) * 0.5 + FVector(0.0, 0.0, CHEST);
		if (Run > 300.f && LineClear(Mid, PrimHead)) Sc -= Mode == 2 ? 1.6f : 0.7f;
		// spacing: not on top of a mate (one grenade for two)
		for (const AAAAPoliceOfficer* Mt : Mates)
		{
			const float Dm = (float)FVector::Dist2D(Mt->GetActorLocation(), G);
			if (Dm < 350.f) Sc -= (350.f - Dm) * 0.004f;
		}
		if (Lead && FVector::Dist2D(Lead->GetActorLocation(), G) < 300.f) Sc -= 0.8f;
		// what this move is for
		if (Mode == 1) Sc += (DNow - D) * 0.0012f;                            // forwards
		if (Mode == 2) Sc += (1.f - FMath::Abs(FVector::DotProduct((G - Primary).GetSafeNormal2D(), Axis))) * 2.f;   // off his line
		if (Mode == 3) Sc += (D - DNow) * 0.0015f;                            // backwards
		if (Team == AAATeam::Player && Order == EAAASquadOrder::Assault) Sc -= D * 0.0004f;
		// height: looking down on them
		Sc += FMath::Clamp((float)(G.Z - Primary.Z) / 300.f, -0.3f, 0.5f);
		if (bLow && bOver) Sc += 0.15f;   // fight over it without stepping out: the head is all he shows
		if (Sc > Best) { Best = Sc; BestC = G; BestA = bA ? PA : (bB ? PB : G); BestB = bB ? PB : (bA ? PA : G); BestLow = bLow; }
	}
	if (Best <= -1e8f) return false;
	Cover = BestC; Peek = BestA; PeekB = BestB; bHasCover = true; bPeeking = false; PeekT = FMath::FRandRange(0.5f, 1.2f);
	bCoverLow = BestLow;
	MoveTo(Cover, true);
	return true;
}

/* ============================================================================================ brain */

bool AAAAPoliceOfficer::ThinkEngage()
{
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	UAAAWeaponsSubsystem* S = Squad();
	const float SinceSeen = Now - LastSeen;
	const float Dist = P ? (float)FVector::Dist(P->GetActorLocation(), GetActorLocation()) : 1e9f;
	if (!(bSees || SinceSeen < 5.f || Now - LastHeard < 3.f)) return false;

	if (State != EState::Engage) { State = EState::Engage; StateT = 0.f; bHasCover = false; }

	// fighting withdrawal if the target rushes close into melee range (< 6.5m)
	if (P && bSees && Dist < 650.f && !bMoving)
	{
		const FVector Away = (GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		const FVector FallbackSpot = GetActorLocation() + Away * 550.0;
		if (PathClear(GetActorLocation(), FallbackSpot))
		{
			MoveTo(FallbackSpot, false);
			bHasCover = false;
			return true;
		}
	}

	// need (new) cover: none yet, or the target can see my cover spot now (flanked)
	CoverCheckT -= 0.3f;
	bool bNeedCover = !bHasCover;
	if (bHasCover && !bMoving && CoverCheckT <= 0.f && P)
	{
		CoverCheckT = 1.5f;
		if (LineClear(Cover + FVector(0.0, 0.0, CHEST), P->GetActorLocation() + FVector(0.0, 0.0, 65.0))) bNeedCover = true;
		if (FVector::Dist2D(Cover, P->GetActorLocation()) < 500.f) bNeedCover = true;   // too close
		if (Role == RoleFlank && StateT > 8.f) { bNeedCover = true; StateT = 0.f; }      // keep moving around him
	}
	if (bNeedCover && (!S || S->CanMove(this)))
	{
		if (!FindCover(Role == RoleFlank, false) && !bMoving)
		{
			// nothing usable: keep a fighting distance, sidestep
			const FVector Tgt = P ? P->GetActorLocation() : LastKnown;
			const FVector To = Tgt - GetActorLocation();
			const FVector Side = FVector::CrossProduct(To.GetSafeNormal2D(), FVector::UpVector) * (FMath::RandBool() ? 1.0 : -1.0);
			const FVector Want = To.Size() > 2600.0 ? GetActorLocation() + To.GetSafeNormal2D() * 600.0 : GetActorLocation() + Side * 300.0;
			if (PathClear(GetActorLocation(), Want)) MoveTo(Want, false);
			bHasCover = false;
		}
	}
	return true;
}

void AAAAPoliceOfficer::ThinkPatrol()
{
	State = EState::Patrol;
	SetTask(ETask::Patrol);
	if (!Patrol.Num()) { bMoving = false; return; }
	const FVector Goal = Patrol[PatrolIdx % Patrol.Num()];
	if (FVector::Dist2D(Goal, GetActorLocation()) < 140.f)
	{
		bMoving = false;
		PatrolWaitT -= 0.3f;
		if (PatrolWaitT <= 0.f) { PatrolIdx = (PatrolIdx + 1) % Patrol.Num(); PatrolWaitT = FMath::FRandRange(2.f, 5.f); }
	}
	else if (!bMoving) MoveTo(Goal, false);
}

void AAAAPoliceOfficer::SetOrder(EAAASquadOrder NewOrder, const FVector& Where, ACharacter* Target)
{
	if (IsOut()) return;
	Order = NewOrder;
	OrderPos = Where.IsNearlyZero() ? GetActorLocation() : Where;
	OrderTarget = Target;
	OrderT = 0.f;
	bOrderReached = false;
	bHasCover = false;
	bMoving = false;
	bPeeking = false;
	ThinkT = FMath::FRandRange(0.05f, 0.45f);   // not all three at the same instant
	LeashC = GetActorLocation();
	LeashR = 0.f;
	if (NewOrder == EAAASquadOrder::Hold) OrderPos = GetActorLocation();
	if (Target && !Target->IsHidden())
	{
		Learn(Target, Target->GetActorLocation(), false, true);
		FoeActor = Target;
		LastKnown = Target->GetActorLocation(); LastHeard = Now; bHasIntel = true;
		Awareness = FMath::Max(Awareness, 0.85f);
	}
	else if (NewOrder == EAAASquadOrder::Assault)
	{
		// "they are over there": go and find them
		LastKnown = OrderPos; LastHeard = Now; bHasIntel = true;
	}
	if (NewOrder == EAAASquadOrder::FallBack) State = EState::Idle;
	// the answer on the radio (the subsystem lets only one of them answer)
	static const TCHAR* const Ack[7] = { TEXT("مفهوم، معك."), TEXT("أتقدّم!"), TEXT("ثابت هنا."), TEXT("نهاجم!"), TEXT("أحتمي!"), TEXT("نيران تغطية!"), TEXT("ننسحب!") };
	Say(Ack[FMath::Clamp((int32)NewOrder, 0, 6)], 0.4f);
}

void AAAAPoliceOfficer::CatchUp(const FVector& Where)
{
	if (IsOut()) return;
	SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
	bMoving = false; bHasCover = false; bPeeking = false; StuckT = 0.f;
	if (ACharacter* L = Player()) FacingYaw = L->GetActorRotation().Yaw;
}

bool AAAAPoliceOfficer::MayMove() const
{
	// bounding overwatch: run while someone covers you (or nobody sees you, or you are alone)
	if (!bSees && Suppression < 0.3f) return true;
	const UAAAWeaponsSubsystem* S = Squad();
	if (!S) return true;
	TArray<AAAAPoliceOfficer*> Mates;
	S->GetMates(this, Mates);
	if (!Mates.Num()) return true;
	int32 Covering = 0, Moving = 0;
	for (const AAAAPoliceOfficer* M : Mates) { if (M->IsCovering()) ++Covering; if (M->IsMovingToCover()) ++Moving; }
	if (Team == AAATeam::Player)
		if (const UAAAWeaponComponent* W = PlayerWeapon()) if (W->GetSinceShot() < 1.5f) ++Covering;   // the leader is shooting too
	return Covering > 0 && Moving == 0;
}

void AAAAPoliceOfficer::ThinkTactical()
{
	UAAAWeaponsSubsystem* S = Squad();
	ACharacter* L = Team == AAATeam::Player ? Player() : nullptr;
	const FVector Me = GetActorLocation();
	OrderT += 0.3f;
	TaskT += 0.3f;
	const bool bLeaderFast = L && L->GetVelocity().Size2D() > 380.f;

	// ---- the area the leader's order allows
	if (Team == AAATeam::Player)
	{
		switch (Order)
		{
		case EAAASquadOrder::Follow:    LeashC = S ? S->FormationSlot(SquadIndex) : Me; LeashR = 1400.f; break;
		case EAAASquadOrder::Move:      LeashC = OrderPos; LeashR = bOrderReached ? 750.f : 0.f; break;
		case EAAASquadOrder::Hold:      LeashC = OrderPos; LeashR = 550.f; break;
		case EAAASquadOrder::Assault:
			LeashC = (OrderTarget.IsValid() && !IsOutChar(OrderTarget.Get())) ? OrderTarget->GetActorLocation() : OrderPos;
			LeashR = Trade == ETrade::Marksman ? 5200.f : 2200.f;   // the marksman takes a far overwatch spot
			break;
		case EAAASquadOrder::TakeCover: LeashR = 1100.f; break;   // LeashC = where the order found him
		case EAAASquadOrder::Suppress:  LeashR = 450.f; break;
		case EAAASquadOrder::FallBack:
			if (L) LeashC = L->GetActorLocation() - L->GetActorForwardVector() * 900.0 + L->GetActorRightVector() * ((SquadIndex - 1) * 280.0);
			LeashR = 0.f;
			break;
		}
		// fall back: run, don't stop to fight; then follow again
		if (Order == EAAASquadOrder::FallBack)
		{
			State = EState::Idle; SetTask(ETask::Retreat);
			if (FVector::Dist2D(Me, LeashC) < 200.f || OrderT > 14.f) { Order = EAAASquadOrder::Follow; bMoving = false; }
			else MoveTo(LeashC, true);
			return;
		}
		// cover fire on a point: everyone keeps firing at it (Shoot does it), from where he stands
		if (Order == EAAASquadOrder::Suppress)
		{
			State = EState::Engage; SetTask(ETask::CoverFire);
			if (FVector::Dist2D(Me, LeashC) > LeashR) MoveTo(LeashC, false); else bMoving = false;
			if (OrderT > 12.f) { Order = EAAASquadOrder::Hold; OrderPos = Me; }
			return;
		}
	}

	// ---- a mate is down: the medic (support first, the nearest who can) runs to him
	if (Team == AAATeam::Player && S)
	{
		AAAAPoliceOfficer* Pat = ReviveWho.Get();
		if (Pat && !Pat->IsDowned()) { ReviveWho = nullptr; Pat = nullptr; }
		if (!Pat && Health > MaxHealth * 0.35f)
		{
			if (AAAAPoliceOfficer* Need = S->NeedsMedic(this))
			{
				const float Dp = (float)FVector::Dist2D(Need->GetActorLocation(), Me);
				const float Danger = (bSees ? 0.6f : 0.f) + Suppression;
				const float Will = (Trade == ETrade::Support ? 1.f : 0.6f) * Courage + (Need->GetBleedT() < 15.f ? 0.4f : 0.f) - Danger * 0.5f - Dp / 6000.f;
				if (Will > 0.15f && Dp < 4500.f)
				{
					ReviveWho = Need; Pat = Need; S->ClaimRevive(this, Need);
					Say(FString::Printf(TEXT("سأسعف %s، غطّوني!"), FAAASquadInfo::Name(Need->SquadIndex)), 0.5f);
				}
			}
		}
		if (Pat)
		{
			SetTask(ETask::Revive); State = EState::Idle;
			S->ClaimRevive(this, Pat);
			const float Dp = (float)FVector::Dist2D(Pat->GetActorLocation(), Me);
			if (Dp > 110.f) { MoveTo(Pat->GetActorLocation() + (Me - Pat->GetActorLocation()).GetSafeNormal2D() * 80.0, true); bHasCover = false; }
			else bMoving = false;   // Act does the first aid while he is next to him
			if (Dp > 110.f && (bSees || Suppression > 0.3f))
			{
				TArray<FVector> Th;
				if (FreshThreats(Th, 5.f) > 0) S->RequestCoverFire(this, Th[0]);
			}
			return;
		}
	}

	// ---- the fight
	const bool bShotAt = Now - LastHurtT < 3.f;
	const bool bFresh = bHasIntel && (bSees || Now - LastSeen < 6.f || Now - LastHeard < 4.f);
	const bool bFight = bFresh && (!bHoldFire || bShotAt || Team != AAATeam::Player);
	if (bFight)
	{
		if (State != EState::Engage) { State = EState::Engage; StateT = 0.f; EngageT = 0.f; bHasCover = false; if (Team == AAATeam::Player && S) ++S->ContactSerial; }
		EngageT += 0.3f;
		ACharacter* F = FoeDead() ? nullptr : FoeActor.Get();
		const FVector Tgt = (F && bSees) ? F->GetActorLocation() : LastKnown;
		const float D = (float)FVector::Dist2D(Tgt, Me);
		const bool bInCover = IsInCover();
		const AAAAPoliceOfficer* FO = Cast<AAAAPoliceOfficer>(F);

		// hurt and under fire: pull back to a safer spot, ask for cover
		if (Health < MaxHealth * 0.4f && bShotAt && TaskT > 3.f && Task != ETask::Retreat)
		{
			if (FindTacticalCover(3)) { SetTask(ETask::Retreat); Say(TEXT("أنا مصاب، أتراجع!")); if (S) S->RequestCoverFire(this, Tgt); return; }
		}
		if (Task == ETask::Retreat && bMoving && TaskT < 6.f) return;

		// reload: in cover, or when nobody is in my sights; tell the others
		if (ReloadT < 0.f && Ammo <= 12 && (!bSees || bInCover || Ammo == 0))
		{
			ReloadT = 0.f; bPeeking = false;
			SetTask(ETask::Reload);
			if (Mags > 0) --Mags;
			Say(Mags <= 1 ? TEXT("أعيد التلقيم، ذخيرتي قليلة!") : TEXT("أعيد التلقيم! غطّوني"));
			if (S && bSees) S->RequestCoverFire(this, Tgt);
			return;
		}
		if (ReloadT >= 0.f) { SetTask(ETask::Reload); if (!bHasCover && !bMoving) FindTacticalCover(0); return; }

		// frag him out: he hides (I know where, I can't see him), at a throwing range, no friends near him
		if (Grenades > 0 && GrenadeCD <= 0.f && ThrowT < 0.f && !bSees && Now - LastSeen < 4.f && D > 900.f && D < 3000.f && !(Team == AAATeam::Player && bHoldFire))
		{
			bool bClear = true;
			if (S)
			{
				TArray<AAAAPoliceOfficer*> Mates;
				S->GetMates(this, Mates);
				for (const AAAAPoliceOfficer* M : Mates) if (FVector::Dist2D(M->GetActorLocation(), Tgt) < 900.f) bClear = false;
			}
			if (L && FVector::Dist2D(L->GetActorLocation(), Tgt) < 1100.f) bClear = false;
			if (bClear && FMath::FRand() < 0.35f + Aggression * 0.3f)
			{
				ThrowT = 0.f; bGrenadeSpawned = false;
				GrenadeCD = FMath::FRandRange(18.f, 28.f);
				TargetThrowLoc = Tgt + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), 0.f);
				SetTask(ETask::Grenade);
				Say(TEXT("قنبلة!"), 0.5f);
				return;
			}
		}

		// a mate is moving and asked me to cover him
		if (CoverFireT > 0.f && !bSees) { SetTask(ETask::CoverFire); bMoving = false; return; }

		// is my spot still good?
		CoverCheckT -= 0.3f;
		bool bCompromised = false;
		if (bHasCover && !bMoving && CoverCheckT <= 0.f)
		{
			CoverCheckT = 1.2f;
			TArray<FVector> Th;
			FreshThreats(Th, 6.f);
			for (const FVector& T : Th) if (LineClear(Cover + FVector(0.0, 0.0, CHEST), T + FVector(0.0, 0.0, 65.0))) { bCompromised = true; break; }
			if (bCompromised) Say(TEXT("مكشوف هنا، أغيّر موقعي!"));
		}
		const bool bOutOfBand = !bMoving && (D < PrefMin * 0.6f || D > PrefMax * 1.35f) && EngageT > 4.f;
		const bool bLeashOut = LeashR > 1.f && bHasCover && FVector::Dist2D(Cover, LeashC) > LeashR * 1.3f;

		// flank: the rifleman (or the bold) goes round a pinned enemy while the others keep him busy
		const bool bPinned = FO && (FO->GetSuppression() > 0.35f || (FO->IsInCover() && !FO->SeesFoe()));
		const bool bMayFlank = (Team != AAATeam::Player || Order == EAAASquadOrder::Assault || Order == EAAASquadOrder::Follow);
		if (!bMoving && bMayFlank && Now - LastFlankT > 14.f && EngageT > 5.f && (Trade == ETrade::Rifleman || Aggression > 0.7f) && bPinned && MayMove())
		{
			if (FindTacticalCover(2))
			{
				LastFlankT = Now; SetTask(ETask::Flank);
				Say(TEXT("ألتفّ عليه من الجنب!"));
				if (S) S->RequestCoverFire(this, Tgt);
				return;
			}
		}
		// assault: bound forward cover to cover while the others cover
		if (!bMoving && Team == AAATeam::Player && Order == EAAASquadOrder::Assault && Trade != ETrade::Marksman && D > PrefMin && TaskT > 3.f && MayMove())
		{
			if (FindTacticalCover(1)) { SetTask(ETask::Bound); Say(TEXT("أتحرك!"), 2.5f); if (S && bSees) S->RequestCoverFire(this, Tgt); return; }
		}
		// the army pushes too, now and then
		if (!bMoving && Team == AAATeam::Enemy && Aggression > 0.6f && D > PrefMin && TaskT > 6.f && FMath::FRand() < 0.25f && MayMove())
		{
			if (FindTacticalCover(1)) { SetTask(ETask::Bound); return; }
		}
		if ((!bHasCover || bCompromised || bOutOfBand || bLeashOut) && !bMoving)
		{
			if (MayMove() || bCompromised || !bHasCover)
			{
				if (FindTacticalCover(0)) { SetTask(ETask::Reposition); if (bSees && S) S->RequestCoverFire(this, Tgt); return; }
				// nothing usable: sidestep out of his line, keep fighting
				const FVector To = Tgt - Me;
				const FVector Side = FVector::CrossProduct(To.GetSafeNormal2D(), FVector::UpVector) * (FMath::RandBool() ? 1.0 : -1.0);
				FVector Want = D > PrefMax ? Me + To.GetSafeNormal2D() * 500.0 : Me + Side * 300.0;
				if (LeashR > 1.f && FVector::Dist2D(Want, LeashC) > LeashR) Want = LeashC + (Want - LeashC).GetSafeNormal2D() * LeashR * 0.8f;
				if (PathClear(Me, Want)) MoveTo(Want, false);
				bHasCover = false;
			}
		}
		if (!bMoving) SetTask(ETask::Fight);
		return;
	}

	// ---- no fight: search a fresh lead, or carry out the order / patrol
	if (bHasIntel && Now - LastHeard < 15.f && Team == AAATeam::Enemy)
	{
		if (State != EState::Search) { State = EState::Search; StateT = 0.f; SearchT = 0.f; MoveTo(LastKnown + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f), false); bHasCover = false; }
		SetTask(ETask::Search);
		if (!bMoving) { SearchT += 0.3f; if (SearchT > 4.f) { MoveTo(LastKnown + FVector(FMath::FRandRange(-800.f, 800.f), FMath::FRandRange(-800.f, 800.f), 0.f), false); SearchT = 0.f; } }
		if (StateT > 35.f) { bHasIntel = false; Known.Reset(); }
		return;
	}
	if (Team == AAATeam::Enemy) { ThinkPatrol(); return; }

	State = EState::Idle;
	SetTask(ETask::Order);
	if (ReloadT < 0.f && Ammo < 20) { ReloadT = 0.f; if (Mags > 0) --Mags; }   // top up between fights
	if (Mags <= 1 && !bToldLowAmmo) { bToldLowAmmo = true; Say(TEXT("ذخيرتي قاربت على النفاد!"), 0.5f); }
	switch (Order)
	{
	case EAAASquadOrder::Follow:
	{
		const FVector Slot = LeashC;
		const float D = (float)FVector::Dist2D(Me, Slot);
		if (D > 260.f) MoveTo(Slot, D > 650.f || bLeaderFast);
		else bMoving = false;
		bHasCover = false;
		break;
	}
	case EAAASquadOrder::Move:
	case EAAASquadOrder::Assault:
	{
		// a short line across the approach (not three men on one spot)
		const FVector Goal0 = LeashC;
		const FVector Across = FVector::CrossProduct((Goal0 - (L ? L->GetActorLocation() : Me)).GetSafeNormal2D(), FVector::UpVector);
		const FVector Goal = Goal0 + Across * ((SquadIndex - 1) * 300.0);
		if (!bOrderReached && FVector::Dist2D(Me, Goal) < 170.f) { bOrderReached = true; if (SquadIndex == 0) Say(TEXT("وصلنا. المكان آمن.")); }
		if (bOrderReached)
		{
			bMoving = false;
			if (Order == EAAASquadOrder::Assault && OrderT > 4.f) { Order = EAAASquadOrder::Hold; OrderPos = Me; }
		}
		else if (!bMoving || FVector::Dist2D(MoveTarget, Goal) > 300.f) MoveTo(Goal, true);
		break;
	}
	case EAAASquadOrder::Hold:
		if (FVector::Dist2D(Me, OrderPos) > 240.f) MoveTo(OrderPos, false);
		else bMoving = false;
		break;
	case EAAASquadOrder::TakeCover:
		if (!bHasCover && !bMoving)
		{
			// the threat: what we know of, else whatever is in front of the leader
			if (!Known.Num() && L) LastKnown = L->GetActorLocation() + L->GetActorForwardVector() * 2500.0;
			FindTacticalCover(0);
		}
		break;
	default: break;
	}
}

void AAAAPoliceOfficer::ThinkPolice()
{
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	UAAAWeaponsSubsystem* S = Squad();
	const UAAAWeaponComponent* W = PlayerWeapon();
	const bool bFoePlayer = P && P == Player();
	const bool bPlayerArmed = W && W->IsArmed();
	PlayerArmedT = bPlayerArmed ? PlayerArmedT + 0.3f : 0.f;
	const int32 Stars = S ? S->WantedStars() : 1;
	const float Dist = P ? (float)FVector::Dist(P->GetActorLocation(), GetActorLocation()) : 1e9f;

	if (bStandDown || Stars == 0 || (W && W->IsDead() && !P)) { State = EState::Idle; bMoving = false; return; }
	if (State == EState::Exit && Now - BornT < 0.45f) return;   // getting out of the car (Act steps him out meanwhile)

	// badly hurt: fall back to cover far from the threat, once
	if (Health < MaxHealth * 0.35f && !bRetreated && bHasIntel)
	{
		bRetreated = true; State = EState::Retreat; StateT = 0.f;
		if (!FindCover(false, true)) MoveTo(GetActorLocation() + (GetActorLocation() - ThreatPos()).GetSafeNormal2D() * 1500.0, true);
		if (S) S->CallBackup();
		return;
	}
	if (State == EState::Retreat && StateT < 6.f && bMoving) return;

	// one star and no gun out: arrest instead of shooting
	if (bFoePlayer && Stars <= 1 && !bPlayerArmed && bHasIntel && (W ? W->GetSinceShot() > 6.f : true))
	{
		if (State != EState::Arrest) { State = EState::Arrest; StateT = 0.f; ArrestT = 0.f; }
		if (Dist > 220.f) MoveTo(P->GetActorLocation() - (P->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 170.0, Dist > 900.f);
		else bMoving = false;
		return;
	}

	if (ThinkEngage()) return;
	if (bHasIntel)
	{
		// lost him: go and look where he was last seen / heard
		if (State != EState::Search) { State = EState::Search; StateT = 0.f; SearchT = 0.f; MoveTo(LastKnown, true); bHasCover = false; }
		if (!bMoving) { SearchT += 0.3f; if (SearchT > 4.f) { MoveTo(LastKnown + FVector(FMath::FRandRange(-800.f, 800.f), FMath::FRandRange(-800.f, 800.f), 0.f), false); SearchT = 0.f; } }
		if (StateT > 40.f) { bHasIntel = false; State = EState::Idle; bMoving = false; }
		return;
	}
	State = EState::Idle; bMoving = false;
}

void AAAAPoliceOfficer::Think()
{
	if (bDowned) return;
	ScanFoes();
	if (EvadeT > 0.f) return;   // running from a grenade: nothing else matters for these seconds
	// can he take a knee here and still see what he is watching? (eyes ~1.1 m when kneeling)
	{
		bKneelOK = true;
		const ACharacter* F = FoeDead() ? nullptr : FoeActor.Get();
		if (F && (bSees || Now - LastSeen < 3.f) && GetCapsuleComponent())
		{
			const FVector KneelEye(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 108.0);
			bKneelOK = LineClear(KneelEye, F->GetActorLocation() + FVector(0.0, 0.0, 50.0));
		}
	}
	if (Team == AAATeam::Police) ThinkPolice();
	else ThinkTactical();
}

void AAAAPoliceOfficer::Act(float Dt)
{
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	ACharacter* Lead = Player();
	UCharacterMovementComponent* CM = GetCharacterMovement();
	// getting out of the car: a few steps away from the door while drawing the rifle and turning to the suspect
	if (State == EState::Exit && Now - BornT < 0.6f && !ExitDir.IsNearlyZero())
	{
		const float K = FMath::Clamp((Now - BornT) / 0.6f, 0.f, 1.f);
		AddMovementInput(ExitDir, FMath::Lerp(0.9f, 0.35f, K));
		if (CM) CM->MaxWalkSpeed = WalkSpeed;
	}
	// first aid next to a downed mate
	AAAAPoliceOfficer* Pat = ReviveWho.Get();
	const bool bAiding = Task == ETask::Revive && Pat && Pat->IsDowned() && FVector::Dist2D(Pat->GetActorLocation(), GetActorLocation()) < 130.f;
	if (bAiding)
	{
		Pat->AddRevive(Dt / 3.6f);
		if (!Pat->IsDowned()) { ReviveWho = nullptr; SetTask(ETask::None); Say(TEXT("أنت بخير، قم وقاتل!"), 0.5f); }
	}
	// peeking from cover: step out (one side, then the other: not where he waits for me), shoot, duck back in
	if (State == EState::Engage && bHasCover && !bMoving)
	{
		PeekT -= Dt;
		const bool bPinnedDown = Team != AAATeam::Police && Suppression > 0.7f;   // heads down
		if (PeekT <= 0.f)
		{
			if (bPeeking || ReloadT >= 0.f || Ammo <= 0 || bPinnedDown)
			{
				bPeeking = false;
				PeekT = FMath::Max(0.5f, FMath::FRandRange(0.8f, 2.0f) + Suppression * 2.f + (bHurtWhilePeeking ? 1.f : 0.f) - (Team != AAATeam::Police ? Aggression * 0.6f : 0.f));
				if (Ammo < 12 && ReloadT < 0.f) { ReloadT = 0.f; if (Mags > 0) --Mags; }
			}
			else
			{
				bPeeking = true;
				if (bHurtWhilePeeking || FMath::FRand() < 0.35f) bPeekSideB = !bPeekSideB;   // the other side (he aims at the last one)
				// low cover: mostly up over the top, sometimes round the end; a side peek is now and then made from a
				// crouch (the head shows where nobody is aiming); hit last time over the top -> try the side
				const bool bSideOK = FVector::Dist2D(Peek, Cover) > 30.f || FVector::Dist2D(PeekB, Cover) > 30.f;
				bPeekOver = bCoverLow && (!bSideOK || (!bHurtWhilePeeking && FMath::FRand() < 0.65f));
				bLowPeek = !bPeekOver && FMath::FRand() < 0.3f;
				bHurtWhilePeeking = false;
				PeekT = (Trade == ETrade::Marksman && Team != AAATeam::Police) ? FMath::FRandRange(2.5f, 4.f) : FMath::FRandRange(1.6f, 3.2f);
			}
		}
		const FVector Spot = bPeeking ? (bPeekOver ? Cover : (bPeekSideB ? PeekB : Peek)) : Cover;
		if (FVector::Dist2D(Spot, GetActorLocation()) > 25.f) AddMovementInput((Spot - GetActorLocation()).GetSafeNormal2D(), 1.f);
		if (CM) CM->MaxWalkSpeed = WalkSpeed;
	}
	else if (bMoving)
	{
		const FVector To = MoveTarget - GetActorLocation();
		if (To.Size2D() < 48.f) bMoving = false;
		else
		{
			if (SmoothDir.IsNearlyZero()) SmoothDir = To.GetSafeNormal2D();   // member (was a static map keyed by pointer)

			FVector DesiredDir = To.GetSafeNormal2D();
			const FVector Me = GetActorLocation();
			const FVector Start = Me + FVector(0.0, 0.0, 22.0);

			// 7-angle volumetric sphere-sweep feeler fan (detects walls, poles, trees, AND traffic/police cars)
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(OfficerAvoid), false, this);
			if (P) TraceParams.AddIgnoredActor(P);
			FCollisionObjectQueryParams AllObjects(FCollisionObjectQueryParams::AllObjects);
			const FCollisionShape FeelerSphere = FCollisionShape::MakeSphere(28.f);

			const float FeelerAngles[7] = { 0.f, -28.f, 28.f, -58.f, 58.f, -95.f, 95.f };
			const float ProbeDist = FMath::Min(165.f, (float)To.Size2D());

			FVector BestDir = DesiredDir;
			float BestScore = -1e9f;

			for (int32 K = 0; K < 7; ++K)
			{
				const FVector TestDir = FRotator(0.0, FeelerAngles[K], 0.0).RotateVector(DesiredDir).GetSafeNormal2D();
				FHitResult Hit;
				const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, Start, Start + TestDir * ProbeDist, FQuat::Identity, AllObjects, FeelerSphere, TraceParams);
				const float ClearFrac = (bHit && Hit.bBlockingHit) ? Hit.Time : 1.0f;

				// Score favors clear distance, alignment with goal, and continuity with previous smoothed direction (prevents left/right oscillation)
				float Score = ClearFrac * 2.2f
					+ FVector::DotProduct(TestDir, DesiredDir) * 0.85f
					+ FVector::DotProduct(TestDir, SmoothDir) * 0.45f;

				if (bHit && Hit.bBlockingHit && ClearFrac < 0.45f)
				{
					Score -= 2.5f;
				}
				if (Score > BestScore)
				{
					BestScore = Score;
					BestDir = TestDir;
					// If straight ahead is 100% clear, take it immediately
					if (K == 0 && ClearFrac >= 0.98f) break;
				}
			}

			// separation: gently steer apart from nearby soldiers (and the leader) so pawns never stack or block each other
			FVector Separation = FVector::ZeroVector;
			for (TActorIterator<AAAAPoliceOfficer> It(GetWorld()); It; ++It)
			{
				if (*It == this || !IsValid(*It) || It->IsDead()) continue;
				const FVector Diff = Me - It->GetActorLocation();
				const float Dist2D = (float)Diff.Size2D();
				if (Dist2D > 1.f && Dist2D < 145.f)
				{
					Separation += Diff.GetSafeNormal2D() * ((145.f - Dist2D) / 145.f);
				}
			}
			if (Team == AAATeam::Player && Lead)
			{
				const FVector Diff = Me - Lead->GetActorLocation();
				const float Dist2D = (float)Diff.Size2D();
				if (Dist2D > 1.f && Dist2D < 170.f) Separation += Diff.GetSafeNormal2D() * ((170.f - Dist2D) / 170.f) * 1.4f;
			}
			if (!Separation.IsNearlyZero())
			{
				BestDir = (BestDir + Separation * 0.65f).GetSafeNormal2D();
			}

			// Smoothly interpolate steering direction for organic human movement
			SmoothDir = FMath::VInterpNormalRotationTo(SmoothDir.IsNearlyZero() ? BestDir : SmoothDir, BestDir, Dt, 380.f);
			AddMovementInput(SmoothDir, 1.f);
			// under fire the run is a sprint; the support gunner with his heavy gun is a bit slower
			float Spd = bRunning ? RunSpeed : WalkSpeed;
			if (Team != AAATeam::Police && bRunning && Suppression > 0.3f) Spd *= 1.12f;
			if (Team != AAATeam::Police && Trade == ETrade::Support) Spd *= 0.93f;
			if (CM) CM->MaxWalkSpeed = Spd;

			StuckT = GetVelocity().Size2D() < 35.f ? StuckT + Dt : FMath::Max(0.f, StuckT - Dt * 0.5f);
			if (StuckT > 1.1f)
			{
				// Un-wedge sidestep: pick a perpendicular escape spot and re-think immediately
				const FVector EscapeSide = FRotator(0.0, FMath::RandBool() ? 95.0 : -95.0, 0.0).RotateVector(DesiredDir);
				SmoothDir = EscapeSide;
				bMoving = false;
				bHasCover = false;
				ThinkT = 0.f;
				StuckT = 0.f;
			}
		}
	}

	// facing: at the target when there is one to look at, else where I run (the player's men: their sector)
	float WantYaw = FacingYaw;
	const bool bLookAtTarget = bHasIntel && (State == EState::Exit || State == EState::Engage || State == EState::Arrest || State == EState::Search || State == EState::Retreat) && !(bRunning && bMoving && !bSees);
	if (bAiding && Pat) WantYaw = (Pat->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
	else if (Team == AAATeam::Player && Order == EAAASquadOrder::Suppress) WantYaw = (OrderPos - GetActorLocation()).Rotation().Yaw;
	else if (CoverFireT > 0.f && !bSees) WantYaw = (CoverFirePos - GetActorLocation()).Rotation().Yaw;
	else if (bLookAtTarget) WantYaw = (ThreatPos() - GetActorLocation()).Rotation().Yaw;
	else if (GetVelocity().Size2D() > 50.f) WantYaw = GetVelocity().Rotation().Yaw;
	else if (Team == AAATeam::Player && Lead)
	{
		// at rest: watch your sector (left / right / rear of the leader's view)
		const float Sector[3] = { -55.f, 55.f, 170.f };
		if (Order == EAAASquadOrder::Follow || Order == EAAASquadOrder::Hold || Order == EAAASquadOrder::Move)
			WantYaw = (float)Lead->GetActorRotation().Yaw + Sector[FMath::Clamp(SquadIndex, 0, 2)] * (Lead->GetVelocity().Size2D() > 60.f ? 0.15f : 1.f);
	}
	FacingYaw = FMath::FixedTurn(FacingYaw, (float)WantYaw, 320.f * Dt);
	SetActorRotation(FRotator(0.0, FacingYaw, 0.0));

	// arrest: a cop next to a still player for a moment
	if (Team == AAATeam::Police && State == EState::Arrest && P && P == Lead && bSees)
	{
		const bool bClose = FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 320.f && P->GetVelocity().Size2D() < 130.f;
		ArrestT = bClose ? ArrestT + Dt : FMath::Max(0.f, ArrestT - Dt);
		if (ArrestT > 2.5f) { if (UAAAWeaponsSubsystem* S = Squad()) if (ATrafficAIManager* Tm = S->Traffic()) Tm->BustPlayer(); ArrestT = 0.f; }
	}
}

void AAAAPoliceOfficer::FireAt(const FVector& AimPoint, float HitChance, ACharacter* Victim)
{
	// a real bullet (the player's rifle component flies it): it hits what is really in the way
	FVector Aim = AimPoint + FMath::VRand() * 10.0;
	if (Victim && FMath::FRand() > HitChance)
	{
		const FVector Dn = (AimPoint - Muzzle()).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Dn, FVector::UpVector).GetSafeNormal();
		Aim = AimPoint + Side * (FMath::FRandRange(55.0, 170.0) * (FMath::RandBool() ? 1.0 : -1.0)) + FVector(0.0, 0.0, FMath::FRandRange(-40.0, 80.0));
	}
	const bool bTac = Team != AAATeam::Police;
	const float Dmg = (bTac && Trade == ETrade::Marksman) ? SoldierDamage * 2.f : SoldierDamage;
	const float Sup = (bTac && Trade == ETrade::Support) ? 0.3f : 0.18f;
	if (UAAAWeaponComponent* W = PlayerWeapon()) W->FireAI(Muzzle(), Aim, Dmg, this, Team, Sup);
}

void AAAAPoliceOfficer::Shoot(float Dt)
{
	if (ThrowT >= 0.f || bDowned) return;
	ShotCD -= Dt;
	if (ReloadT >= 0.f)
	{
		ReloadT += Dt;
		const float RL = Anim ? Anim->GetReloadLength() : 2.2f;
		if (ReloadT >= RL) { ReloadT = -1.f; Ammo = UsesKar() ? 5 : 30; }
		return;
	}
	if (!Rifle || Task == ETask::Revive) return;
	UAAAWeaponsSubsystem* Sq = Squad();
	const bool bTac = Team != AAATeam::Police;

	auto Fired = [&]()
	{
		LastShotT = Now;
		if (Sq) Sq->OnGunshot(Muzzle(), Team);
		FlashT = 0.04f;
		KickV += (bTac && Trade == ETrade::Marksman) ? 20.f : 12.f;
		if (UsesKar())
		{
			SniperT = 0.f; bKarSoundDone = false; bKarCasingDone = false;   // fire + bolt-cycle clip
			ShotCD = FMath::Max(ShotCD, 1.5f);                               // no second shot before the bolt is closed
		}
		ShotSide = FMath::FRandRange(-1.f, 1.f);
		if (Flash) Flash->SetRelativeRotation(FRotator(0.0, 0.0, FMath::FRandRange(0.0, 90.0)));
	};
	// the trade's rhythm: marksman single aimed shots, support long bursts, rifleman short bursts
	auto NextBurst = [&]() -> int32 { return !bTac ? FMath::RandRange(3, 5) : (Trade == ETrade::Marksman ? 1 : (Trade == ETrade::Support ? FMath::RandRange(6, 10) : FMath::RandRange(2, 4))); };
	auto BurstGap = [&]() -> float { return !bTac ? 0.11f : (Trade == ETrade::Support ? 0.085f : 0.1f); };
	auto Pause = [&]() -> float { return (bTac ? (Trade == ETrade::Marksman ? FMath::FRandRange(2.1f, 2.8f) : FMath::FRandRange(0.45f, 1.0f)) : FMath::FRandRange(0.55f, 1.3f)) + Suppression * 0.8f; };

	// cover fire: on the leader's point, or where a mate asked me to keep their heads down - seen or not
	const bool bOrderFire = Team == AAATeam::Player && Order == EAAASquadOrder::Suppress;
	const bool bMateFire = bTac && CoverFireT > 0.f && !bSees && !(Team == AAATeam::Player && bHoldFire);
	if (bOrderFire || bMateFire)
	{
		const FVector Pt = bOrderFire ? OrderPos : CoverFirePos;
		const float YawErrS = FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, (float)(Pt - GetActorLocation()).Rotation().Yaw));
		if (YawErrS > 15.f || ShotCD > 0.f) return;
		if (!bOrderFire && bHasCover && !bPeeking && FVector::Dist2D(GetActorLocation(), Cover) < 60.f) { bPeeking = true; PeekT = 2.5f; return; }
		if (Ammo <= 0) { ReloadT = 0.f; if (Mags > 0) --Mags; return; }
		if (BurstLeft <= 0) BurstLeft = FMath::Max(3, NextBurst());
		--BurstLeft; --Ammo;
		ShotCD = BurstLeft > 0 ? BurstGap() : FMath::FRandRange(0.4f, 0.8f);
		FireAt(Pt + FVector(FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(20.f, 110.f)), 0.f, nullptr);
		Fired();
		return;
	}
	if (State != EState::Engage || !bSees) return;
	if (Team == AAATeam::Player && bHoldFire && Now - LastHurtT > 3.f) return;   // weapons tight (unless shot at)
	if (bHasCover && !bPeeking && FVector::Dist2D(GetActorLocation(), Cover) < 60.f) return;   // tucked in cover
	ACharacter* P = FoeDead() ? nullptr : FoeActor.Get();
	if (!P) return;
	UAAAWeaponComponent* W = PlayerWeapon();
	const bool bAtPlayer = P == Player();
	if (bAtPlayer && (!W || W->IsDead())) return;
	const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, (float)(P->GetActorLocation() - GetActorLocation()).Rotation().Yaw));
	if (YawErr > 18.f || ShotCD > 0.f) return;
	// the player's men: never shoot through the leader or a mate
	if (Team == AAATeam::Player)
	{
		if (const ACharacter* L = Player())
			if (FMath::PointDistToSegment(L->GetActorLocation() + FVector(0.0, 0.0, 30.0), Muzzle(), P->GetActorLocation()) < 90.f) return;
		if (Sq)
		{
			TArray<AAAAPoliceOfficer*> Mates;
			Sq->GetMates(this, Mates);
			for (const AAAAPoliceOfficer* M : Mates) if (FMath::PointDistToSegment(M->GetActorLocation() + FVector(0.0, 0.0, 30.0), Muzzle(), P->GetActorLocation()) < 80.f) return;
		}
	}
	if (Ammo <= 0) { ReloadT = 0.f; if (Mags > 0) --Mags; return; }
	if (BurstLeft <= 0) BurstLeft = NextBurst();
	--BurstLeft; --Ammo;
	ShotCD = BurstLeft > 0 ? BurstGap() : Pause();
	// hit chance: distance (the marksman's falls off much slower), aim settling while tracking, suppression,
	// target movement, partial cover, skill; long bursts climb (the support gunner is no sniper)
	const float Dm = (float)FVector::Dist(P->GetActorLocation(), GetActorLocation()) / 100.f;
	const bool bMk = bTac && Trade == ETrade::Marksman;
	float Hit = 0.62f - Dm / (bMk ? 260.f : 110.f);
	Hit *= 0.3f + 0.7f * FMath::Clamp(TrackT / (bMk ? 1.4f : 2.2f), 0.f, 1.f);
	Hit *= 1.f - 0.6f * Suppression;
	Hit -= (float)P->GetVelocity().Size2D() / 100.f / 25.f;
	if (bHeadOnly) Hit *= 0.45f;
	if (GetVelocity().Size2D() > 250.f) Hit *= 0.5f;   // running and gunning
	if (bTac && Trade == ETrade::Support) Hit *= 0.85f;
	if (bIsCrouched && GetVelocity().Size2D() < 30.f) Hit *= 1.12f;   // braced on a knee
	if (P->bIsCrouched) Hit *= 0.8f;                                  // a smaller target
	Hit = FMath::Clamp(Hit * Skill, 0.03f, 0.8f);
	if (bAtPlayer) W->FireIncoming(Muzzle(), Hit, BulletDamage);
	else
	{
		// the marksman aims for the head
		const double Zaim = (bMk && !bHeadOnly && FMath::FRand() < 0.5f) ? 66.0 : (bHeadOnly ? 62.0 : 25.0);
		FireAt(P->GetActorLocation() + FVector(0.0, 0.0, Zaim), Hit, P);
	}
	Fired();
}

/* ============================================================================================ damage */

void AAAAPoliceOfficer::GoDown(const FVector& Dir)
{
	// the leader's men: down, bleeding, calling for a medic (the death clip held at its end)
	bDowned = true;
	BleedT = BLEED_TIME;
	ReviveProg = 0.f;
	Health = 0.f;
	bMoving = false; bHasCover = false; bPeeking = false; ReloadT = -1.f; ThrowT = -1.f;
	ReviveWho = nullptr;
	SetTask(ETask::None);
	if (UCharacterMovementComponent* CM = GetCharacterMovement()) { CM->StopMovementImmediately(); CM->DisableMovement(); }
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Rifle) Rifle->SetVisibility(false, true);
	DeathImpulse = Dir;
	DeathIdx = Anim ? UAAARifleAnimInstance::PickDeath(HitDirDeg) : INDEX_NONE;
	DeathLen = UAAARifleAnimInstance::GetDeathLength(DeathIdx);
	DeadT = 0.f;
	WritheT = 0.f;
	// he falls as a body: local joint springs follow the death clip while gravity drops the pelvis naturally
	BeginActiveRagdoll(0.85f);
	if (bBlastHit)
	{
		const FVector PushDir = (GetActorLocation() - BlastOrigin + FVector(0.0, 0.0, 65.0)).GetSafeNormal();
		GetMesh()->AddImpulse(PushDir * (260.f + 320.f * BlastProx), NAME_None, true);
	}
	else HitImpulse(Dir, 2800.f);
	if (UAAAWeaponsSubsystem* S = Squad())
	{
		S->Say(this, TEXT("أُصبت! أحتاج مسعفاً!"));
		S->OfficerDown(this);
	}
}

void AAAAPoliceOfficer::AddRevive(float Amount)
{
	if (!bDowned || bDead) return;
	ReviveProg += Amount;
	BleedT = FMath::Max(BleedT, 6.f);   // first aid stops the bleeding for now
	if (ReviveProg >= 1.f)
	{
		bDowned = false;
		ReviveProg = 0.f;
		Health = MaxHealth * 0.45f;
		SnapT = 0.35f;  // the body is pulled back into the lying pose, then the death clip plays backwards
		Suppression = 0.f;
		if (USkeletalMeshComponent* M = GetMesh())
		{
			const FVector PelvisWorld = M->GetBoneLocation(TEXT("pelvis"));
			if (!PelvisWorld.IsNearlyZero())
			{
				FVector GroundPos = PelvisWorld;
				if (GroundAt(PelvisWorld, GroundPos)) SetActorLocation(GroundPos);
			}
		}
		if (Rifle) Rifle->SetVisibility(true, true);
		if (UCharacterMovementComponent* CM = GetCharacterMovement()) CM->SetMovementMode(MOVE_Walking);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Say(TEXT("شكراً، عدت للقتال!"), 0.2f);
	}
}

void AAAAPoliceOfficer::OnBlast(float Prox, const FVector& Origin, float Radius)
{
	BlastOrigin = Origin; BlastR = FMath::Max(Radius, 100.f); BlastProx = FMath::Clamp(Prox, 0.f, 1.f);
	const FVector Push = (GetActorLocation() - Origin + FVector(0.0, 0.0, 80.0)).GetSafeNormal();
	if (bDead || bDowned)
	{
		// the bodies on the ground: pushed (a few m/s), not launched
		if (bPhysBody && GetMesh())
		{
			SetRagdollStrength(0.f);
			GetMesh()->AddImpulse((Push + FVector(0.0, 0.0, 0.35)).GetSafeNormal() * (220.f + 360.f * BlastProx), NAME_None, true);
		}
		return;
	}
	// right on top of it: killed by the blast; further out the radial damage wounds (or downs / kills) him
	const float KillAt = Team == AAATeam::Player ? 0.8f : 0.45f;
	if (Prox < KillAt)
	{
		Suppress(1.f); HitT = 0.f; HitStr = 1.f;
		HitDirDeg = UAAARifleAnimInstance::DirFrom(this, Origin);
		return;
	}
	bBlastHit = true;
	Die(Push * 1000.0);
}

/* ============================================================================================ the body (active ragdoll) */

void AAAAPoliceOfficer::BeginActiveRagdoll(float Strength)
{
	USkeletalMeshComponent* M = GetMesh();
	if (!M) return;
	if (!bPhysBody)
	{
		const FVector PreRagdollVel = GetVelocity();
		UPhysicsAsset* PA = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (PA && !M->GetPhysicsAsset()) M->SetPhysicsAsset(PA, true);

		M->RefreshBoneTransforms();
		M->UpdateComponentToWorld();
		M->bUpdateJointsFromAnimation = true;
		M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		M->bEnableUpdateRateOptimizations = false;
		M->SetCollisionProfileName(TEXT("Ragdoll"));
		M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		M->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		if (!M->GetBodyInstance() || !M->GetBodyInstance()->IsValidBodyInstance()) M->RecreatePhysicsState();

		// Local-space joint springs only (bIsLocalSimulation = true, below pelvis with bIncludeSelf = false):
		// the limbs fold along the death animation while the pelvis and whole body fall freely under real gravity
		// without any world-space position tether suspending or rubber-banding the ragdoll in mid-air!
		if (!PhysAnim)
		{
			PhysAnim = NewObject<UPhysicalAnimationComponent>(this, TEXT("ActiveRagdoll"));
			PhysAnim->RegisterComponent();
		}
		PhysAnim->SetSkeletalMeshComponent(M);
		FPhysicalAnimationData D;
		D.bIsLocalSimulation = true;
		D.OrientationStrength = 520.f;
		D.AngularVelocityStrength = 48.f;
		D.PositionStrength = 0.f;
		D.VelocityStrength = 0.f;
		D.MaxLinearForce = 0.f;
		D.MaxAngularForce = 0.f;
		PhysAnim->ApplyPhysicalAnimationSettingsBelow(TEXT("pelvis"), D, false);

		M->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), true, true);
		M->SetAllBodiesBelowPhysicsBlendWeight(TEXT("pelvis"), 1.f, false, true);

		// Joint and body damping so limbs settle naturally with human weight instead of jittering or spinning
		for (FBodyInstance* BI : M->Bodies)
		{
			if (!BI) continue;
			BI->LinearDamping = 0.18f;
			BI->AngularDamping = 1.45f;
			BI->UpdateDampingProperties();
		}
		M->WakeAllRigidBodies();
		if (!PreRagdollVel.IsNearlyZero())
		{
			M->SetPhysicsLinearVelocity(PreRagdollVel * 0.65f, false);
		}
		bPhysBody = true;
		bRagdolled = true;
		PhysK = -1.f;
	}
	SetRagdollStrength(Strength);
}

void AAAAPoliceOfficer::SetRagdollStrength(float K)
{
	K = FMath::Max(0.f, K);
	if (PhysAnim && bPhysBody && (FMath::Abs(K - PhysK) > 0.005f || (K == 0.f && PhysK != 0.f)))
	{
		PhysAnim->SetStrengthMultiplyer(K);
		PhysK = K;
	}
}

void AAAAPoliceOfficer::EndActiveRagdoll()
{
	USkeletalMeshComponent* M = GetMesh();
	if (!M || !bPhysBody) return;
	if (PhysAnim) PhysAnim->SetStrengthMultiplyer(0.f);
	M->PutAllRigidBodiesToSleep();
	M->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), false, true);
	M->SetSimulatePhysics(false);
	M->SetCollisionProfileName(TEXT("CharacterMesh"));
	M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (UCapsuleComponent* CapComp = GetCapsuleComponent())
	{
		M->AttachToComponent(CapComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	M->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -92.0), FRotator(0.0, -90.0, 0.0));
	M->RefreshBoneTransforms();
	M->UpdateComponentToWorld();
	bPhysBody = false;
	bRagdolled = false;
	PhysK = -1.f;
}

void AAAAPoliceOfficer::HitImpulse(const FVector& Dir, float Amount)
{
	USkeletalMeshComponent* M = GetMesh();
	if (!M || !bPhysBody || Dir.IsNearlyZero()) return;
	const FVector ShotDir = Dir.GetSafeNormal();
	const FName Bone = (LastHitBone.IsNone() || M->GetBoneIndex(LastHitBone) == INDEX_NONE) ? FName(TEXT("spine_03")) : LastHitBone;
	const FVector At = LastHitPoint.IsNearlyZero() ? M->GetBoneLocation(Bone) : LastHitPoint;
	// Impulse at the exact struck bone + a natural center-of-mass knockback on the torso/pelvis
	M->AddImpulseAtLocation(ShotDir * Amount, At, Bone);
	M->AddImpulse((ShotDir + FVector(0.0, 0.0, 0.15)).GetSafeNormal() * FMath::Clamp(Amount * 0.065f, 90.f, 220.f), TEXT("pelvis"), true);
}

float AAAAPoliceOfficer::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead) return 0.f;
	float Dmg = DamageAmount;
	FVector Dir = GetActorForwardVector() * -1.0;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Pd = static_cast<const FPointDamageEvent&>(DamageEvent);
		Dir = Pd.ShotDirection;
		LastHitBone = Pd.HitInfo.BoneName;
		LastHitPoint = Pd.HitInfo.ImpactPoint;
		const float Top = (float)(GetActorLocation().Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (!bDowned && (Pd.HitInfo.ImpactPoint.Z > Top - 28.f || LastHitBone == FName(TEXT("head")))) Dmg *= 3.f;   // headshot
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent& Rd = static_cast<const FRadialDamageEvent&>(DamageEvent);
		Dir = (GetActorLocation() - Rd.Origin + FVector(0.0, 0.0, 75.0)).GetSafeNormal();
		Dmg *= 1.35f;
		BlastOrigin = Rd.Origin;
		BlastProx = FMath::Clamp(1.f - (float)FVector::Dist(Rd.Origin, GetActorLocation()) / FMath::Max(BlastR, 100.f), 0.f, 1.f);
	}
	else if (DamageCauser)
	{
		Dir = (GetActorLocation() - DamageCauser->GetActorLocation() + FVector(0.0, 0.0, 75.0)).GetSafeNormal();
	}
	// who did it: the pawn behind the controller, else the causer (a soldier's bullet, a grenade's thrower)
	AActor* Att = (EventInstigator && EventInstigator->GetPawn()) ? EventInstigator->GetPawn() : DamageCauser;
	if (AAAAGrenade* G = Cast<AAAAGrenade>(Att)) Att = G->GetInstigator() ? (AActor*)G->GetInstigator() : G->GetOwner();
	UAAAWeaponsSubsystem* S = Squad();
	const uint8 AttTeam = S ? S->TeamOf(Att) : (uint8)255;
	// friendly fire: the leader's rounds only graze his own men (and they tell him off)
	if (Team == AAATeam::Player && AttTeam == AAATeam::Player)
	{
		Dmg *= 0.2f;
		Say(TEXT("انتبه! أنا من جماعتك!"), 2.f);
	}
	// a downed man: only a blast finishes him
	if (bDowned)
	{
		if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && Dmg > 40.f) { bBlastHit = true; Die(Dir * 1600.0); }
		return 0.f;
	}
	Health -= Dmg;
	LastHurtT = Now;
	if (bPeeking) bHurtWhilePeeking = true;
	HitT = 0.f; HitIndex = FMath::RandRange(0, 3);
	HitDirDeg = UAAARifleAnimInstance::DirFrom(this, GetActorLocation() - Dir.GetSafeNormal() * 500.0);   // the shot came from -Dir
	HitStr = FMath::Clamp(Dmg / 40.f, 0.2f, 1.f);
	bBlastHit = DamageEvent.IsOfType(FRadialDamageEvent::ClassID);
	Suppress(0.6f);
	if (ACharacter* AC = Cast<ACharacter>(Att))
	{
		if (S && AC != this && Hostile(Team, AttTeam, FMath::Max(1, S->WantedStars())))
		{
			FoeActor = AC;
			LastKnown = AC->GetActorLocation(); LastHeard = Now; bHasIntel = true; Awareness = 1.f;
			if (Team != AAATeam::Police)
			{
				Learn(AC, AC->GetActorLocation(), true, false);
				Say(FString::Printf(TEXT("يطلقون علي! %s"), *Bearing(AC->GetActorLocation())), 3.f);
			}
		}
	}
	if (Health <= 0.f)
	{
		if (Team == AAATeam::Player && Dmg < 160.f) GoDown(Dir * 1600.0);
		else Die(Dir * 1600.0);
	}
	return Dmg;
}

void AAAAPoliceOfficer::Die(const FVector& Dir)
{
	if (bDead) return;
	const bool bWasDowned = bDowned;
	bDead = true; bDowned = false; bMoving = false;
	if (UAAAWeaponsSubsystem* S = Squad())
	{
		if (!bWasDowned) S->OfficerDown(this);
		if (Team == AAATeam::Police) if (ATrafficAIManager* Tm = S->Traffic()) Tm->ReportOfficerDown();
	}
	if (UCharacterMovementComponent* CM = GetCharacterMovement()) { CM->StopMovementImmediately(); CM->DisableMovement(); }
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Remove all weapons and items when dead (destroy children explicitly so DestroyComponent does not promote them onto GetMesh)
	if (FlashLight) { FlashLight->DestroyComponent(false); FlashLight = nullptr; }
	if (Flash) { Flash->DestroyComponent(false); Flash = nullptr; }
	if (Rifle)
	{
		TArray<USceneComponent*> RifleChildren;
		Rifle->GetChildrenComponents(true, RifleChildren);
		for (USceneComponent* Child : RifleChildren)
		{
			if (Child) Child->DestroyComponent(false);
		}
		Rifle->DestroyComponent(false);
		Rifle = nullptr;
	}
	if (Cap) { Cap->DestroyComponent(false); Cap = nullptr; }
	if (Brim) { Brim->DestroyComponent(false); Brim = nullptr; }

	DeathImpulse = Dir;
	// already lying there (bled out): the body just goes slack where it lies
	if (bWasDowned && !bBlastHit)
	{
		DeadT = FMath::Max(DeathLen, 0.1f);
		BeginActiveRagdoll(0.f);
		return;
	}
	DeadT = 0.f;
	// shot: the death clip for the side the hit came from drives the falling body's local joints (springs fading out),
	// while gravity and the bullet impulse move the pelvis; blast: a limp body thrown by the shockwave
	DeathIdx = (Anim && !bBlastHit) ? UAAARifleAnimInstance::PickDeath(HitDirDeg) : INDEX_NONE;
	DeathLen = UAAARifleAnimInstance::GetDeathLength(DeathIdx);
	if (DeathLen < 0.2f) DeathIdx = INDEX_NONE;
	BeginActiveRagdoll((DeathIdx != INDEX_NONE && !bBlastHit) ? 0.75f : 0.f);
	if (bBlastHit)
	{
		const FVector PushDir = (GetActorLocation() - BlastOrigin + FVector(0.0, 0.0, 65.0)).GetSafeNormal();
		GetMesh()->AddImpulse(PushDir * (340.f + 460.f * BlastProx), NAME_None, true);
		GetMesh()->AddRadialImpulse(BlastOrigin - FVector(0.0, 0.0, 60.0), BlastR, 260.f + 340.f * BlastProx, ERadialImpulseFalloff::RIF_Linear, true);
	}
	else
	{
		HitImpulse(Dir, 3200.f);
	}
}

void AAAAPoliceOfficer::StartRagdoll(const FVector& Dir)
{
	// a limp body (kept for callers outside: same physics as the death fall, strength 0)
	BeginActiveRagdoll(0.f);
	HitImpulse(Dir, FMath::Clamp((float)Dir.Size(), 1800.f, 3400.f));
}

/* ============================================================================================ frame */

void AAAAPoliceOfficer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.05f);
	Now = (float)GetWorld()->GetTimeSeconds();
	if (bDead)
	{
		DeadT += Dt;
		// local joint springs: the death clip shapes the fall for the first ~65% of the clip, then fades to 0 so the body settles limp on the ground
		float K = 0.f;
		if (!bBlastHit && DeathIdx != INDEX_NONE && DeathLen > 0.2f)
		{
			const float FadeEnd = DeathLen * 0.75f;
			K = (DeadT < FadeEnd) ? FMath::Lerp(0.75f, 0.f, DeadT / FadeEnd) : 0.f;
		}
		SetRagdollStrength(K);
		if (Anim)
		{
			Anim->Inputs.DeathIndex = DeathIdx;
			Anim->Inputs.DeathTime = DeathIdx != INDEX_NONE ? FMath::Min(DeadT, FMath::Max(DeathLen - 0.03f, 0.05f)) : -1.f;
			Anim->Inputs.LocalVel = FVector2D::ZeroVector;
			Anim->Inputs.bFalling = false;
		}
		if (DeadT > 30.f && CanDespawn()) Destroy();
		return;
	}
	if (bDowned)
	{
		// lying there (the death clip held at its last frame), bleeding out
		DeadT += Dt;
		BleedT -= Dt;
		WritheT += Dt;
		const float Fall = FMath::Max(DeathLen - 0.05f, 0.05f);
		float Pose = FMath::Min(DeadT, Fall);
		if (DeadT > Fall) Pose = Fall - 0.15f * (0.5f + 0.5f * FMath::Sin(WritheT * 1.4f)) * FMath::Clamp((BleedT - 3.f) / 20.f, 0.15f, 1.f);
		SetRagdollStrength(DeadT < Fall ? FMath::Lerp(0.75f, 0.28f, DeadT / Fall) : 0.28f);
		if (DeadT > Fall && FMath::FRand() < Dt * 0.6f && GetMesh() && bPhysBody)
		{
			static const TCHAR* const Limbs[5] = { TEXT("hand_l"), TEXT("hand_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("head") };
			const FName B(Limbs[FMath::RandRange(0, 4)]);
			if (GetMesh()->GetBoneIndex(B) != INDEX_NONE) GetMesh()->AddImpulse(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.2f, 1.f)) * 55.f, B, true);
		}
		if (Anim)
		{
			Anim->Inputs.DeathTime = DeathIdx != INDEX_NONE ? FMath::Max(Pose, 0.02f) : -1.f;
			Anim->Inputs.DeathIndex = DeathIdx;
			Anim->Inputs.LocalVel = FVector2D::ZeroVector;
			Anim->Inputs.bFalling = false;
		}
		if (BleedT < 15.f) Say(TEXT("أنزف... بسرعة!"), 9.f);
		if (BleedT <= 0.f) Die(FVector::ZeroVector);
		return;
	}
	if (!bGear) BuildGear();
	if (Cap && !bCapPlaced) bCapPlaced = true;
	StateT += Dt;
	GrenadeCD -= Dt;
	if (EvadeT > 0.f) EvadeT -= Dt;
	// the player's men get their breath back out of the fight
	if (Team == AAATeam::Player && Now - LastHurtT > 8.f && Health < MaxHealth) Health = FMath::Min(MaxHealth, Health + 4.f * Dt);
	Perceive(Dt);
	if ((ThinkT -= Dt) <= 0.f) { ThinkT = FMath::FRandRange(0.25f, 0.35f); Think(); }
	if (SnapT >= 0.f)
	{
		// first aid done: the springs pull the body firmly into the lying pose, then the bones stop simulating
		SnapT -= Dt;
		SetRagdollStrength(2.5f);
		if (SnapT < 0.f) { EndActiveRagdoll(); GetUpT = 0.f; }
	}
	if (GetUpT < 0.f && SnapT < 0.f) { Act(Dt); Shoot(Dt); }
	UpdateStance(Dt);
	// weapon spring, flash
	KickV += (-Kick * 260.f - KickV * 26.f) * Dt;
	Kick = FMath::Max(0.f, Kick + KickV * Dt);
	FlashT -= Dt;
	const bool bFlash = FlashT > 0.f;
	if (Flash && Flash->IsVisible() != bFlash) Flash->SetVisibility(bFlash);
	if (FlashLight && (FlashLight->Intensity > 0.f) != bFlash) FlashLight->SetIntensity(bFlash ? 5000.f : 0.f);
	if (HitT >= 0.f) { HitT += Dt; if (HitT > 1.2f) HitT = -1.f; }

	// the police's grenade decision (the tactical sides decide it in ThinkTactical)
	ACharacter* GF = FoeDead() ? nullptr : FoeActor.Get();
	if (Team == AAATeam::Police && Awareness > 0.5f && ThrowT < 0.f && Grenades > 0 && GrenadeCD <= 0.f && GF)
	{
		const float Dist = (float)FVector::Dist(GetActorLocation(), GF->GetActorLocation());
		if (Dist >= 900.f && Dist <= 3200.f && (!bSees || FMath::FRand() < 0.35f))
		{
			ThrowT = 0.f;
			bGrenadeSpawned = false;
			GrenadeCD = FMath::FRandRange(16.f, 26.f);
			TargetThrowLoc = GF->GetActorLocation() + FVector(FMath::FRandRange(-100.f, 100.f), FMath::FRandRange(-100.f, 100.f), 0.f);
		}
	}
	if (ThrowT >= 0.f)
	{
		ThrowT += Dt;
		if (ThrowT >= 0.80f && !bGrenadeSpawned)   // release point of the throw animation
		{
			bGrenadeSpawned = true;
			--Grenades;
			if (UWorld* W = GetWorld())
			{
				const FVector StartLoc = GetMesh() ? GetMesh()->GetSocketLocation(TEXT("hand_l")) : GetActorLocation() + FVector(0.0, 0.0, 60.0);
				FVector LaunchVel;
				if (!UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, LaunchVel, StartLoc, TargetThrowLoc, 0.f, 0.42f))
				{
					LaunchVel = (TargetThrowLoc - StartLoc).GetSafeNormal() * 1400.f + FVector(0.0, 0.0, 480.0);
				}
				FActorSpawnParameters P;
				P.Owner = this;
				P.Instigator = this;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AAAAGrenade* G = W->SpawnActor<AAAAGrenade>(StartLoc, FRotator::ZeroRotator, P))
				{
					G->Launch(LaunchVel, this, 3.2f);
				}
			}
		}
		if (ThrowT >= 1.60f)
		{
			ThrowT = -1.f;
			bGrenadeSpawned = false;
		}
	}

	// animation inputs (the same rifle animation as the player)
	if (Anim)
	{
		FAAARifleInputs& I = Anim->Inputs;
		const FVector V = GetVelocity();
		I.LocalVel = FVector2D(FVector::DotProduct(V, GetActorForwardVector()), FVector::DotProduct(V, GetActorRightVector()));
		const bool bOrderSup = Team == AAATeam::Player && Order == EAAASquadOrder::Suppress;
		const bool bSup = bOrderSup || (CoverFireT > 0.f && !bSees);
		const FVector Aimed = bOrderSup ? OrderPos : CoverFirePos;
		const FVector Tgt = (bSup ? Aimed + FVector(0.0, 0.0, 60.0) : ThreatPos()) - (GetActorLocation() + FVector(0.0, 0.0, EYES));
		I.AimPitch = (bHasIntel || bSup) ? FMath::Clamp((float)FMath::RadiansToDegrees(FMath::Atan2(Tgt.Z, Tgt.Size2D())), -45.f, 45.f) : -8.f;
		I.Kick = Kick;
		I.KickSide = ShotSide;
		// first aid uses the reload's hand work (both hands busy at the body)
		const bool bAidAnim = Task == ETask::Revive && !bMoving;
		I.ReloadTime = bAidAnim ? FMath::Fmod(StateT, 1.6f) + 0.3f : ReloadT;
		// drawing the rifle while getting out of the car (the equip clip on the upper body), then the normal pose
		const float SinceBorn = Now - BornT;
		I.EquipTime = (SinceBorn < Anim->GetEquipLength()) ? SinceBorn : -1.f;
		I.HitTime = HitT;
		I.HitIndex = HitIndex;
		I.HitDir = HitDirDeg;
		I.HitStrength = HitStr;
		I.VelZ = (float)V.Z;
		I.DeathTime = -1.f;
		if (SnapT >= 0.f) { I.DeathIndex = DeathIdx; I.DeathTime = DeathIdx != INDEX_NONE ? FMath::Max(DeathLen - 0.05f, 0.05f) : -1.f; }
		// getting up after first aid: the death clip played backwards
		if (GetUpT >= 0.f)
		{
			GetUpT += Dt;
			const float K = FMath::Clamp(GetUpT / GETUP_TIME, 0.f, 1.f);
			I.DeathIndex = DeathIdx;
			I.DeathTime = K < 1.f ? FMath::Max(0.001f, (1.f - K) * FMath::Max(DeathLen - 0.05f, 0.05f)) : -1.f;
			if (K >= 1.f) GetUpT = -1.f;
		}
		I.ThrowTime = ThrowT;
		// the marksman's Kar98k: fire + bolt cycle (the bolt part follows the same timeline as the animation)
		if (SniperT >= 0.f)
		{
			SniperT += Dt;
			if (SniperT >= 1.45f) SniperT = -1.f;
		}
		I.SniperFireTime = (UsesKar() && Rifle && ReloadT < 0.f) ? SniperT : -1.f;
		I.bKarRifle = UsesKar();
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
		I.CrouchWeight = CrouchTgt;
		I.LeanAmount = LeanTgt;
		I.bFalling = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
		I.bJumpUp = false;
		I.bAiming = true;
		I.bLeftHandIK = true;
	}
	else GetUpT = -1.f;
}

void AAAAPoliceOfficer::EvadeGrenade(const FVector& Where, float TimeLeft, float Radius)
{
	if (IsOut() || ThrowT >= 0.f || GetUpT >= 0.f || SnapT >= 0.f) return;
	if (EvadeT > 0.f && FVector::Dist2D(Where, EvadeFrom) < 150.f) return;   // already running from this one
	const FVector Me = GetActorLocation();
	const float D = (float)FVector::Dist2D(Me, Where);
	// too late to get clear: nothing to gain by moving, he turns away and braces (the blast decides)
	const float Need = Radius + 150.f - D;
	if (Need <= 0.f) return;
	if (TimeLeft < Need / 520.f * 0.5f) { Say(TEXT("انبطح!"), 2.f); return; }
	EvadeFrom = Where;
	EvadeT = FMath::Clamp(TimeLeft + 0.5f, 0.8f, 4.f);
	bHasCover = false; bPeeking = false;
	FVector Away = (Me - Where).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = -GetActorForwardVector();
	// straight away is best, else the nearest angle with a clear path (cover between him and it is even better)
	const float Angles[] = { 0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f };
	FVector Best = Me + Away * Need;
	float BestScore = -1e9f;
	for (float A : Angles)
	{
		const FVector Dir = FRotator(0.0, A, 0.0).RotateVector(Away);
		FVector P;
		if (!GroundAt(Me + Dir * (double)(Need + 80.f), P)) continue;
		if (!PathClear(Me, P)) continue;
		float Score = (float)FVector::Dist2D(P, Where) - FMath::Abs(A) * 2.f;
		if (!LineClear(Where + FVector(0.0, 0.0, 30.0), P + FVector(0.0, 0.0, 60.0))) Score += 400.f;   // something solid shields him
		if (Score > BestScore) { BestScore = Score; Best = P; }
	}
	MoveTo(Best, true);
	Say(Team == AAATeam::Player ? TEXT("قنبلة! ابتعدوا!") : TEXT("قنبلة!"), 1.f);
}

void AAAAPoliceOfficer::UpdateStance(float Dt)
{
	UCharacterMovementComponent* CM = GetCharacterMovement();
	const FVector Me = GetActorLocation();
	StillT = GetVelocity().Size2D() < 30.f ? StillT + Dt : 0.f;
	float Cr = 0.f, Ln = 0.f;
	const bool bBusy = IsOut() || ThrowT >= 0.f || GetUpT >= 0.f || SnapT >= 0.f || EvadeT > 0.f || State == EState::Exit || (CM && CM->IsFalling());
	if (!bBusy)
	{
		const bool bTucked = bHasCover && !bMoving && !bPeeking && FVector::Dist2D(Me, Cover) < 80.f;
		// tucked in: behind low cover always down; behind a wall down when the rounds come close or he reloads
		if (bTucked && (bCoverLow || Suppression > 0.35f || ReloadT >= 0.f)) Cr = 1.f;
		if (bHasCover && bPeeking && !bMoving)
		{
			if (bPeekOver) Cr = 0.f;   // up over the top to fire
			else
			{
				const FVector Spot = bPeekSideB ? PeekB : Peek;
				const FVector Right = FRotator(0.0, FacingYaw, 0.0).RotateVector(FVector(0.0, 1.0, 0.0));
				const double Sd = FVector::DotProduct(Spot - Cover, Right);
				if (FMath::Abs(Sd) > 20.0) Ln = (Sd > 0.0 ? 0.8f : -0.8f) * FMath::Clamp(1.f - (float)FVector::Dist2D(Me, Spot) / 120.f, 0.35f, 1.f);
				Cr = bLowPeek ? 1.f : 0.f;
			}
		}
		// first aid: on his knees next to the wounded man
		if (Task == ETask::Revive && !bMoving && ReviveWho.IsValid() && FVector::Dist2D(ReviveWho->GetActorLocation(), Me) < 160.f) Cr = 1.f;
		// halted in the open (the leader stopped, a patrol halt, overwatch): take a knee if he still sees from there
		if (Team != AAATeam::Police && !bHasCover && !bMoving && StillT > 1.f && bKneelOK && Task != ETask::CoverFire) Cr = 1.f;
		// a marksman shooting from the open always kneels
		if (Team != AAATeam::Police && Trade == ETrade::Marksman && !bHasCover && !bMoving && bSees && bKneelOK) Cr = 1.f;
	}
	CrouchTgt = Cr;
	LeanTgt = Ln;
	// capsule with hysteresis: short capsule = his eyes and the incoming rounds really pass lower
	// (a body on the ground keeps its capsule: changing it would shift the mesh under the ragdoll)
	if (!IsOut())
	{
		if (Cr > 0.6f && !bIsCrouched && CM && !CM->IsFalling()) Crouch();
		else if (Cr < 0.4f && bIsCrouched) UnCrouch();
	}
	if (CM) CM->MaxWalkSpeedCrouched = Trade == ETrade::Support ? 160.f : 170.f;
}
