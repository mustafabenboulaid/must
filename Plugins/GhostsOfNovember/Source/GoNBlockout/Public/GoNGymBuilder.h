#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GoNGymBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTextRenderComponent;

// Editable mirror of GoN::Blockout::FGymMetrics (Logic/GoNGymLayout.h).
USTRUCT(BlueprintType)
struct GONBLOCKOUT_API FGoNGymMetrics
{
	GENERATED_BODY()

	// Keep in sync with the character's capsule and movement component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	float CapsuleRadius = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	float CapsuleHalfHeight = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	float MaxStepHeight = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	float WalkableSlopeDeg = 44.765f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	float LowCoverHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	float HighCoverHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	float StepRise = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	float StepRun = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	int32 StepCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	TArray<float> VaultHeights = {60.0f, 90.0f, 120.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	TArray<float> LedgeHeights = {150.0f, 200.0f, 250.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	TArray<float> RampAnglesDeg = {20.0f, 35.0f, 50.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Course")
	TArray<float> DoorWidths = {90.0f, 120.0f, 200.0f};
};

// Generates the metrics gym (cover, vault, climb, stairs, ramps, doorways) from C++.
// Drop it into an empty level, or let AGoNGymGameMode spawn it (`gon.gym.open`).
UCLASS(meta = (DisplayName = "GoN Gym Builder"))
class GONBLOCKOUT_API AGoNGymBuilder : public AActor
{
	GENERATED_BODY()

public:
	AGoNGymBuilder();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "GoN|Gym")
	void Rebuild();

	// Capsule-centre spawn point and facing, in world space.
	FTransform GetPlayerStartTransform() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoN|Gym")
	FGoNGymMetrics Metrics;

	UPROPERTY(EditAnywhere, Category = "GoN|Gym")
	bool bShowLabels = true;

	UPROPERTY(EditAnywhere, Category = "GoN|Gym")
	TObjectPtr<UStaticMesh> CubeMesh;

	// Tinted per piece through a "Color" vector parameter when the material has one.
	UPROPERTY(EditAnywhere, Category = "GoN|Gym")
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	UPROPERTY(VisibleAnywhere, Category = "GoN|Gym")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PieceComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> LabelComponents;

	FVector PlayerStartLocal = FVector::ZeroVector;
	float PlayerStartYaw = 0.0f;
};
