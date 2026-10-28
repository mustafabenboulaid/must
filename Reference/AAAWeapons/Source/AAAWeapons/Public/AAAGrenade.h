#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AAAGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;
class USoundBase;

/**
 * Hand grenade projectile thrown by player or AI police.
 * Simulates ballistic arc with realistic bouncing, timed fuse, procedural mesh,
 * radial explosive damage, physics blast impulse, smoke/fireball VFX and camera shake.
 */
UCLASS()
class AAAWEAPONS_API AAAAGrenade : public AActor
{
	GENERATED_BODY()

public:
	AAAAGrenade();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Launch the grenade with given initial velocity and optional fuse override (for cooked grenades) */
	void Launch(const FVector& Velocity, ACharacter* InThrower, float InFuseTime = 3.2f);

	/** Force immediate detonation */
	void Explode();

	bool IsExploded() const { return bExploded; }
	float GetTimeRemaining() const { return FMath::Max(0.f, FuseTimer); }

	UFUNCTION()
	void OnGrenadeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
	float BlastRadius = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
	float BlastDamage = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
	float BlastImpulse = 90000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
	TObjectPtr<USoundBase> BounceSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grenade")
	TObjectPtr<USoundBase> ExplosionSound;

	TWeakObjectPtr<ACharacter> Thrower;

private:
	void BuildProceduralMesh();

	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
	TObjectPtr<UPointLightComponent> BlastLight;
	TObjectPtr<UStaticMeshComponent> BlastSphere;
	TArray<TObjectPtr<UStaticMeshComponent>> SmokePuffs;
	TArray<FVector> PuffVelocities;

	// secondary motion: tumble in flight, roll on the ground, the safety lever (spoon) flying off on release
	void TickSpinAndSpoon(float Dt);
	void Fragment(const FVector& BlastLoc, AController* InstigatorCtrl);
	void WarnNearby();
	TObjectPtr<UStaticMeshComponent> Spoon;
	FVector SpinAxis = FVector(0.0, 1.0, 0.0);
	float SpinRate = 0.f;          // deg/s
	float LastHitT = -9.f;
	FVector SpoonVel = FVector::ZeroVector;
	FVector SpoonSpinAxis = FVector(1.0, 0.0, 0.0);
	float SpoonSpin = 0.f;         // deg/s
	bool bSpoonFree = false, bSpoonRest = false;
	float WarnT = 0.f, FlightT = 0.f;
	// fragments: hot sparks flying out, dust where they strike
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;
	TArray<FVector> SparkVel;
	TArray<TObjectPtr<UStaticMeshComponent>> FragDust;

	float FuseTimer = 3.2f;
	float ExplodeAnimT = 0.f;
	bool bExploded = false;
	bool bLaunched = false;
	float LastBounceSoundT = 0.f;
};
