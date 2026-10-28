#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AAAWeaponDataAsset.generated.h"

class UStaticMesh;
class UMaterialInterface;
class USoundBase;

UENUM(BlueprintType)
enum class EAAAWeaponCategory : uint8
{
	Pistol UMETA(DisplayName = "Handgun / Pistol"),
	Rifle  UMETA(DisplayName = "Assault Rifle"),
	LMG    UMETA(DisplayName = "Heavy LMG / Support"),
	Shotgun UMETA(DisplayName = "Shotgun"),
	Sniper UMETA(DisplayName = "Sniper Rifle"),
	Custom UMETA(DisplayName = "Custom Weapon")
};

/**
 * UAAAWeaponDataAsset - Primary Data Asset for extensible data-driven weapon configuration.
 * Allows creating and tuning weapons completely in Blueprint editor without touching C++ code.
 */
UCLASS(BlueprintType, Blueprintable)
class AAAWEAPONS_API UAAAWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Identification & Metadata
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Identity")
	FName WeaponID = TEXT("Colt1911");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Identity")
	FText DisplayName = FText::FromString(TEXT("Colt 1911 (.45 ACP)"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Identity")
	EAAAWeaponCategory Category = EAAAWeaponCategory::Pistol;

	// 3D Visual Meshes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visuals")
	TObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visuals")
	TObjectPtr<UStaticMesh> SlideMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visuals")
	TObjectPtr<UStaticMesh> HammerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visuals")
	TObjectPtr<UStaticMesh> MagazineMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visuals")
	TObjectPtr<UMaterialInterface> MaterialOverride;

	// Weapon Transforms, Sockets & Offsets (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Offsets")
	FVector GripOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Offsets")
	FRotator GripRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Offsets")
	FVector MuzzleLocation = FVector(17.43f, 0.0f, 8.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Offsets")
	FVector EjectionLocation = FVector(7.5f, 1.1f, 7.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Offsets")
	FVector HammerPivot = FVector(-2.6f, 0.0f, 6.0f);

	// Mechanical Animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mechanical Animation")
	bool bHasSlideBlowback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mechanical Animation", meta = (EditCondition = "bHasSlideBlowback"))
	float SlideTravelDistance = 3.8f; // cm of rearward travel

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mechanical Animation", meta = (EditCondition = "bHasSlideBlowback"))
	float SlideReturnSpeed = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mechanical Animation")
	bool bHasHammer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mechanical Animation", meta = (EditCondition = "bHasHammer"))
	float HammerCockedAngle = 32.0f; // degrees

	// Ballistics & Combat Performance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat", meta = (ClampMin = "1", ClampMax = "500"))
	float Damage = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat", meta = (ClampMin = "0.5", ClampMax = "50"))
	float FireRate = 7.5f; // rounds per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat")
	bool bFullAuto = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "1", ClampMax = "500"))
	int32 MagazineSize = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo")
	int32 DefaultReserveAmmo = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ballistics", meta = (ClampMin = "50", ClampMax = "3000"))
	float MuzzleVelocity = 350.0f; // m/s

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ballistics")
	float BulletGravity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ballistics")
	float ImpactImpulse = 9000.0f;

	// Spread & Recoil
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float RecoilPitch = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float RecoilYaw = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float RecoilRecovery = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float SpreadHip = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float SpreadAim = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float SpreadPerShot = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float SpreadMax = 3.5f;

	// Audio Foley & Combat SFX
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> ShotSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> MagOutSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> MagInSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> BoltSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> DrySound;
};
