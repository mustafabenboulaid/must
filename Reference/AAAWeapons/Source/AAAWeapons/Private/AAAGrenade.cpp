#include "AAAGrenade.h"
#include "AAAShapes.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "TrafficAIManager.h"
#include "DrawDebugHelpers.h"
#include "AAAPoliceOfficer.h"
#include "AAAWeaponComponent.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	USoundAttenuation* GetGrenade3DBlastAttenuation()
	{
		static USoundAttenuation* BlastAtten = nullptr;
		if (!BlastAtten)
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AAA_Grenade3DBlastAtten"));
			A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
			S.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.AttenuationShapeExtents = FVector(320.f, 0.f, 0.f);
			S.FalloffDistance = 16000.f; // 160m blast carry
			S.dBAttenuationAtMax = -52.f;
			S.StereoSpread = 75.f;
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = 600.f;
			S.LPFRadiusMax = 12000.f;
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = 1200.f;
			S.bEnableOcclusion = true;
			S.OcclusionTraceChannel = ECC_Visibility;
			S.OcclusionLowPassFilterFrequency = 950.f;
			S.OcclusionVolumeAttenuation = 0.55f;
			BlastAtten = A;
		}
		return BlastAtten;
	}

	USoundAttenuation* GetGrenade3DBounceAttenuation()
	{
		static USoundAttenuation* BounceAtten = nullptr;
		if (!BounceAtten)
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AAA_Grenade3DBounceAtten"));
			A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
			S.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.AttenuationShapeExtents = FVector(55.f, 0.f, 0.f);
			S.FalloffDistance = 3000.f;
			S.dBAttenuationAtMax = -60.f;
			S.StereoSpread = 30.f;
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = 100.f;
			S.LPFRadiusMax = 2500.f;
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = 3000.f;
			S.bEnableOcclusion = true;
			BounceAtten = A;
		}
		return BounceAtten;
	}
}

AAAAGrenade::AAAAGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	Collision->InitSphereRadius(7.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->BodyInstance.bNotifyRigidBodyCollision = true;
	// (no mass override here: projectile movement moves it, and setting a body mass in the constructor logs
	//  "GetSimplePhysicalMaterial : GEngine not initialized" for the class default object)
	SetRootComponent(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 3500.f;
	Movement->bRotationFollowsVelocity = false;   // it tumbles by itself (TickSpinAndSpoon)
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.42f;
	Movement->Friction = 0.55f;
	Movement->BounceVelocityStopSimulatingThreshold = 35.f;
	Movement->ProjectileGravityScale = 1.0f;
}

void AAAAGrenade::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentHit.AddDynamic(this, &AAAAGrenade::OnGrenadeHit);
	BuildProceduralMesh();

	// Load fallback explosion and bounce SFX from engine/game content
	if (!ExplosionSound)
	{
		ExplosionSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/AAACity/Sounds/rifle_shot.rifle_shot"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!BounceSound)
	{
		BounceSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/AAACity/Sounds/bullet_impact.bullet_impact"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void AAAAGrenade::BuildProceduralMesh()
{
	UMaterialInstanceDynamic* Olive = AAAShapes::Mat(this, FLinearColor(0.12f, 0.16f, 0.08f));
	UMaterialInstanceDynamic* DarkMetal = AAAShapes::Mat(this, FLinearColor(0.04f, 0.04f, 0.045f));
	UMaterialInstanceDynamic* Steel = AAAShapes::Mat(this, FLinearColor(0.25f, 0.25f, 0.26f));

	// 1. Grenade body (egg/pineapple shape)
	UStaticMeshComponent* Body = AAAShapes::Make(this, false, FVector(7.5, 7.5, 9.5), Olive, false, true);
	Body->SetupAttachment(Collision);
	Body->RegisterComponent();
	Meshes.Add(Body);

	// 2. Fuze neck (cylinder)
	UStaticMeshComponent* Fuze = AAAShapes::Make(this, true, FVector(2.8, 2.8, 4.0), DarkMetal, false, true);
	Fuze->SetupAttachment(Collision);
	Fuze->SetRelativeLocation(FVector(0.0, 0.0, 5.5));
	Fuze->RegisterComponent();
	Meshes.Add(Fuze);

	// 3. Safety lever / spoon
	UStaticMeshComponent* Lever = AAAShapes::Make(this, false, FVector(1.4, 0.8, 8.5), Steel, false, true);
	Lever->SetupAttachment(Collision);
	Lever->SetRelativeLocation(FVector(2.2, 0.0, 1.5));
	Lever->SetRelativeRotation(FRotator(-8.0, 0.0, 0.0));
	Lever->RegisterComponent();
	Spoon = Lever;   // not in Meshes: it leaves the grenade on release and stays visible after the blast
}

void AAAAGrenade::Launch(const FVector& Velocity, ACharacter* InThrower, float InFuseTime)
{
	Thrower = InThrower;
	FuseTimer = FMath::Clamp(InFuseTime, 0.3f, 4.0f);
	bLaunched = true;

	if (InThrower)
	{
		Collision->IgnoreActorWhenMoving(InThrower, true);
	}

	if (Movement)
	{
		Movement->Velocity = Velocity;
		Movement->UpdateComponentVelocity();
	}

	// end-over-end tumble from the wrist snap (axis across the throw, a little random wobble)
	const FVector Dir = Velocity.GetSafeNormal2D();
	SpinAxis = Dir.IsNearlyZero() ? FVector(0.0, 1.0, 0.0) : FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
	SpinAxis = (SpinAxis + FMath::VRand() * 0.25).GetSafeNormal();
	SpinRate = Velocity.Size() > 50.0 ? FMath::FRandRange(520.f, 860.f) : 0.f;

	// the spoon: the striker spring flicks it off the moment the hand opens - it flies off sideways with a ping
	if (Spoon && !bSpoonFree)
	{
		bSpoonFree = true;
		Spoon->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		const FVector Side = Dir.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		SpoonVel = Velocity * 0.45 + Side * FMath::FRandRange(180.f, 300.f) + FVector(0.0, 0.0, FMath::FRandRange(220.f, 340.f));
		SpoonSpinAxis = FMath::VRand();
		SpoonSpin = FMath::FRandRange(1400.f, 2200.f);
		if (BounceSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, BounceSound, Spoon->GetComponentLocation(), FRotator::ZeroRotator, 0.32f, 2.6f, 0.f, GetGrenade3DBounceAttenuation());
		}
	}
}

void AAAAGrenade::TickSpinAndSpoon(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) return;
	const float NowT = (float)W->GetTimeSeconds();
	if (!bExploded && Movement)
	{
		const FVector V = Movement->Velocity;
		const bool bOnGround = NowT - LastHitT < 0.2f && FMath::Abs(V.Z) < 120.0;
		if (bOnGround && V.Size2D() > 5.0)
		{
			// rolling: the spin locks to the ground speed (radius ~7 cm), the egg shape makes it wobble
			const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, V.GetSafeNormal2D()).GetSafeNormal();
			SpinAxis = FMath::Lerp(SpinAxis, RollAxis, FMath::Min(1.f, Dt * 10.f)).GetSafeNormal();
			SpinRate = FMath::RadiansToDegrees((float)V.Size2D() / 7.f) * (1.f + 0.15f * FMath::Sin(NowT * 21.f));
		}
		else if (bOnGround)
		{
			SpinRate = FMath::FInterpTo(SpinRate, 0.f, Dt, 6.f);
		}
		if (SpinRate > 1.f && !SpinAxis.IsNearlyZero())
		{
			AddActorWorldRotation(FQuat(SpinAxis, FMath::DegreesToRadians(SpinRate * Dt)));
		}
	}

	if (bSpoonFree && !bSpoonRest && Spoon)
	{
		SpoonVel.Z -= 980.0 * Dt;
		SpoonVel *= 1.0 - 0.35 * Dt;   // a flat strip of steel: plenty of air drag
		const FVector From = Spoon->GetComponentLocation();
		FVector To = From + SpoonVel * Dt;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(GrenadeSpoon), false, this);
		if (Thrower.IsValid()) Q.AddIgnoredActor(Thrower.Get());
		if (W->LineTraceSingleByChannel(H, From, To, ECC_WorldStatic, Q))
		{
			To = H.ImpactPoint + H.ImpactNormal * 1.0;
			const double In = FVector::DotProduct(SpoonVel, H.ImpactNormal);
			SpoonVel = (SpoonVel - H.ImpactNormal * In * 1.35) * 0.45;   // tinny bounce, most energy lost
			SpoonSpin *= 0.4f;
			SpoonSpinAxis = (SpoonSpinAxis + FMath::VRand() * 0.6).GetSafeNormal();
			if (BounceSound && In < -120.0)
			{
				UGameplayStatics::PlaySoundAtLocation(this, BounceSound, To, FRotator::ZeroRotator, 0.18f, 2.9f, 0.f, GetGrenade3DBounceAttenuation());
			}
			if (SpoonVel.Size() < 45.0)
			{
				bSpoonRest = true;
				// lies flat on the ground
				Spoon->SetWorldRotation(FRotationMatrix::MakeFromZX(H.ImpactNormal, FVector::CrossProduct(H.ImpactNormal, FVector(0.0, 1.0, 0.0)).GetSafeNormal()).Rotator());
			}
		}
		Spoon->SetWorldLocation(To);
		if (!bSpoonRest) Spoon->AddWorldRotation(FQuat(SpoonSpinAxis, FMath::DegreesToRadians(SpoonSpin * Dt)));
	}
}

void AAAAGrenade::WarnNearby()
{
	// soldiers who see it land near them (or hear it clatter close by) scatter - the thrower and his mates
	// included if it falls short. Not instant: it has to be in the air a moment to be noticed.
	UWorld* W = GetWorld();
	if (!W) return;
	const FVector G = GetActorLocation();
	for (TActorIterator<AAAAPoliceOfficer> It(W); It; ++It)
	{
		AAAAPoliceOfficer* O = *It;
		if (!O || O->IsOut()) continue;
		const float D = (float)FVector::Dist(O->GetActorLocation(), G);
		if (D > BlastRadius * 1.15f) continue;
		bool bNotice = D < 350.f;
		if (!bNotice)
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(GrenadeSeen), false, this);
			Q.AddIgnoredActor(O);
			bNotice = !W->LineTraceSingleByChannel(H, O->GetActorLocation() + FVector(0.0, 0.0, 65.0), G + FVector(0.0, 0.0, 10.0), ECC_Visibility, Q);
		}
		if (bNotice) O->EvadeGrenade(G, FuseTimer, BlastRadius);
	}
}

void AAAAGrenade::OnGrenadeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExploded) return;
	const float Now = (float)GetWorld()->GetTimeSeconds();
	// each knock steals spin and tips the axis (the egg-shaped body skips rather than rolls cleanly)
	if (Now - LastHitT > 0.08f && Movement && Movement->Velocity.Size() > 150.0)
	{
		SpinRate *= 0.55f;
		SpinAxis = (SpinAxis + FMath::VRand() * 0.5).GetSafeNormal();
	}
	LastHitT = Now;
	if (Now - LastBounceSoundT > 0.12f && (NormalImpulse.Size() > 80.f || (Movement && Movement->Velocity.Size() > 120.f)))
	{
		LastBounceSoundT = Now;
		if (BounceSound)
		{
			BounceSound->AttenuationSettings = GetGrenade3DBounceAttenuation();
			UGameplayStatics::PlaySoundAtLocation(this, BounceSound, Hit.ImpactPoint, FRotator::ZeroRotator, 0.72f, FMath::FRandRange(1.3f, 1.6f), 0.f, GetGrenade3DBounceAttenuation());
		}
	}
}

void AAAAGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bExploded)
	{
		TickSpinAndSpoon(DeltaSeconds);
		if (bLaunched)
		{
			FuseTimer -= DeltaSeconds;
			FlightT += DeltaSeconds;
			if (FlightT > 0.35f && (WarnT -= DeltaSeconds) <= 0.f) { WarnT = 0.25f; WarnNearby(); }

			// Draw warning danger circle on the ground around the live grenade
			if (UWorld* W = GetWorld())
			{
				const FVector GrenadeLoc = GetActorLocation();
				FHitResult GroundHit;
				FCollisionQueryParams GParams(SCENE_QUERY_STAT(GrenadeGround), false, this);
				FVector GroundPt = GrenadeLoc - FVector(0.0, 0.0, 7.0);
				if (W->LineTraceSingleByChannel(GroundHit, GrenadeLoc + FVector(0.0, 0.0, 25.0), GrenadeLoc - FVector(0.0, 0.0, 600.0), ECC_WorldStatic, GParams))
				{
					GroundPt = GroundHit.ImpactPoint + FVector(0.0, 0.0, 3.0);
				}

				const float NowT = (float)W->GetTimeSeconds();
				const float Pulse = 0.5f + 0.5f * FMath::Sin(NowT * 12.f);
				const FColor OuterColor = FColor(255, 50, 30, (uint8)(140 + Pulse * 115));
				const FColor InnerColor = FColor(255, 15, 10, (uint8)(200 + Pulse * 55));

				// 1. Outer blast hazard radius circle (550 cm)
				DrawDebugCircle(W, GroundPt, BlastRadius * 0.85f, 36, OuterColor, false, -1.f, 0, 3.5f, FVector(0, 1, 0), FVector(1, 0, 0), false);
				// 2. Inner lethal kill-zone circle (220 cm)
				DrawDebugCircle(W, GroundPt, 220.f, 24, InnerColor, false, -1.f, 0, 4.5f, FVector(0, 1, 0), FVector(1, 0, 0), false);

				// 3. Dynamic expanding sonar wave
				const float PulseR = FMath::Fmod(NowT * 350.f, BlastRadius * 0.85f);
				const float PulseAlpha = 1.f - (PulseR / (BlastRadius * 0.85f));
				const FColor WaveColor = FColor(255, 130, 20, (uint8)(PulseAlpha * 220));
				DrawDebugCircle(W, GroundPt, PulseR, 28, WaveColor, false, -1.f, 0, 2.0f, FVector(0, 1, 0), FVector(1, 0, 0), false);

				// 4. Center crosshairs
				DrawDebugLine(W, GroundPt - FVector(35, 0, 0), GroundPt + FVector(35, 0, 0), InnerColor, false, -1.f, 0, 3.f);
				DrawDebugLine(W, GroundPt - FVector(0, 35, 0), GroundPt + FVector(0, 35, 0), InnerColor, false, -1.f, 0, 3.f);
			}

			if (FuseTimer <= 0.f)
			{
				Explode();
			}
		}
		return;
	}

	// Animate explosion visual elements and then destroy
	ExplodeAnimT += DeltaSeconds;
	TickSpinAndSpoon(DeltaSeconds);
	for (int32 i = 0; i < Sparks.Num(); ++i)
	{
		if (!Sparks[i] || !Sparks[i]->IsVisible() || !SparkVel.IsValidIndex(i)) continue;
		SparkVel[i].Z -= 980.0 * DeltaSeconds;
		Sparks[i]->AddWorldOffset(SparkVel[i] * DeltaSeconds);
		if (ExplodeAnimT > 0.09f + 0.012f * (i % 12)) Sparks[i]->SetVisibility(false);
	}
	for (UStaticMeshComponent* Dust : FragDust)
	{
		if (!Dust) continue;
		const float K = FMath::Clamp(ExplodeAnimT / 1.2f, 0.f, 1.f);
		Dust->SetWorldScale3D(FVector(0.25 + 0.55 * K) * (1.0 - K));
		Dust->AddWorldOffset(FVector(0.0, 0.0, 30.0 * DeltaSeconds));
	}

	if (BlastLight)
	{
		const float LightAlpha = FMath::Clamp(1.f - ExplodeAnimT / 0.35f, 0.f, 1.f);
		BlastLight->SetIntensity(55000.f * LightAlpha);
	}

	if (BlastSphere)
	{
		if (ExplodeAnimT < 0.16f)
		{
			const float GrowK = ExplodeAnimT / 0.16f;
			const float Radius = FMath::Lerp(15.f, BlastRadius * 0.65f, GrowK);
			BlastSphere->SetRelativeScale3D(FVector(Radius / 50.0));
		}
		else
		{
			const float FadeK = FMath::Clamp((ExplodeAnimT - 0.16f) / 0.4f, 0.f, 1.f);
			const float Radius = FMath::Lerp(BlastRadius * 0.65f, BlastRadius * 0.8f, FadeK);
			BlastSphere->SetRelativeScale3D(FVector(Radius / 50.0 * (1.f - FadeK)));
			if (FadeK >= 1.f) BlastSphere->SetVisibility(false);
		}
	}

	for (int32 i = 0; i < SmokePuffs.Num(); ++i)
	{
		if (SmokePuffs[i] && PuffVelocities.IsValidIndex(i))
		{
			SmokePuffs[i]->AddWorldOffset(PuffVelocities[i] * DeltaSeconds);
			const float PuffFade = FMath::Clamp(1.f - ExplodeAnimT / 1.3f, 0.f, 1.f);
			SmokePuffs[i]->SetWorldScale3D(SmokePuffs[i]->GetComponentScale() * (1.f + DeltaSeconds * 0.6f) * PuffFade);
		}
	}

	if (ExplodeAnimT > 1.4f)
	{
		Destroy();
	}
}

void AAAAGrenade::Explode()
{
	if (bExploded) return;
	bExploded = true;

	const FVector BlastLoc = GetActorLocation();

	// Hide physical grenade model
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M) M->SetVisibility(false);
	}
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Movement) Movement->StopMovementImmediately();

	// 1. 3D Spatial Audio Boom: multi-layered deep explosion blast + 3D urban wall shockwave reflections
	if (ExplosionSound)
	{
		ExplosionSound->AttenuationSettings = GetGrenade3DBlastAttenuation();
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, BlastLoc, FRotator::ZeroRotator, 2.0f, 0.35f, 0.f, GetGrenade3DBlastAttenuation());
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, BlastLoc + FVector(380.0, -320.0, 120.0), FRotator::ZeroRotator, 0.85f, 0.26f, 0.f, GetGrenade3DBlastAttenuation());
	}
	if (BounceSound)
	{
		BounceSound->AttenuationSettings = GetGrenade3DBlastAttenuation();
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, BlastLoc, FRotator::ZeroRotator, 1.4f, 0.78f, 0.f, GetGrenade3DBlastAttenuation());
	}

	// 2. High-intensity Exposure Flash Light
	BlastLight = NewObject<UPointLightComponent>(this);
	BlastLight->SetMobility(EComponentMobility::Movable);
	BlastLight->SetupAttachment(Collision);
	BlastLight->SetIntensity(55000.f);
	BlastLight->SetAttenuationRadius(BlastRadius * 2.6f);
	BlastLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.35f));
	BlastLight->SetCastShadows(false);
	BlastLight->RegisterComponent();

	// 3. Fireball mesh (white-hot core expanding)
	UMaterialInstanceDynamic* FireMat = AAAShapes::Mat(this, FLinearColor(1.5f, 0.8f, 0.25f) * 16.f, true);
	BlastSphere = AAAShapes::Make(this, false, FVector(12.0), FireMat, false, false);
	BlastSphere->SetupAttachment(Collision);
	BlastSphere->RegisterComponent();

	// Ground blast scorch disc
	FHitResult ScorchHit;
	FCollisionQueryParams SParams(SCENE_QUERY_STAT(GrenadeScorch), false, this);
	if (GetWorld()->LineTraceSingleByChannel(ScorchHit, BlastLoc + FVector(0.0, 0.0, 30.0), BlastLoc - FVector(0.0, 0.0, 400.0), ECC_WorldStatic, SParams))
	{
		UMaterialInstanceDynamic* ScorchMat = AAAShapes::Mat(this, FLinearColor(0.012f, 0.012f, 0.01f));
		UStaticMeshComponent* Scorch = AAAShapes::Make(this, true, FVector(190.0, 190.0, 0.8), ScorchMat, false, false);
		Scorch->SetMobility(EComponentMobility::Movable);
		Scorch->SetWorldLocationAndRotation(ScorchHit.ImpactPoint + ScorchHit.ImpactNormal * 1.5f, ScorchHit.ImpactNormal.Rotation() + FRotator(90.0, 0.0, 0.0));
		Scorch->RegisterComponent();
	}

	// 4. Smoke / debris puffs
	UMaterialInstanceDynamic* SmokeMat = AAAShapes::Mat(this, FLinearColor(0.22f, 0.2f, 0.18f));
	for (int32 i = 0; i < 7; ++i)
	{
		UStaticMeshComponent* Puff = AAAShapes::Make(this, false, FVector(FMath::FRandRange(40.f, 75.f)), SmokeMat, false, true);
		Puff->SetupAttachment(Collision);
		Puff->SetRelativeLocation(FVector(FMath::FRandRange(-35.f, 35.f), FMath::FRandRange(-35.f, 35.f), FMath::FRandRange(10.f, 60.f)));
		Puff->RegisterComponent();
		SmokePuffs.Add(Puff);

		FVector Dir = (Puff->GetRelativeLocation() + FVector(0.0, 0.0, 75.0)).GetSafeNormal();
		PuffVelocities.Add(Dir * FMath::FRandRange(220.f, 440.f));
	}

	// 5. Ragdoll & Damage for AI Police Officers caught in the blast
	for (TActorIterator<AAAAPoliceOfficer> It(GetWorld()); It; ++It)
	{
		AAAAPoliceOfficer* Officer = *It;
		if (Officer)
		{
			const float Dist = (float)FVector::Dist(BlastLoc, Officer->GetActorLocation());
			if (Dist <= BlastRadius)
			{
				const float Prox = 1.f - (Dist / BlastRadius);
				// the soldier decides what the blast does to him (killed close in, wounded further out, thrown
				// by a believable push of the air - a few metres per second - not launched like a rocket)
				Officer->OnBlast(Prox, BlastLoc, BlastRadius);
			}
		}
	}

	// 6. Radial damage with falloff for other actors (player, destructibles)
	TArray<AActor*> Ignored;
	Ignored.Add(this);
	AController* InstigatorCtrl = Thrower.IsValid() ? Thrower->GetController() : nullptr;
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		BlastDamage,
		35.f,
		BlastLoc,
		140.f,
		BlastRadius,
		1.f,
		UDamageType::StaticClass(),
		Ignored,
		this,
		InstigatorCtrl
	);

	// 6b. Fragmentation: the steel body breaks into hundreds of splinters. Each ray is a sheaf of them; behind
	//     cover you are safe, in the open they still wound well past the blast itself.
	Fragment(BlastLoc, InstigatorCtrl);

	// 7. Physics blast impulse on surrounding rigid bodies (cars, debris)
	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(BlastRadius);
	FCollisionQueryParams QParams(SCENE_QUERY_STAT(GrenadeBlast), false, this);
	if (GetWorld()->OverlapMultiByChannel(Overlaps, BlastLoc, FQuat::Identity, ECC_WorldDynamic, SphereShape, QParams))
	{
		for (const FOverlapResult& Ov : Overlaps)
		{
			if (UPrimitiveComponent* Prim = Ov.GetComponent())
			{
				if (Prim->IsSimulatingPhysics())
				{
					// bodies (the player's ragdoll, the dead): a human-sized push; soldiers handle their own in OnBlast
					if (USkeletalMeshComponent* Sk = Cast<USkeletalMeshComponent>(Prim))
					{
						if (!Cast<AAAAPoliceOfficer>(Sk->GetOwner())) Sk->AddRadialImpulse(BlastLoc - FVector(0.0, 0.0, 80.0), BlastRadius, 900.f, ERadialImpulseFalloff::RIF_Linear, true);
					}
					else Prim->AddRadialImpulse(BlastLoc, BlastRadius, BlastImpulse * 1.5f, ERadialImpulseFalloff::RIF_Linear, true);
				}
			}
		}
	}

	// 8. Alert traffic / police subsystem of the blast
	for (TActorIterator<ATrafficAIManager> It(GetWorld()); It; ++It)
	{
		It->ReportGunfire(BlastLoc);
		break;
	}

	// 9. Camera Exposure Flash & Impact on local player
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			const float Dist = FVector::Dist(PlayerPawn->GetActorLocation(), BlastLoc);
			if (Dist < 2400.f)
			{
				const float Strength = 1.f - (Dist / 2400.f);
				if (UAAAWeaponComponent* WC = PlayerPawn->FindComponentByClass<UAAAWeaponComponent>())
				{
					WC->TriggerExplosionExposure(Strength);
				}
				const float KickPitch = FMath::FRandRange(-5.5f, -2.0f) * Strength;
				const float KickYaw = FMath::FRandRange(-3.5f, 3.5f) * Strength;
				PC->AddPitchInput(KickPitch);
				PC->AddYawInput(KickYaw);
			}
		}
	}
}

void AAAAGrenade::Fragment(const FVector& BlastLoc, AController* InstigatorCtrl)
{
	UWorld* W = GetWorld();
	if (!W) return;
	const float FragRange = 2200.f;
	const FVector From = BlastLoc + FVector(0.0, 0.0, 12.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(GrenadeFrag), false, this);
	TMap<AActor*, int32> HitsOn;
	UMaterialInstanceDynamic* HotMat = AAAShapes::Mat(this, FLinearColor(1.6f, 0.75f, 0.2f) * 22.f, true);
	UMaterialInstanceDynamic* DustMat = AAAShapes::Mat(this, FLinearColor(0.3f, 0.28f, 0.25f));
	const int32 Rays = 44;
	for (int32 i = 0; i < Rays; ++i)
	{
		// mostly out sideways and up (the ground eats the downward half)
		const float Yaw = (360.f / Rays) * i + FMath::FRandRange(-4.f, 4.f);
		const float Elev = FMath::FRandRange(-8.f, 32.f);
		const FVector Dir = FRotator(Elev, Yaw, 0.f).Vector();
		FHitResult H;
		const bool bHit = W->LineTraceSingleByChannel(H, From, From + Dir * FragRange, ECC_Visibility, Q);
		if (bHit)
		{
			AActor* A = H.GetActor();
			const float D = H.Distance;
			// close in, the blast itself already did its work (OnBlast / radial damage)
			if (A && Cast<APawn>(A) && D > BlastRadius * 0.75f)
			{
				int32& N = HitsOn.FindOrAdd(A);
				if (N < 2)
				{
					++N;
					const float K = FMath::Clamp((D - BlastRadius * 0.75f) / (FragRange - BlastRadius * 0.75f), 0.f, 1.f);
					UGameplayStatics::ApplyPointDamage(A, FMath::Lerp(38.f, 9.f, K), Dir, H, InstigatorCtrl, this, UDamageType::StaticClass());
				}
			}
			else if (!Cast<APawn>(A) && i % 3 == 0 && FragDust.Num() < 14)
			{
				// a puff of dust and chips where the splinters strike
				UStaticMeshComponent* Dust = AAAShapes::Make(this, false, FVector(10.0), DustMat, false, false);
				Dust->SetMobility(EComponentMobility::Movable);
				Dust->SetWorldLocation(H.ImpactPoint + H.ImpactNormal * 6.0);
				Dust->RegisterComponent();
				FragDust.Add(Dust);
			}
		}
		// visible sparks: a few hot splinters streaking out
		if (i % 2 == 0)
		{
			UStaticMeshComponent* Sp = AAAShapes::Make(this, false, FVector(9.0, 0.9, 0.9), HotMat, false, false);
			Sp->SetMobility(EComponentMobility::Movable);
			Sp->SetWorldLocationAndRotation(From + Dir * 20.0, Dir.Rotation());
			Sp->RegisterComponent();
			Sparks.Add(Sp);
			SparkVel.Add(Dir * FMath::FRandRange(2600.f, 4200.f));
		}
	}
}
