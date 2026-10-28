// AAA City Weapons - Squad Types & Tactical Data Definitions
#pragma once

#include "CoreMinimal.h"
#include "AAASquadTypes.generated.h"

UENUM(BlueprintType)
enum class EAAASquadRole : uint8
{
	Leader    UMETA(DisplayName = "Squad Leader (Player)"),
	Assault   UMETA(DisplayName = "Bravo-1: Assault / Pointman"),
	Support   UMETA(DisplayName = "Bravo-2: Support / Heavy Gunner"),
	Marksman  UMETA(DisplayName = "Bravo-3: Marksman / Medic")
};

UENUM(BlueprintType)
enum class EAAASquadFormation : uint8
{
	Wedge     UMETA(DisplayName = "Tactical Wedge (V-Formation)"),
	Column    UMETA(DisplayName = "Column / Urban File"),
	Line      UMETA(DisplayName = "Line / Frontal Assault"),
	Dispersed UMETA(DisplayName = "Dispersed Cover")
};

UENUM(BlueprintType)
enum class EAAASquadCommand : uint8
{
	FollowLeader   UMETA(DisplayName = "Follow Formation"),
	MoveToLocation UMETA(DisplayName = "Move & Take Cover"),
	AttackTarget   UMETA(DisplayName = "Focus Fire & Attack"),
	SuppressArea   UMETA(DisplayName = "Suppressive Fire"),
	HoldPosition   UMETA(DisplayName = "Hold Position"),
	Regroup        UMETA(DisplayName = "Regroup on Leader"),
	ReviveTeammate UMETA(DisplayName = "Revive Downed Ally")
};

UENUM(BlueprintType)
enum class EAAASquadState : uint8
{
	Idle          UMETA(DisplayName = "Idle"),
	Following     UMETA(DisplayName = "Following Formation"),
	MovingToPoint UMETA(DisplayName = "Moving to Point"),
	InCover       UMETA(DisplayName = "Taking Cover"),
	Engaging      UMETA(DisplayName = "Engaging Enemy"),
	Suppressing   UMETA(DisplayName = "Laying Suppression"),
	Holding       UMETA(DisplayName = "Holding Ground"),
	Downed        UMETA(DisplayName = "Downed / Wounded"),
	Reviving      UMETA(DisplayName = "Reviving Ally")
};

UENUM(BlueprintType)
enum class EAAASquadRoE : uint8
{
	FireAtWill     UMETA(DisplayName = "Fire at Will (Aggressive)"),
	ReturnFireOnly UMETA(DisplayName = "Return Fire Only (Defensive)"),
	HoldFire       UMETA(DisplayName = "Hold Fire (Stealth)")
};

USTRUCT(BlueprintType)
struct FAAASquadMemberData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	TWeakObjectPtr<class AAAASquadMemberCharacter> Character = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	EAAASquadRole Role = EAAASquadRole::Assault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FString Callsign = TEXT("Bravo-1");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	EAAASquadState State = EAAASquadState::Following;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	int32 Ammo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FVector FormationOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	FVector DesignatedLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad")
	bool bIsDowned = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSquadCommandEventSignature, EAAASquadCommand, Command, FVector, TargetLocation, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSquadMemberStateChangedSignature, EAAASquadRole, Role, EAAASquadState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSquadFormationChangedSignature, EAAASquadFormation, NewFormation);
