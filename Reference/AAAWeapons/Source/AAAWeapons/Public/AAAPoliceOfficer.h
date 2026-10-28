// AAA City Weapons - a soldier on foot. The same body / rifle / brain plays three sides (Team):
//  * Police (AAATeam::Police): gets out of a police car that stopped near the WANTED player
//  * Enemy (AAATeam::Enemy): army patrols that walk their beat and fight the player's team on sight
//  * Player (AAATeam::Player): one of the leader's three men, each with his own trade and temper
//
// The player's men and the army think for themselves ("Squad"-style), every 0.3 s:
//  * a memory of contacts: what I saw, heard, or was told on the radio (with the radio's delay and error),
//    how sure I am, how old it is, who is shooting at whom
//  * a threat score per contact (close, visible, shooting at me / the leader / a mate, the leader's marked target,
//    already taken by another rifle -> spread the fire)
//  * a utility choice of task: fight from cover, reposition (cover seen / out of my range band), bound forward
//    (only while a mate covers: overwatch), flank a pinned enemy along a hidden route, suppress for a mate who
//    moves, frag an enemy who hides, pull back when hurt, reload in cover, run to revive a downed mate
//  * cover is scored against every known threat (protection, a peek spot each side, my trade's range band,
//    spacing from the mates, how exposed the run there is, the leash of the leader's order)
//  * trades: rifleman (pushes, flanks), marksman (long range, single aimed shots, overwatch),
//    support (long bursts, suppression, the team's medic)
//  * the leader's men go down instead of dying at once: they bleed out unless a mate (or the leader: hold F)
//    patches them up, then they get up and fight on
//  * they talk: contact with direction and range, moving / covering, reloading, man down, grenade (HUD callouts)
// The police keep their own simpler brain (arrest, chase, cover, flank, backup).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AAAFireTeamTypes.h"
#include "AAAPoliceOfficer.generated.h"

class UAAARifleAnimInstance;
class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;
class UAAAWeaponComponent;
class UPhysicalAnimationComponent;

UCLASS()
class AAAWEAPONS_API AAAAPoliceOfficer : public ACharacter
{
	GENERATED_BODY()
public:
	AAAAPoliceOfficer();

	UPROPERTY(EditAnywhere, Category = "Officer") float MaxHealth = 100.f;
	/** skill: multiplies the hit chance */
	UPROPERTY(EditAnywhere, Category = "Officer") float Skill = 1.f;
	/** damage of one bullet on the player */
	UPROPERTY(EditAnywhere, Category = "Officer") float BulletDamage = 7.f;
	/** damage of one bullet on another soldier (before the headshot multiplier) */
	UPROPERTY(EditAnywhere, Category = "Officer") float SoldierDamage = 19.f;
	/** cm */
	UPROPERTY(EditAnywhere, Category = "Officer") float SightRange = 7000.f;
	UPROPERTY(EditAnywhere, Category = "Officer") float RunSpeed = 470.f;
	UPROPERTY(EditAnywhere, Category = "Officer") float WalkSpeed = 190.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ---- side (set before BeginPlay: SpawnActorDeferred)
	uint8 Team = AAATeam::Police;
	/** 0..2 in his group: the player's men (name, colour, formation slot, trade); the army (trade) */
	int32 SquadIndex = -1;
	static bool Hostile(uint8 A, uint8 B, int32 WantedStars);

	/** what a man does best */
	enum class ETrade : uint8 { Rifleman, Marksman, Support };
	/** what he is doing now (HUD) */
	enum class ETask : uint8 { None, Order, Fight, Reposition, Bound, Flank, CoverFire, Grenade, Retreat, Reload, Revive, Search, Patrol };

	// ---- squad / senses (called by UAAAWeaponsSubsystem)
	enum ERole : uint8 { RoleSuppress = 0, RoleFlank = 1 };
	void SetRole(uint8 NewRole) { Role = NewRole; }
	uint8 GetRole() const { return Role; }
	bool IsDead() const { return bDead; }
	/** dead, or down and bleeding (not a target, not a fighter) */
	bool IsOut() const { return bDead || bDowned; }
	bool IsDowned() const { return bDowned && !bDead; }
	float GetBleedT() const { return BleedT; }
	float GetReviveProgress() const { return ReviveProg; }
	bool IsMovingToCover() const { return bMoving && bRunning; }
	bool SeesPlayer() const { return bSees; }
	bool SeesFoe() const { return bSees; }
	bool IsEngaged() const { return bHasIntel && !IsOut(); }
	bool InCombat() const { return !IsOut() && (bSees || Now - LastSeen < 4.f || Now - LastShotT < 2.f); }
	bool IsReloading() const { return ReloadT >= 0.f; }
	bool IsInCover() const { return bHasCover && !bMoving && FVector::Dist2D(GetActorLocation(), Cover) < 90.f; }
	/** holding still with a target in sight: can cover a mate who moves */
	bool IsCovering() const { return !IsOut() && !bMoving && ReloadT < 0.f && (bSees || CoverFireT > 0.f); }
	float GetSuppression() const { return Suppression; }
	float GetSpawnTime() const { return BornT; }
	float GetHealth01() const { return FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f); }
	float GetAmmo01() const { return FMath::Clamp((float)Ammo / 30.f, 0.f, 1.f); }
	int32 GetMags() const { return Mags; }
	ETask GetTask() const { return Task; }
	ETrade GetTrade() const { return Trade; }
	FVector GetCoverPoint() const { return bHasCover ? Cover : FVector(1e9); }
	ACharacter* Foe() const { return FoeActor.Get(); }
	void HearShot(const FVector& Where, float Loudness);
	void Suppress(float Amount);
	void RadioIntel(const FVector& Where);
	/** the radio: a mate reports a contact (Who may be null: "they are over there") */
	void RadioContact(ACharacter* Who, const FVector& Where);
	/** a mate is moving under fire: shoot at Where for a while, seen or not */
	void RequestCoverFire(const FVector& Where, float Seconds);
	void StandDown() { bStandDown = true; }
	bool CanDespawn() const;
	void Die(const FVector& Dir);
	void StartRagdoll(const FVector& Dir);
	/** caught by a grenade (Prox 0..1 from the edge to the centre) */
	void OnBlast(float Prox, const FVector& Origin, float Radius);
	/** a live grenade nearby that he noticed: run clear of it (or brace if there's no time) */
	void EvadeGrenade(const FVector& Where, float TimeLeft, float Radius);
	/** first aid (seconds of work, from a mate or the leader): back on his feet when it reaches 1 */
	void AddRevive(float Amount);
	/** just got out of a police car: step away from the door (Out) drawing the rifle, then fight */
	void BeginExit(const FVector& Out);
	FVector ExitDir = FVector::ZeroVector;

	// ---- the player's men
	void SetOrder(EAAASquadOrder NewOrder, const FVector& Where, ACharacter* Target);
	EAAASquadOrder GetOrder() const { return Order; }
	void SetHoldFire(bool bHold) { bHoldFire = bHold; }
	FVector GetOrderPos() const { return OrderPos; }
	/** jump to a spot near the leader (left behind: the leader drove off, climbed a wall ...) */
	void CatchUp(const FVector& Where);
	/** ammo from the leader's bag */
	void Resupply() { Mags = 6; if (ReloadT < 0.f) Ammo = 30; bToldLowAmmo = false; }
	/** what I know of the enemy (HUD markers): position, how fresh (0..1), seen with my own eyes */
	void GetContacts(TArray<FVector>& OutPos, TArray<float>& OutFresh) const;

	// ---- army patrols
	void SetPatrol(const TArray<FVector>& Points) { Patrol = Points; PatrolIdx = 0; }

private:
	enum class EState : uint8 { Exit, Engage, Search, Retreat, Arrest, Idle, Patrol };

	struct FContact
	{
		TWeakObjectPtr<ACharacter> Who;
		FVector Pos = FVector::ZeroVector;   // last known position
		float SeenT = -99.f;                  // last time I saw him
		float InfoT = -99.f;                  // last time I learned anything about him
		bool bVisible = false;
	};

	ACharacter* Player() const;
	UAAAWeaponComponent* PlayerWeapon() const;
	bool FoeIsPlayer() const { return FoeActor.IsValid() && FoeActor.Get() == Player(); }
	bool FoeDead() const;
	static bool IsOutChar(const ACharacter* C);
	void BuildGear();
	void ScanFoes();
	void Perceive(float Dt);
	void Think();
	void ThinkPolice();
	void ThinkTactical();
	void ThinkPatrol();
	/** the police cover / peek / sidestep fight (true when it took over this think) */
	bool ThinkEngage();
	void Act(float Dt);
	void Shoot(float Dt);
	void FireAt(const FVector& AimPoint, float HitChance, ACharacter* Victim);
	bool FindCover(bool bFlank, bool bAway);
	/** the tactical cover search: Mode 0 fight here, 1 bound forward, 2 flank, 3 fall back */
	bool FindTacticalCover(int32 Mode);
	bool LineClear(const FVector& A, const FVector& B) const;
	bool PathClear(const FVector& From, const FVector& To) const;
	bool GroundAt(const FVector& P, FVector& Out) const;
	void MoveTo(const FVector& P, bool bRun);
	FVector Muzzle() const;
	FVector ThreatPos() const;
	class UAAAWeaponsSubsystem* Squad() const;

	// tactics
	FContact* FindContact(const ACharacter* Who);
	void Learn(ACharacter* Who, const FVector& Where, bool bSeen, bool bFromRadio);
	void UpdateContacts(float Dt);
	float ThreatOf(const FContact& K) const;
	void PickTarget();
	int32 FreshThreats(TArray<FVector>& OutPos, float MaxAge) const;
	bool MayMove() const;
	void Say(const FString& Text, float MinGap = 1.5f);
	FString Bearing(const FVector& Where) const;
	void GoDown(const FVector& Dir);
	void SetTask(ETask NewTask) { if (Task != NewTask) { Task = NewTask; TaskT = 0.f; } }
	void ApplyTrade();

	UPROPERTY(Transient) TObjectPtr<UAAARifleAnimInstance> Anim;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Rifle;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flash;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cap;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Brim;
	/** the "active ragdoll": the bones simulate, pulled towards the animation (death fall, lying wounded) by springs
	 *  whose strength fades - a body that falls like a person, reacts to every hit and settles, never flies or freezes */
	UPROPERTY(Transient) TObjectPtr<UPhysicalAnimationComponent> PhysAnim;
	bool bPhysBody = false;
	float PhysK = 0.f, SnapT = -1.f, WritheT = 0.f, WritheDir = 1.f;
	FName LastHitBone = NAME_None;
	FVector LastHitPoint = FVector::ZeroVector, BlastOrigin = FVector::ZeroVector;
	float BlastR = 650.f, BlastProx = 0.f;
	void BeginActiveRagdoll(float Strength);
	void SetRagdollStrength(float K);
	void EndActiveRagdoll();
	void HitImpulse(const FVector& Dir, float Amount);
	FVector MuzzleLocal = FVector(60.0, 0.0, 6.6);
	bool bGear = false, bCapPlaced = false;

	EState State = EState::Exit;
	uint8 Role = RoleSuppress;
	float Health = 100.f, BornT = 0.f, Now = 0.f, ThinkT = 0.f, StateT = 0.f;
	// senses
	TWeakObjectPtr<ACharacter> FoeActor;
	bool bSees = false, bHasIntel = false, bHeadOnly = false;
	float Awareness = 0.f, TrackT = 0.f, LastSeen = -99.f, LastHeard = -99.f, Suppression = 0.f, PlayerArmedT = 0.f, LastShotT = -99.f, LastHurtT = -99.f;
	FVector LastKnown = FVector::ZeroVector;
	TArray<FContact> Known;
	int32 KnownScan = 0;
	// cover / movement
	bool bHasCover = false, bMoving = false, bRunning = false, bPeeking = false, bRetreated = false, bStandDown = false;
	FVector Cover = FVector::ZeroVector, Peek = FVector::ZeroVector, PeekB = FVector::ZeroVector, MoveTarget = FVector::ZeroVector;
	bool bPeekSideB = false, bHurtWhilePeeking = false;
	float PeekT = 0.f, CoverCheckT = 0.f, StuckT = 0.f, SearchT = 0.f, ArrestT = 0.f, FacingYaw = 0.f;
	/** cover / movement must stay within LeashR of LeashC (0 = no leash) */
	FVector LeashC = FVector::ZeroVector;
	float LeashR = 0.f;
	// trade / temper
	ETrade Trade = ETrade::Rifleman;
	float PrefMin = 1000.f, PrefMax = 3000.f, Aggression = 0.5f, Courage = 0.6f;
	// tactics
	ETask Task = ETask::None;
	float TaskT = 0.f, LastFlankT = -99.f, LastSayT = -99.f, LastMoveCallT = -99.f, CoverFireT = 0.f, EngageT = 0.f;
	FVector CoverFirePos = FVector::ZeroVector;
	FString LastSaid;
	TWeakObjectPtr<AAAAPoliceOfficer> ReviveWho;
	int32 Mags = 6;
	bool bToldLowAmmo = false, bToldContact = false;
	// downed / first aid
	bool bDowned = false;
	float BleedT = 0.f, ReviveProg = 0.f, GetUpT = -1.f;
	// orders (the player's men)
	EAAASquadOrder Order = EAAASquadOrder::Follow;
	FVector OrderPos = FVector::ZeroVector;
	TWeakObjectPtr<ACharacter> OrderTarget;
	bool bHoldFire = false, bOrderReached = false;
	float OrderT = 0.f;
	// patrol (army)
	TArray<FVector> Patrol;
	int32 PatrolIdx = 0;
	float PatrolWaitT = 0.f;
	// weapon
	int32 Ammo = 30, BurstLeft = 0;
	float ShotCD = 0.f, BurstPause = 0.5f, ReloadT = -1.f, Kick = 0.f, KickV = 0.f, FlashT = 0.f, HitT = -1.f;
	float ShotSide = 0.f;   // sideways kick of the last shot (picked once per shot: a new random value every frame made the weapon jitter)
	int32 HitIndex = 0;
	float HitDirDeg = 0.f, HitStr = 0.4f;   // the last hit: from where (degrees from facing) and how hard
	bool bBlastHit = false, bRagdolled = false;
	int32 DeathIdx = INDEX_NONE;
	float DeathLen = 0.f;
	FVector DeathImpulse = FVector::ZeroVector;
	bool bDead = false;
	float DeadT = 0.f;

	// grenade
	int32 Grenades = 2;
	float GrenadeCD = 8.f;
	float EvadeT = -1.f;
	// stance: crouch behind low cover (rise to fire over it), lean out of side peeks (sometimes from a crouch),
	// kneel to treat a mate, take a knee when halted in the open; the capsule follows (eyes / hits really drop)
	void UpdateStance(float Dt);
	float CrouchTgt = 0.f, LeanTgt = 0.f, StillT = 0.f;
	bool bCoverLow = false, bPeekOver = false, bLowPeek = false, bKneelOK = true;
	FVector EvadeFrom = FVector::ZeroVector;
	float ThrowT = -1.f;
	// marksman's Kar98k: fire + bolt-cycle clip timer (2.00 s, < 0 = idle) and the bolt that moves with it
	float SniperT = -1.f;
	bool bKarSoundDone = false, bKarCasingDone = false;
	TWeakObjectPtr<USceneComponent> KarBolt;
	bool UsesKar() const { return Team != AAATeam::Police && Trade == ETrade::Marksman; }
	bool bGrenadeSpawned = false;
	FVector TargetThrowLoc = FVector::ZeroVector;
	FVector SmoothDir = FVector::ZeroVector;   // smoothed steering direction
};
