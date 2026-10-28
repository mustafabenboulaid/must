// AAA City Weapons - world subsystem:
//  * gives the player's character the rifle component (no Blueprint changes needed)
//  * police on foot: when police cars stop next to the WANTED player on foot, their crew gets out (AAAPoliceOfficer)
//  * every side's radio: shared intel, hearing of gunshots / bullets flying by, roles (cover fire / flank),
//    bounding (only part of a side moves at a time), cover spots never shared, backup, stand-down and clean-up
//  * the player's fire team: three men (leader = the player) who take orders (follow / move / hold / assault /
//    take cover / suppress / fall back, hold fire, formation) and are replaced by reinforcements when they fall
//  * army patrols: groups of three that walk a beat somewhere around the player and fight his team on sight
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AAAFireTeamTypes.h"
#include "AAAWeaponsSubsystem.generated.h"

class ACharacter;
class AAAAPoliceOfficer;
class ATrafficAIManager;

UCLASS()
class AAAWEAPONS_API UAAAWeaponsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	friend class UAAAMissionSubsystem;   // chapter 1 spawns its own patrol (SpawnSoldier / FindHiddenSpot)
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAAAWeaponsSubsystem, STATGROUP_Tickables); }

	/** most police officers on foot at once */
	int32 MaxOfficers = 8;
	/** the player's fire team (3 men) */
	bool bSquadEnabled = true;
	/** army patrols around the player */
	bool bEnemyPatrols = true;
	int32 MaxEnemies = 6;

	// ---- events from the rifles
	void OnPlayerGunshot(const FVector& Where);
	void OnBulletPath(const FVector& A, const FVector& B) { OnBulletPathTeam(A, B, AAATeam::Player); }
	void OnBulletPathTeam(const FVector& A, const FVector& B, uint8 ShooterTeam, float Amount = 0.18f);
	void OnGunshot(const FVector& Where, uint8 ShooterTeam);

	// ---- every side's radio
	void Register(AAAAPoliceOfficer* O);
	void ShareIntel(AAAAPoliceOfficer* From, const FVector& Where);
	bool CanMove(const AAAAPoliceOfficer* Me) const;
	bool CoverTaken(const AAAAPoliceOfficer* Me, const FVector& Where) const;
	void CallBackup();
	void OfficerDown(AAAAPoliceOfficer* O);
	ATrafficAIManager* Traffic();
	int32 WantedStars();
	/** everybody Me may shoot at: the player, the soldiers of the hostile sides */
	void GetFoes(const AAAAPoliceOfficer* Me, TArray<ACharacter*>& Out);
	/** the side of any character (the player's own body = Player) */
	uint8 TeamOf(const AActor* A) const;

	// ---- the player's fire team
	ACharacter* Leader() const { return Equipped.Get(); }
	AAAAPoliceOfficer* GetSquadMember(int32 Idx) const { return SquadMen.IsValidIndex(Idx) ? SquadMen[Idx].Get() : nullptr; }
	/** where member Idx stands in the formation now (world, on the ground) */
	FVector FormationSlot(int32 Idx) const;
	/** -1 = the whole team */
	void SquadSelect(int32 Idx) { SquadSel = FMath::Clamp(Idx, -1, 2); }
	int32 GetSquadSelected() const { return SquadSel; }
	/** an order for the selected man (or the whole team) */
	void SquadOrder(EAAASquadOrder O, const FVector& Where, ACharacter* Target);
	void SquadToggleFire();
	bool IsSquadHoldFire() const { return bHoldFire; }
	void SquadCycleFormation();
	EAAAFormation GetFormation() const { return Formation; }
	/** last order (HUD marker) */
	FVector LastOrderPos = FVector::ZeroVector;
	EAAASquadOrder LastOrder = EAAASquadOrder::Follow;
	float LastOrderT = -99.f;
	/** seconds until a fallen man is replaced (HUD) */
	float GetReinforceT(int32 Idx) const { return (Idx >= 0 && Idx < 3) ? ReinforceT[Idx] : 0.f; }
	/** "contact" message for the HUD (serial changes when a new fight starts) */
	int32 ContactSerial = 0;

	// ---- the radio (callouts the HUD shows, contacts the mates learn after a short delay)
	struct FCallout { FString Who; FString Text; FLinearColor Color = FLinearColor::White; float T = 0.f; };
	TArray<FCallout> Callouts;   // newest last
	int32 CalloutSerial = 0;
	void Say(const AAAAPoliceOfficer* From, const FString& Text);
	void SayLeader(const FString& Text);
	/** From reports Who (may be null) at Where: his side hears it after the radio delay */
	void ShareContact(const AAAAPoliceOfficer* From, uint8 Team, ACharacter* Who, const FVector& Where);
	/** Mover asks the mates who cover to fire at Where */
	void RequestCoverFire(const AAAAPoliceOfficer* Mover, const FVector& Where);
	/** how many of Me's side already shoot at Who */
	int32 TargetClaims(const AAAAPoliceOfficer* Me, const ACharacter* Who) const;
	/** the mates of Me (same side, alive, not down) */
	void GetMates(const AAAAPoliceOfficer* Me, TArray<AAAAPoliceOfficer*>& Out) const;
	/** a downed mate of Me's side nobody else is helping (nearest), null if none */
	AAAAPoliceOfficer* NeedsMedic(const AAAAPoliceOfficer* Me) const;
	bool IsReviveTaken(const AAAAPoliceOfficer* Patient, const AAAAPoliceOfficer* Me) const;
	void ClaimRevive(const AAAAPoliceOfficer* Medic, const AAAAPoliceOfficer* Patient);
	/** the leader's ammo bag: refills the men within 10 m */
	int32 ResupplySquad(const FVector& Where);
	/** the downed man of the player's team the leader stands next to (for first aid with F), null if none */
	AAAAPoliceOfficer* DownedNear(const FVector& Where, float Radius) const;

private:
	struct FPendingContact { uint8 Team = 0; TWeakObjectPtr<ACharacter> Who; FVector Where = FVector::ZeroVector; float T = 0.f; TWeakObjectPtr<const AAAAPoliceOfficer> From; };
	TArray<FPendingContact> Pending;
	struct FMedic { TWeakObjectPtr<const AAAAPoliceOfficer> Patient, Medic; float T = 0.f; };
	TArray<FMedic> Medics;
public:

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
	void SpawnOfficers(ACharacter* C);
	void AssignRoles(ACharacter* C);
	void TickSquad(ACharacter* C, float Dt);
	void TickEnemies(ACharacter* C, float Dt);
	AAAAPoliceOfficer* SpawnSoldier(uint8 Team, const FVector& Where, float Yaw, int32 Index);
	bool FindHiddenSpot(const FVector& Around, float MinR, float MaxR, FVector& Out) const;
	TWeakObjectPtr<ACharacter> Equipped;
	TWeakObjectPtr<ATrafficAIManager> TrafficMgr;
	TArray<TWeakObjectPtr<AAAAPoliceOfficer>> Officers;
	TArray<TWeakObjectPtr<AAAAPoliceOfficer>> SquadMen;   // 3 slots (may hold nulls / dead men)
	float DismountT = 0.f, RoleT = 0.f, SearchT = 0.f, BackupT = 0.f, CalmT = 0.f, SquadT = 0.f, EnemyT = 6.f;
	float ReinforceT[3] = { 0.f, 0.f, 0.f };
	int32 ExtraCars = 0, SquadSel = -1;
	bool bHoldFire = false, bSquadSpawned = false, bWasFighting = false;
	EAAAFormation Formation = EAAAFormation::Wedge;
};
