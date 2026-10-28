#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GoNWoundedComrade.generated.h"

class AGoNCharacter;
class USkeletalMeshComponent;
class USphereComponent;
class UTextRenderComponent;

// A wounded (fictional) comrade the player can carry: E near him picks him up onto the
// shoulders, E again puts him down. Lies in a held pose (end of a death clip by default)
// until a carry/wounded clip is imported.
UCLASS(meta = (DisplayName = "GoN Wounded Comrade"))
class GONCHARACTER_API AGoNWoundedComrade : public AActor
{
	GENERATED_BODY()

public:
	AGoNWoundedComrade();

	virtual void BeginPlay() override;

	void SetCarried(AGoNCharacter* NewCarrier);
	AGoNCharacter* GetCarrier() const { return Carrier; }

	// Lays him on the ground near Location (traced down from at most 1.5 m above it).
	void PlaceOnGroundNear(const FVector& Location, float Yaw, const AActor* IgnoredActor);

	// characters.json id of the fictional comrade this dummy stands for (optional).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Wounded")
	FName CharacterId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Wounded")
	TObjectPtr<USphereComponent> InteractVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Wounded")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "GoN|Wounded")
	TObjectPtr<UTextRenderComponent> Label;

private:
	void ApplyPose();

	UPROPERTY(Transient)
	TObjectPtr<AGoNCharacter> Carrier;
};
