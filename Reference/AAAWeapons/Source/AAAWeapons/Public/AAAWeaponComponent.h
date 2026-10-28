// AAA City Weapons - a rifle for the third-person character (added automatically by UAAAWeaponsSubsystem).
//
// Keys (on foot):  1 = rifle out / away   RMB = aim   LMB = fire (full auto)   R = reload   T = training targets
// * the rifle model is built from engine shapes to fit the hands of the rifle animation (see UAAARifleAnimInstance)
// * bullets are simulated (muzzle velocity, gravity) and swept every frame; every round draws a tracer
// * recoil: camera kick (pitch + random yaw) that recovers when you stop, weapon kick on a spring (IK), spread bloom
// * impacts: bullet holes, dust puffs, physics impulses, damage to other actors, training targets, police cars
// * the police shoot back when you are WANTED and on foot (AAATraffic); health, regeneration, WASTED and respawn
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AAAWeaponDataAsset.h"
#include "AAAWeaponComponent.generated.h"

class ACharacter;
class APlayerController;
class UCanvas;
class USceneComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USpringArmComponent;
class UCameraComponent;
class USoundBase;
class UDamageType;
class UAAARifleAnimInstance;
class ATrafficAIManager;
class UMaterialInstanceDynamic;

/** one entry of a sub-wheel (the "Ghosts of November" HUD: weapons / equipment rings) */
struct FAAARadialSubItem { const TCHAR* Icon; const TCHAR* Label; int32 Action; };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponFiredSignature, UAAAWeaponDataAsset*, WeaponData, FVector, MuzzleLocation, FVector, FireDirection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, ReserveAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReloadStageSignature, int32, Stage, FName, StageDescription);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponSwitchedSignature, UAAAWeaponDataAsset*, NewWeapon);

UENUM(BlueprintType)
enum class EAAAWeaponType : uint8
{
	Rifle  UMETA(DisplayName = "Assault Rifle"),
	LMG    UMETA(DisplayName = "Squad LMG"),
	Pistol UMETA(DisplayName = "Tactical Pistol"),
	Kar98  UMETA(DisplayName = "Kar98k Sniper")
};

UCLASS(ClassGroup = (AAACity), Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class AAAWEAPONS_API UAAAWeaponComponent : public UActorComponent
{
	GENERATED_BODY()
	friend class UAAASmartHudWidget;
	friend class UAAAMissionSubsystem;   // chapter 1: weapon lock, ammo, toasts
public:
	UAAAWeaponComponent();

	// ---------------------------------------------------------------- Data-Driven Blueprint Architecture
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponFiredSignature OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnAmmoChangedSignature OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnReloadStageSignature OnReloadStage;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponSwitchedSignature OnWeaponSwitched;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Data")
	TArray<TObjectPtr<UAAAWeaponDataAsset>> AvailableWeapons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Data")
	TObjectPtr<UAAAWeaponDataAsset> CurrentWeaponData;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Data")
	void EquipWeaponByDataAsset(UAAAWeaponDataAsset* NewWeaponData);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Data")
	void EquipWeaponByIndex(int32 WeaponIndex);

	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	UAAAWeaponDataAsset* GetCurrentWeaponData() const { return CurrentWeaponData; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	const TArray<UAAAWeaponDataAsset*>& GetAvailableWeapons() const { return AvailableWeapons; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Data")
	void AddWeaponDataAsset(UAAAWeaponDataAsset* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void TriggerFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void TriggerReload();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Events", meta = (DisplayName = "On Fire Visuals (BP)"))
	void ReceiveOnFireVisuals(UAAAWeaponDataAsset* Weapon, FVector MuzzleLoc, FVector FireDir);

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Events", meta = (DisplayName = "On Reload Stage (BP)"))
	void ReceiveOnReloadStage(int32 Stage, FName StageName);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle")
	EAAAWeaponType CurrentWeaponType = EAAAWeaponType::Rifle;

	UFUNCTION(BlueprintCallable, Category = "Rifle")
	void SwitchWeapon(EAAAWeaponType NewType);

	// ---------------------------------------------------------------- weapon
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "1", ClampMax = "500")) float Damage = 26.f;
	/** rounds per second (full auto) */
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "1", ClampMax = "30")) float FireRate = 10.f;
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "1", ClampMax = "200")) int32 MagazineSize = 30;
	UPROPERTY(EditAnywhere, Category = "Rifle") int32 ReserveAmmo = 270;
	UPROPERTY(EditAnywhere, Category = "Rifle") bool bInfiniteReserve = true;
	/** metres per second */
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "50", ClampMax = "2000")) float MuzzleVelocity = 720.f;
	UPROPERTY(EditAnywhere, Category = "Rifle", meta = (ClampMin = "0", ClampMax = "3")) float BulletGravity = 1.f;
	/** physics impulse on simulated objects per hit (kg*cm/s) */
	UPROPERTY(EditAnywhere, Category = "Rifle") float ImpactImpulse = 9000.f;

	// ---------------------------------------------------------------- recoil / spread (degrees)
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float RecoilPitch = 0.62f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float RecoilYaw = 0.28f;
	/** how much of the climb pulls back down when you stop shooting (0..1) */
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil", meta = (ClampMin = "0", ClampMax = "1")) float RecoilRecovery = 0.75f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float SpreadHip = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float SpreadAim = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float SpreadPerShot = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Recoil") float SpreadMax = 3.5f;

	// ---------------------------------------------------------------- model fit (applied on top of the calibration)
	UPROPERTY(EditAnywhere, Category = "Rifle|Model") FVector GripOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Rifle|Model") FRotator GripRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, Category = "Rifle|Model") FLinearColor MetalColor = FLinearColor(0.035f, 0.037f, 0.04f);
	UPROPERTY(EditAnywhere, Category = "Rifle|Model") FLinearColor PolymerColor = FLinearColor(0.12f, 0.1f, 0.075f);

	// ---------------------------------------------------------------- camera
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float ArmedArmLength = 300.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") FVector ArmedSocketOffset = FVector(0.0, 50.0, 25.0);
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") FVector ArmedTargetOffset = FVector(0.0, 0.0, 45.0);
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float AimArmLength = 180.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") FVector AimSocketOffset = FVector(0.0, 55.0, 20.0);
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") FVector AimTargetOffset = FVector(0.0, 0.0, 45.0);
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float AimFOV = 65.f;
	/** camera shake on each shot (0 = off, 1 = default, 2 = strong) */
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float CameraShakeScale = 1.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float ArmedWalkSpeed = 390.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Camera") float AimWalkSpeed = 210.f;

	// ---------------------------------------------------------------- grenade
	UPROPERTY(EditAnywhere, Category = "Rifle|Grenade") int32 Grenades = 4;
	UPROPERTY(EditAnywhere, Category = "Rifle|Grenade") int32 MaxGrenades = 4;
	UPROPERTY(EditAnywhere, Category = "Rifle|Grenade") float GrenadeRegenDelay = 25.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Grenade") float GrenadeThrowSpeed = 1600.f;

	// ---------------------------------------------------------------- health / police
	UPROPERTY(EditAnywhere, Category = "Rifle|Health") float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Health") float RegenDelay = 6.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Health") float RegenPerSecond = 9.f;
	UPROPERTY(EditAnywhere, Category = "Rifle|Police") bool bPoliceShoot = true;
	UPROPERTY(EditAnywhere, Category = "Rifle|Police") float PoliceDamage = 6.f;
	/** metres */
	UPROPERTY(EditAnywhere, Category = "Rifle|Police") float PoliceRange = 55.f;
	/** seconds between bursts of one unit */
	UPROPERTY(EditAnywhere, Category = "Rifle|Police") float PoliceBurstInterval = 1.6f;

	// ---------------------------------------------------------------- HUD ("Ghosts of November")
	/** size of the corner panels (dossier, compass, health, weapon); 1 = the HTML design size */
	UPROPERTY(EditAnywhere, Category = "Rifle|HUD", meta = (ClampMin = "0.4", ClampMax = "1.5")) float HudPanelScale = 0.85f;
	/** size of the wheel; 1 = the HTML design size */
	UPROPERTY(EditAnywhere, Category = "Rifle|HUD", meta = (ClampMin = "0.4", ClampMax = "1.5")) float HudWheelScale = 0.72f;

	// ---------------------------------------------------------------- sounds (empty: /Game/AAACity/Sounds/rifle_shot ... if imported)
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> ShotSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> M4ShotSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> PistolShotSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TArray<TObjectPtr<USoundBase>> FootstepSounds;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> DrySound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> ReloadSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> HurtSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> MagOutSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> MagInSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> BoltSound;
	UPROPERTY(EditAnywhere, Category = "Rifle|Sound") TObjectPtr<USoundBase> EquipSound;

	UFUNCTION(BlueprintCallable, Category = "Rifle") void SetArmed(bool bNewArmed);
	UFUNCTION(BlueprintPure, Category = "Rifle") bool IsArmed() const { return bArmed; }
	UFUNCTION(BlueprintPure, Category = "Rifle") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Rifle") bool IsDead() const { return bDead; }
	/** Kind 1 = weapons ring, 2 = equipment ring */
	static const FAAARadialSubItem* GetRadialSubItems(int32 Kind, int32& OutNum);
	bool IsRadialMenuOpen() const { return bRadialMenuOpen; }
	int32 GetRadialSelectedIndex() const { return RadialSelectedIndex; }
	float GetRadialMenuAlpha() const { return RadialMenuAlpha; }
	/** an AI shooter fires at the owner: HitChance (0..1) decides between the body and a near miss */
	void FireIncoming(const FVector& From, float HitChance, float BulletDamage);
	/** a soldier's round (any side) from From towards Aim: flown with the others, hurts whoever it hits */
	void FireAI(const FVector& From, const FVector& Aim, float BulletDmg, AActor* ShotBy, uint8 ShooterTeam, float SuppressAmt = 0.18f);
	/** the leader's aim point (camera centre) and the enemy soldier under it, if any */
	bool AimTrace(FVector& OutAt, ACharacter*& OutEnemy) const;
	/** first aid by the leader (HUD) */
	float GetReviveHoldT() const { return ReviveHoldT; }
	class AAAAPoliceOfficer* GetRevivePatient() const;
	UFUNCTION(BlueprintCallable, Category = "Rifle") void TriggerExplosionExposure(float Strength);
	float GetSinceShot() const { return SinceShot; }
	/** crouch / cover state (HUD): crouched pose, behind low cover, leaning out (-1 left .. +1 right) */
	bool IsCrouchedPose() const { return CrouchOut > 0.5f; }
	bool IsInCover() const { return bInCover; }
	float GetLean() const { return LeanOut; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FBullet { FVector P, V; float Life = 0.f; bool bPolice = false; int32 Tracer = -1; float Dmg = 0.f; bool bAI = false; uint8 Team = 0; float Sup = 0.18f; TWeakObjectPtr<AActor> Shooter; };
	struct FFx { int32 Idx = -1; float T = 0.f, Life = 0.f; FVector V = FVector::ZeroVector; FVector Base = FVector::OneVector; };

	ACharacter* Char() const;
	APlayerController* ControllingPC() const;
	void BuildRifle(float LeftDist);
	void RebuildWeaponModel();
	void AttachRifle();
	void FireShot(APlayerController* PC);
	void FirePolice(const FVector& From, ACharacter* C);
	void StepBullets(float Dt);
	void Impact(const FHitResult& Hit, const FVector& Dir, bool bPolice);
	void UpdateFx(float Dt);
	void UpdateCamera(float Dt, bool bOn);
	void UpdateCameraShake(float Dt);
	void AddCameraShake(float Strength);
	void UpdatePolice(float Dt, ACharacter* C);
	void SpawnTargets(ACharacter* C);
	void Die();
	void StartRagdoll();
	float ClimbCamLift = 0.f;   // camera pivot raised while climbing (cm)
	// crouch & smart cover (C toggles crouch; near low cover the player ducks behind it, rises to aim over it,
	// and leans out when the aim is blocked by a wall corner)
	void UpdateCrouchCover(ACharacter* C, APlayerController* PC, float Dt, bool bInputOk);
	void ScanCover(ACharacter* C, APlayerController* PC);
	bool bCrouchInit = false, bCrouchToggle = false, bInCover = false, bLowCover = false;
	float CrouchOut = 0.f, LeanOut = 0.f, CoverTopH = 0.f, CoverScanT = 0.f, CoverOffT = 0.f, StillT = 0.f;
	float CrouchCamZ = 0.f, CrouchCamY = 0.f, LastCapsuleHalfH = -1.f;
	bool bBodyHiddenForCam = false;
	// hit / death animation state
	FVector LastHitFrom = FVector::ZeroVector;
	float LastHitFromT = 99.f, LastDamage = 0.f;
	float PlayerHitT = -1.f, PlayerHitDir = 0.f, PlayerHitStr = 0.4f;
	int32 PlayerHitIdx = 0;
	int32 DeathIdx = INDEX_NONE;
	float DeathAnimLen = 0.f;
	bool bRagdolled = false;
	void Respawn();
	void StartCookingGrenade();
	void ReleaseGrenade();
	void SpawnThrownGrenade();
	void UpdateTrajectory(ACharacter* C, APlayerController* PC);
	void LoadSounds();
	void PlaySound(USoundBase* S, const FVector& At, float Vol, float Pitch) const;
	ATrafficAIManager* Traffic();
	FTransform MuzzleTransform() const;
	UStaticMeshComponent* Pooled(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32& Next, int32 Max, bool bCylinder, UMaterialInstanceDynamic* M, bool bShadow, int32& OutIdx);
	void DrawHud(UCanvas* Cv, APlayerController* PC);
	void SetupAaaPostProcess();
	void UpdateAaaPostProcess(float Dt);
	void CreateSmartHud();
	UFUNCTION() void OnOwnerDamaged(AActor* DamagedActor, float Amount, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	bool bPostProcessInitialized = false;
	UPROPERTY(Transient) TObjectPtr<class UAAASmartHudWidget> SmartHudWidget;
	UPROPERTY(Transient) TObjectPtr<class APostProcessVolume> GlobalPPV;

	// model
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Rifle;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> RifleParts;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Magazine;
	// Kar98k: the moving bolt (root of the bolt parts), the 2.00 s fire + bolt-cycle clip timer (< 0 = idle) and its one-shot events
	TWeakObjectPtr<USceneComponent> KarBolt;
	float SniperFireT = -1.f;
	// Kar98k reload: rounds to load this time (one by one, AAAKarReload), loaded so far, the round in the right hand
	int32 KarReloadN = 0, KarLoaded = 0;
	bool bKarBoltOpenSnd = false, bKarBoltCloseSnd = false;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> KarRound;
	bool bBoltSoundPlayed = false, bBoltCasingOut = false;
	void SpawnCasing(class ACharacter* C);
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flash;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> FlashLight;
	FVector MuzzleLocal = FVector(70.0, 0.0, 6.0), EjectLocal = FVector(10.0, 2.5, 8.0);
	bool bRifleAttached = false, bBuilt = false;

	// pools
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Tracers;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Holes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Casings;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TracerMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PoliceTracerMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HoleMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PuffMat;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BrassMat;
	int32 NextTracer = 0, NextHole = 0, NextPuff = 0, NextCasing = 0;
	TArray<FBullet> Bullets;
	TArray<FFx> PuffFx, CasingFx;

	// state
	UPROPERTY(Transient) TObjectPtr<UAAARifleAnimInstance> Anim;
	UPROPERTY(Transient) TObjectPtr<UClass> SavedAnimClass;
	TWeakObjectPtr<ATrafficAIManager> TrafficMgr;
	TWeakObjectPtr<USpringArmComponent> Boom;
	TWeakObjectPtr<UCameraComponent> Camera;
	float SavedArm = 300.f, SavedFov = 90.f, SavedWalk = 500.f;
	FVector SavedSocket = FVector::ZeroVector, SavedTargetOffset = FVector::ZeroVector;
	bool bSavedOrient = true, bSavedDesired = false, bSavedYaw = false, bCamSaved = false;
	float CamBlend = 0.f, AimBlend = 0.f;
	// procedural camera shake: kick springs (pitch / yaw / roll / push-back / FOV punch) + short trauma noise
	float CamTrauma = 0.f, CamNoiseT = 0.f;
	float CamKickP = 0.f, CamKickPV = 0.f, CamKickY = 0.f, CamKickYV = 0.f, CamKickR = 0.f, CamKickRV = 0.f;
	float CamPush = 0.f, CamPushV = 0.f, CamFovPunch = 0.f, CamFovPunchV = 0.f;
	FTransform CamBaseRel = FTransform::Identity;
	bool bCamBaseSaved = false, bCamShaking = false;

	bool bArmed = false, bAiming = false, bDead = false, bHudOn = false;
	bool bRadialMenuOpen = false;
	int32 RadialSelectedIndex = 0;
	float RadialMenuAlpha = 0.f;
	FVector2D RadialCursorOffset = FVector2D::ZeroVector;
	// sub-wheels: 0 = main ring, 1 = weapons, 2 = equipment (opened with LMB / Enter on the sector, RMB / Backspace goes back)
	int32 RadialKind = 0, RadialSubIndex = 0, RadialActionSerial = 0;
	float RadialKindT = 9.f, RadialClosedT = 9.f;
	bool bRadialNeedRelease = false;
	FString RadialActionLabel;
	float Stamina = 100.f;
	void RadialAction(const FString& Label) { RadialActionLabel = Label; ++RadialActionSerial; }
	void ExecuteRadialSub(int32 Kind, int32 Index, ACharacter* C);
	void UpdateSquadInput(APlayerController* PC, ACharacter* C, float Dt, bool bInputOk);
	float ReviveHoldT = 0.f;
	TWeakObjectPtr<class AAAAPoliceOfficer> RevivePatient;
	int32 Ammo = 30;
	float FireCD = 0.f, Spread = 0.f, Kick = 0.f, KickV = 0.f, KickSide = 0.f;
	// the weapon turning a little in the grip on each shot (wrist / palm give), on top of the arm animation
	float InHandPitch = 0.f, InHandPitchV = 0.f, InHandYaw = 0.f, InHandYawV = 0.f;
	float RecoilAccum = 0.f, SinceShot = 9.f, ReloadT = -1.f, ArmT = 0.f, FlashT = 0.f;
	float Health = 100.f, SinceHurt = 99.f, HurtFlash = 0.f, ExposureFlashT = 0.f, DeadT = 0.f, HitMarkT = 0.f, HelpT = 12.f;
	int32 Score = 0, LastPoints = 0; float PointsT = 0.f;
	int32 Shooter = 0; float PoliceT = 1.5f; int32 BurstLeft = 0; float BurstT = 0.f; FVector BurstFrom = FVector::ZeroVector;
	FDelegateHandle HudHandle;
	FName SavedMeshProfile;
	FVector SavedMeshLoc = FVector::ZeroVector, DefaultMeshLoc = FVector(0.0, 0.0, -90.0);
	FRotator SavedMeshRot = FRotator::ZeroRotator, DefaultMeshRot = FRotator(0.0, -90.0, 0.0);
	bool bSoundsLoaded = false, bIgnoringMove = false;
	int32 ReloadStage = 0;              // 0 hand going to the magazine, 1 on it, 2 pulled out, 3 new one in, 4 bolt
	FTransform MagRel = FTransform::Identity;
	float TrafficSearchT = 0.f;
	TArray<TWeakObjectPtr<AActor>> SpawnedTargets;

	// Grenade state
	bool bCookingGrenade = false;
	float CookTimer = 0.f;
	float ThrowT = -1.f;
	bool bGrenadeSpawned = false;
	float PendingFuse = 3.2f;
	float GrenadeRegenTimer = 0.f;
	TArray<FVector> TrajectoryPoints;
	FVector PredictedLandingLoc = FVector::ZeroVector;

	// Jumelle / Binoculars State & Two-Hand IK
	bool bBinocularsActive = false;
	float BinocularsBlend = 0.f;
	float BinocularsZoom = 4.0f;
	float TargetBinocularsZoom = 4.0f;
	float RangefinderDist = 0.f;
	TWeakObjectPtr<USceneComponent> BinocularsComp;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> BinocularsParts;

	void ToggleBinoculars();
	void SetBinoculars(bool bActive);
	void BuildBinoculars();
	bool IsBinocularsActive() const { return bBinocularsActive; }
	float GetBinocularsZoom() const { return BinocularsZoom; }
	float GetRangefinderDist() const { return RangefinderDist; }
	float GetBinocularsBlend() const { return BinocularsBlend; }

	// mantling / climbing controller state (was a function-local static map: one per component now)
	struct FClimbControllerState
	{
		bool bMantling = false;
		float MantleTime = 0.f;
		float MantleDuration = 0.65f;
		FVector MantleStartLoc = FVector::ZeroVector;
		FVector MantleApexLoc = FVector::ZeroVector;
		FVector MantleEndLoc = FVector::ZeroVector;
		FVector WallNormal = FVector(-1.0, 0.0, 0.0);
		FVector LedgeEdge = FVector::ZeroVector;
		FVector HandAnchorL = FVector::ZeroVector;
		FVector HandAnchorR = FVector::ZeroVector;
		FVector HandNormL = FVector::UpVector;
		FVector HandNormR = FVector::UpVector;
		FVector FootStartL = FVector::ZeroVector;
		FVector FootStartR = FVector::ZeroVector;
		FVector FootWallL = FVector::ZeroVector;
		FVector FootWallR = FVector::ZeroVector;
		FVector FootWallNormL = FVector(-1.0, 0.0, 0.0);
		FVector FootWallNormR = FVector(-1.0, 0.0, 0.0);
		FVector FootTopL = FVector::ZeroVector;
		FVector FootTopR = FVector::ZeroVector;
		FVector FootTopNormL = FVector::UpVector;
		FVector FootTopNormR = FVector::UpVector;
		float ClimbWeight = 0.f;
		float Cooldown = 0.f;
		// timeline (fractions of MantleDuration): body keys, foot keys, when the hands let go
		int32 NumKeys = 0;
		FVector Key[6];
		float KeyT[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
		int32 NumFootKeys = 0;
		FVector FootKeyL[4], FootKeyR[4], FootKeyNL[4], FootKeyNR[4];
		float FootKeyT[4] = { 0.f, 0.f, 0.f, 0.f };
		float HandReleaseA = 0.62f, HandReleaseB = 0.8f;
		bool bOverWall = false;   // thin wall: over the top and down the far side
		// baked climbing clip (tall walls): when it plays (fractions, < 0 = not used) and how it maps onto the ledge
		float ClipA = -1.f, ClipB = -1.f;
		FVector ClipLip = FVector::ZeroVector, ClipFw = FVector::ForwardVector, ClipRt = FVector::RightVector, ClipPelvisOff = FVector::ZeroVector;
		float ClipRatio = 1.f;
		float ClipFootWL = 0.f, ClipFootWR = 0.f;
		FVector FacePt = FVector::ZeroVector;   // a point on the wall face under the lip (WallNormal points out of it)
		float SinceEnd = 99.f;                  // seconds since the last climb ended
	};
	FClimbControllerState ClimbState;

	bool bRunByDefault = false;   // Caps Lock: walk <-> run
};
