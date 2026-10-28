#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Logic/GoNBlockoutKit.h"

#include "GoNBlockoutPiece.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTextRenderComponent;

// Editable mirror of GoN::Blockout::FCoverRules (Logic/GoNBlockoutKit.h).
USTRUCT(BlueprintType)
struct GONBLOCKOUT_API FGoNCoverRules
{
	GENERATED_BODY()

	// Keep in sync with the character capsule.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float CapsuleRadius = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float CapsuleHalfHeight = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float Spacing = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float EdgeMargin = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float Clearance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float LowMinHeight = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float LowMaxHeight = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float HighMinHeight = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float MinFaceLength = 60.0f;

	// Floors (slabs, terraces) up to this much above a cover's base count as the floor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float MaxStepHeight = 45.0f;
};

// Generated cover point, in world space. Read by AI (M3).
USTRUCT(BlueprintType)
struct GONBLOCKOUT_API FGoNCoverPoint
{
	GENERATED_BODY()

	// Capsule foot position on the floor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	FVector Location = FVector::ZeroVector;

	// Horizontal, from the cover face toward the character.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	FVector Normal = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	bool bHigh = false;

	// World Z of the top of the cover.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	float CoverTop = 0.0f;
};

UENUM(BlueprintType)
enum class EGoNCoverHeight : uint8
{
	Low,
	High
};

// Base of the placeable blockout kit: builds instanced cubes (collision + nav) from the
// pure shape of the subclass, colour-coded per role, and generates cover points.
// Origin = ground level. Keep the actor scale at 1 so the metrics stay true.
UCLASS(Abstract, HideCategories = (Rendering, Replication, Input, LOD, Cooking))
class GONBLOCKOUT_API AGoNBlockoutPiece : public AActor
{
	GENERATED_BODY()

public:
	AGoNBlockoutPiece();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "GoN|Blockout")
	void Rebuild();

	const TArray<FGoNCoverPoint>& GetCoverPoints() const { return CoverPoints; }
	int32 CountCoverPoints(bool bHigh) const;
	const FString& GetPieceLabel() const { return PieceLabel; }

	// Debug: show cover cones and the label during play (`gon.blockout.covers 1`).
	void SetDebugVisibleInGame(bool bVisible);

	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	FGoNCoverRules CoverRules;

	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	bool bShowLabel = true;

	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	bool bShowCoverPoints = true;

	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	TObjectPtr<UStaticMesh> ConeMesh;

	// Tinted per role through a "Color" vector parameter when the material has one.
	UPROPERTY(EditAnywhere, Category = "GoN|Blockout")
	TObjectPtr<UMaterialInterface> BaseMaterial;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const PURE_VIRTUAL(AGoNBlockoutPiece::BuildShape, return GoN::Blockout::FKitShape(););

	GoN::Blockout::FCoverRules GetPureCoverRules() const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GoN|Blockout")
	TArray<FGoNCoverPoint> CoverPoints;

	UPROPERTY(VisibleInstanceOnly, Category = "GoN|Blockout")
	FString PieceLabel;

private:
	// One per GoN::Blockout::EKitRole, in enum order.
	UPROPERTY(VisibleAnywhere, Category = "GoN|Blockout")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RoleComponents;

	UPROPERTY(VisibleAnywhere, Category = "GoN|Blockout")
	TObjectPtr<UInstancedStaticMeshComponent> CoverMarkers;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> LabelComponent;

	// Issues are logged once per change, not on every drag in the editor.
	FString LastLoggedIssues;
	bool bDebugVisibleInGame = false;
};

UCLASS(meta = (DisplayName = "GoN Blockout Douar House"))
class GONBLOCKOUT_API AGoNBlockoutDouarHouse : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	// Rooms side by side along X. Front door in the -X wall; exterior stairs along +Y.
	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "1", ClampMax = "2"))
	int32 Rooms = 1;

	// Interior (clear) size of each room.
	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "100"))
	float RoomSizeX = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "100"))
	float RoomSizeY = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "100"))
	float WallHeight = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "5"))
	float WallThickness = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "20"))
	float DoorWidth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "50"))
	float DoorHeight = 210.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "0"))
	float FloorThickness = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "1"))
	float RoofThickness = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "0"))
	float ParapetHeight = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House")
	bool bExteriorStairs = true;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "40"))
	float StairWidth = 100.0f;

	// Maximum rise; adjusted down so the top step is flush with the roof.
	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "1"))
	float StairRise = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Douar House", meta = (ClampMin = "10"))
	float StairRun = 30.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Casbah Alley"))
class GONBLOCKOUT_API AGoNBlockoutAlley : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	// Runs along +X from the actor; terraces rise by StepRise every StepRun.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "200"))
	float Length = 1200.0f;

	// Clear width between the walls.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "40"))
	float Width = 200.0f;

	// Above the highest terrace.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "100"))
	float WallHeight = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "5"))
	float WallThickness = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "1"))
	float FloorThickness = 5.0f;

	// 0 = flat.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "0"))
	float StepRise = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "30"))
	float StepRun = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "40"))
	float DoorWidth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "100"))
	float DoorHeight = 210.0f;

	// 0 = no doorways. Doorways are staggered between the two walls.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "0"))
	float DoorSpacing = 400.0f;

	// Covered passages (sabats) bridging the alley; 0 = none.
	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "0"))
	float SabatSpacing = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "10"))
	float SabatThickness = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Alley", meta = (ClampMin = "50"))
	float SabatDepth = 200.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Roof Bridge"))
class GONBLOCKOUT_API AGoNBlockoutRoofBridge : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	// Plank along X, its TOP at the actor: place the actor at roof height.
	UPROPERTY(EditAnywhere, Category = "Roof Bridge", meta = (ClampMin = "50"))
	float Length = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Roof Bridge", meta = (ClampMin = "20"))
	float Width = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Roof Bridge", meta = (ClampMin = "2"))
	float Thickness = 20.0f;

	// 0 = no rails.
	UPROPERTY(EditAnywhere, Category = "Roof Bridge", meta = (ClampMin = "0"))
	float RailHeight = 0.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Wall"))
class GONBLOCKOUT_API AGoNBlockoutWall : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Wall", meta = (ClampMin = "10"))
	float Length = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (ClampMin = "10"))
	float Height = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (ClampMin = "5"))
	float Thickness = 30.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Stairs"))
class GONBLOCKOUT_API AGoNBlockoutStairs : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	// Steps rise along +X from the actor.
	UPROPERTY(EditAnywhere, Category = "Stairs", meta = (ClampMin = "40"))
	float Width = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Stairs", meta = (ClampMin = "1"))
	float StepRise = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Stairs", meta = (ClampMin = "10"))
	float StepRun = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Stairs", meta = (ClampMin = "1", ClampMax = "200"))
	int32 StepCount = 10;

	// 0 = no landing.
	UPROPERTY(EditAnywhere, Category = "Stairs", meta = (ClampMin = "0"))
	float LandingDepth = 200.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Rock"))
class GONBLOCKOUT_API AGoNBlockoutRock : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Rock", meta = (ClampMin = "20"))
	float SizeX = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Rock", meta = (ClampMin = "20"))
	float SizeY = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Rock", meta = (ClampMin = "20"))
	float SizeZ = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Rock", meta = (ClampMin = "1", ClampMax = "8"))
	int32 Chunks = 4;

	// Same seed, same rock.
	UPROPERTY(EditAnywhere, Category = "Rock", meta = (ClampMin = "0"))
	int32 Seed = 1;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Slope"))
class GONBLOCKOUT_API AGoNBlockoutSlope : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	// Low end on the ground at the actor, rising toward +X. Red when steeper than walkable.
	UPROPERTY(EditAnywhere, Category = "Slope", meta = (ClampMin = "1", ClampMax = "80"))
	float AngleDeg = 30.0f;

	// Along the surface.
	UPROPERTY(EditAnywhere, Category = "Slope", meta = (ClampMin = "50"))
	float Length = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Slope", meta = (ClampMin = "20"))
	float Width = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Slope", meta = (ClampMin = "2"))
	float Thickness = 20.0f;

	// Keep in sync with the character's movement component.
	UPROPERTY(EditAnywhere, Category = "Slope", meta = (ClampMin = "1", ClampMax = "89"))
	float WalkableSlopeDeg = 44.765f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};

UCLASS(meta = (DisplayName = "GoN Blockout Cover"))
class GONBLOCKOUT_API AGoNBlockoutCover : public AGoNBlockoutPiece
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Cover")
	EGoNCoverHeight CoverHeight = EGoNCoverHeight::Low;

	UPROPERTY(EditAnywhere, Category = "Cover", meta = (ClampMin = "10"))
	float Length = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Cover", meta = (ClampMin = "5"))
	float Thickness = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Cover", meta = (ClampMin = "10"))
	float HeightLow = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Cover", meta = (ClampMin = "10"))
	float HeightHigh = 200.0f;

protected:
	virtual GoN::Blockout::FKitShape BuildShape() const override;
};
