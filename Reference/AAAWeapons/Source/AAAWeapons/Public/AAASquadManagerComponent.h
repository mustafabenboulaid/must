// AAA City Weapons - Tactical Squad Manager Component (Squad Leader Command Engine)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AAASquadTypes.h"
#include "AAASquadManagerComponent.generated.h"

class AAAASquadMemberCharacter;
class UCanvas;
class APlayerController;

UCLASS(ClassGroup = (AAACity), Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class AAAWEAPONS_API UAAASquadManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAAASquadManagerComponent();

	// ---------------------------------------------------------------- Events
	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FOnSquadCommandEventSignature OnSquadCommandIssued;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FOnSquadMemberStateChangedSignature OnSquadMemberStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FOnSquadFormationChangedSignature OnSquadFormationChanged;

	// ---------------------------------------------------------------- Squad Setup
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation")
	EAAASquadFormation CurrentFormation = EAAASquadFormation::Wedge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|RoE")
	EAAASquadRoE CurrentRoE = EAAASquadRoE::FireAtWill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation", meta = (ClampMin = "100", ClampMax = "1000"))
	float FormationSpacing = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Spawning")
	bool bAutoSpawnSquad = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Members")
	TArray<TObjectPtr<AAAASquadMemberCharacter>> SquadMembers;

	// ---------------------------------------------------------------- Tactical Commands API
	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueContextualCommand();

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueMoveCommand(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueAttackCommand(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueSuppressCommand(const FVector& Area);

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueHoldCommand();

	UFUNCTION(BlueprintCallable, Category = "Squad|Commands")
	void IssueRegroupCommand();

	UFUNCTION(BlueprintCallable, Category = "Squad|Formation")
	void SetSquadFormation(EAAASquadFormation NewFormation);

	UFUNCTION(BlueprintCallable, Category = "Squad|RoE")
	void SetSquadRoE(EAAASquadRoE NewRoE);

	UFUNCTION(BlueprintCallable, Category = "Squad|Spawning")
	void SpawnSquad();

	UFUNCTION(BlueprintPure, Category = "Squad")
	TArray<FAAASquadMemberData> GetSquadStatus() const;

	UFUNCTION(BlueprintPure, Category = "Squad")
	int32 GetAliveMemberCount() const;

	UFUNCTION(BlueprintPure, Category = "Squad")
	FVector GetTacticalWaypoint() const { return CurrentWaypoint; }

	UFUNCTION(BlueprintPure, Category = "Squad")
	bool HasActiveWaypoint() const { return bHasActiveWaypoint; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void DrawSquadHud(UCanvas* Canvas, APlayerController* PC);

protected:
	ACharacter* PlayerCharacter() const;
	APlayerController* PlayerController() const;
	void UpdateFormationPositions(float Dt);
	FVector CalculateFormationOffset(int32 MemberIndex, EAAASquadFormation Formation, float Spacing) const;

	FVector CurrentWaypoint = FVector::ZeroVector;
	bool bHasActiveWaypoint = false;
	float WaypointTimer = 0.f;
	float CommandKeyCooldown = 0.f;
	FDelegateHandle HudHandle;
};
