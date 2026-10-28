// AAA City Weapons - Tactical Squad Member Character (Assault, Support, Marksman)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/StrongObjectPtr.h"
#include "AAASquadTypes.h"
#include "AAASquadMemberCharacter.generated.h"

class UAAARifleAnimInstance;
class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class UAAASquadManagerComponent;

struct FSquadBulletTracer
{
	FVector Pos = FVector::ZeroVector;
	FVector EndPos = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Speed = 24000.f;
	float DistLeft = 0.f;
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
};

UCLASS(Blueprintable, BlueprintType)
class AAAWEAPONS_API AAAASquadMemberCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAAASquadMemberCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	EAAASquadRole SquadRole = EAAASquadRole::Assault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	FString Callsign = TEXT("Bravo-1");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|State")
	EAAASquadState CurrentState = EAAASquadState::Following;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	float MaxHealth = 120.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Stats")
	float Health = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	int32 MagazineSize = 30;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Stats")
	int32 CurrentAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	float BulletDamage = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	float FireRate = 10.f; // rounds per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	float MoveRunSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Stats")
	float MoveWalkSpeed = 220.f;

	// ---------------------------------------------------------------- Commands API
	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderFollowFormation(const FVector& Offset);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderMoveTo(const FVector& InTargetLocation, bool bTakeCover = true);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderAttack(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderSuppress(const FVector& TargetArea);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderHoldPosition();

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderRegroup();

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void OrderRevive(AAAASquadMemberCharacter* DownedAlly);

	UFUNCTION(BlueprintCallable, Category = "Squad|Actions")
	void ReviveFromDowned();

	UFUNCTION(BlueprintPure, Category = "Squad")
	bool IsDowned() const { return bIsDowned; }

	UFUNCTION(BlueprintPure, Category = "Squad")
	FVector GetCurrentDestination() const { return TargetLocation; }

	UFUNCTION(BlueprintPure, Category = "Squad")
	FVector GetMuzzleLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Squad")
	void SetSquadManager(UAAASquadManagerComponent* Manager) { SquadManager = Manager; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	void BuildGear();
	void RadioCallout(const FString& Message);

protected:
	UPROPERTY(Transient) TObjectPtr<UAAARifleAnimInstance> Anim;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Rifle;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> RifleParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flash;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TacticalHelmet;

	UPROPERTY(Transient) TWeakObjectPtr<UAAASquadManagerComponent> SquadManager;
	UPROPERTY(Transient) TWeakObjectPtr<AActor> CurrentTargetActor;
	UPROPERTY(Transient) TWeakObjectPtr<AAAASquadMemberCharacter> AllyToRevive;

	TStrongObjectPtr<USoundBase> ShotSound;
	TWeakObjectPtr<AActor> PlayerOrderedTarget;

	TArray<FSquadBulletTracer> ActiveTracers;
	FVector MuzzleLocal = FVector(60.0, 0.0, 6.6);
	FVector TargetLocation = FVector::ZeroVector;
	FVector FormationOffset = FVector::ZeroVector;
	FVector CoverPoint = FVector::ZeroVector;
	FVector PeekPoint = FVector::ZeroVector;
	FVector SmoothMoveDir = FVector::ZeroVector;

	bool bGearBuilt = false;
	bool bHasCover = false;
	bool bPeeking = false;
	bool bHoldOrdered = false;
	bool bMoveOrdered = false;
	bool bIsDowned = false;

	float BornTimer = 0.f;
	float DownedTimer = 60.f;
	float ShotCD = 0.f;
	int32 BurstLeft = 0;
	float BurstPause = 0.f;
	float PeekTimer = 0.f;
	float ReloadTimer = -1.f;
	float FacingYaw = 0.f;
	float ReviveProgress = 0.f;
	float SuppressionBurstLeft = 0.f;
	float FlashTimer = 0.f;
	float Kick = 0.f;
	// Marksman's Kar98k: fire + bolt-cycle clip timer (2.00 s, < 0 = idle) and the bolt part that moves with it
	float SniperT = -1.f;
	TWeakObjectPtr<class USceneComponent> KarBolt;
	float KickV = 0.f;
	float ShotSide = 0.f;
	float HitTimer = -1.f;
	int32 HitIdx = 0;
	float HitDirDeg = 0.f;
	float HitStr = 0.35f;
	float StuckTimer = 0.f;
	float ThinkTimer = 0.f;

	ACharacter* PlayerLeader() const;
	bool IsValidHostileTarget(AActor* Candidate) const;
	FVector GetTargetAimPoint(AActor* Target) const;
	void ThinkAI(float Dt);
	void ActMovement(float Dt);
	void ActCombat(float Dt);
	void StepTracers(float Dt);
	void SpawnTracer(const FVector& Start, const FVector& End);
	void FireBulletAt(const FVector& TargetPos);
	bool SampleCoverAround(const FVector& SearchOrigin, const FVector& ThreatLoc, FVector& OutCover, FVector& OutPeek);
	bool GroundTrace(const FVector& InP, FVector& OutGround) const;
	bool LineOfSight(const FVector& From, const FVector& To, AActor* IgnoreTarget = nullptr) const;
};
