#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AAASmartHudWidget.generated.h"

class UAAAWeaponComponent;

UCLASS()
class AAAWEAPONS_API UAAASmartHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitWeapon(UAAAWeaponComponent* InWeapon);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UAAAWeaponComponent> Weapon;

	// Smooth animation state
	float SmoothHealth = 100.f;
	float GhostHealth = 100.f;
	float GhostDelay = 0.f;
	float HitMarkerAlpha = 0.f;
	float WantedPulse = 0.f;
	float CrosshairSpread = 0.f;

	// ---- "Ghosts of November" HUD (AAAGhostsHud.cpp) ----
	void TickGhosts(float InDt);
	int32 PaintGhosts(const FGeometry& G, FSlateWindowElementList& Out, int32 LayerId) const;
	struct FGhostSplat { FVector2f P = FVector2f::ZeroVector; float Size = 100.f; float T = 0.f; };
	float GT = 0.f, RingT = 0.f;
	bool bRingWasOpen = false;
	float PtrRot = 0.f, PtrRotV = 0.f, PtrTarget = 0.f;
	float NeedleRot = 0.f, NeedleV = 0.f;
	int32 PtrSel = 0;
	float HpShown = 100.f, HpV = 0.f, LastHp = 100.f, LastHpForHeal = -1.f;
	float StShown = 100.f, StV = 0.f;
	int32 ShownKind = 0, LastActionSerial = 0;
	float KindCloseT = -1.f;
	float HitFlash = 0.f, CritT = 0.f;
	TArray<FGhostSplat> Splats;
	float NoiseT = 0.f, NoiseBoostT = 0.f;
	float Noise[9] = { 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f, 0.2f };
	bool bDetected = false;
	FString ToastText;
	float ToastT = -1.f;
	int32 LastWeaponType = -1;
	bool bLastArmed = false;
	float EquipAnimT = 9.f, KickAnimT = 9.f, LastSinceShot = 9.f;
	int32 LastAmmo = -1;
	float Heading = 0.f, LookX = 0.f, LookY = 0.f, PrevYaw = 0.f, PrevPitch = 0.f;
	bool bLookInit = false;
	float RainT = 0.f, GrainT = 0.f;
	FVector2f GrainOff = FVector2f::ZeroVector;
};
