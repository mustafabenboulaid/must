// AAA City Weapons - pop-up training target: a round plate on a stand, falls back when hit, stands up again
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AAATrainingTarget.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AAAWEAPONS_API AAAATrainingTarget : public AActor
{
	GENERATED_BODY()
public:
	AAAATrainingTarget();

	/** a bullet hit at this world point: returns the score (10 bull's-eye ... 2 edge), 0 when it was already down */
	int32 OnHit(const FVector& WorldPoint, const FVector& Dir);

	UPROPERTY(EditAnywhere, Category = "Target", meta = (ClampMin = "0.5", ClampMax = "30"))
	float ResetSeconds = 3.f;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Target") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Target") TObjectPtr<USceneComponent> Hinge;

private:
	UStaticMeshComponent* Part(USceneComponent* Parent, bool bCylinder, const FVector& Loc, const FRotator& Rot, const FVector& SizeCm, const FLinearColor& Color, bool bCollide);
	float DownT = -1.f, Angle = 0.f, SwayV = 0.f, Sway = 0.f;
	static constexpr float PlateRadius = 30.f;   // cm
};
