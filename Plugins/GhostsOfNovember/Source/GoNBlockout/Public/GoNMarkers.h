#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"

#include "GoNMarkers.generated.h"

class UArrowComponent;
class UBillboardComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USphereComponent;
class USplineComponent;
class UTextRenderComponent;
class UTexture2D;

// Gameplay markers placed in blockout levels. Until M3-M6 they only store data (except
// AGoNNarrativeTrigger). In the editor they draw their shape every frame; during play only
// with `gon.markers.debug 1`. Place them on the floor (actor origin = feet).
UCLASS(Abstract, HideCategories = (Rendering, Replication, Input, LOD, Cooking, Physics))
class GONBLOCKOUT_API AGoNMarker : public AActor
{
	GENERATED_BODY()

public:
	AGoNMarker();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	virtual FString GetMarkerLabel() const;
	virtual FColor GetMarkerColor() const;

	UPROPERTY(EditAnywhere, Category = "GoN|Marker")
	FString Note;

protected:
	// Called from Tick when the marker should be visualised (one-frame debug lines).
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const;

	// "<Prefix>: <Id>" plus the note, for GetMarkerLabel overrides.
	FString MakeLabel(const FString& Prefix, const FString& Detail) const;
	void RefreshLabel();

	// Subclass constructors: editor sprite and an optional facing arrow.
	void SetMarkerSprite(UTexture2D* Texture);
	void AddFacingArrow();

	UPROPERTY(VisibleAnywhere, Category = "GoN|Marker")
	TObjectPtr<UTextRenderComponent> LabelComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;

	UPROPERTY()
	TObjectPtr<UArrowComponent> FacingArrow;
#endif

private:
	bool bLabelShownInPlay = false;
};

UCLASS(meta = (DisplayName = "GoN Spawn Point"))
class GONBLOCKOUT_API AGoNSpawnPoint : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNSpawnPoint();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Spawn")
	FName SpawnId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Spawn")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Spawn")
	bool bPlayer = false;

protected:
	// Standing capsule (radius 42, half height 96): green when free, red when blocked.
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

UCLASS(meta = (DisplayName = "GoN Patrol Path"))
class GONBLOCKOUT_API AGoNPatrolPath : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNPatrolPath();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	// Spline points in world space, in order.
	UFUNCTION(BlueprintCallable, Category = "GoN|Patrol")
	TArray<FVector> GetPatrolPoints() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Patrol")
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Patrol")
	bool bClosedLoop = false;

	// Wait at each point (index = spline point); missing entries = no wait.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Patrol")
	TArray<float> WaitSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Patrol", meta = (ClampMin = "0"))
	float WalkSpeed = 150.0f;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

UCLASS(meta = (DisplayName = "GoN Observer Post"))
class GONBLOCKOUT_API AGoNObserverPost : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNObserverPost();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Observer", meta = (ClampMin = "1", ClampMax = "179"))
	float FieldOfViewDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Observer", meta = (ClampMin = "1", ClampMax = "179"))
	float VerticalFieldOfViewDeg = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Observer", meta = (ClampMin = "0"))
	float ViewDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Observer", meta = (ClampMin = "0"))
	float EyeHeight = 165.0f;

protected:
	// View cone from eye height along the actor's forward.
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

UCLASS(meta = (DisplayName = "GoN Crowd Zone"))
class GONBLOCKOUT_API AGoNCrowdZone : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNCrowdZone();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Crowd")
	TObjectPtr<UBoxComponent> Zone;

	// People per 10 m2.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Crowd", meta = (ClampMin = "0"))
	float Density = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Crowd")
	FName CrowdProfile;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

UCLASS(meta = (DisplayName = "GoN Trust Zone"))
class GONBLOCKOUT_API AGoNTrustZone : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNTrustZone();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Trust")
	TObjectPtr<UBoxComponent> Zone;

	// Narrative stat key; must start with "trust.".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Trust")
	FString TrustKey = TEXT("trust.douar");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Trust")
	FText Community;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;

private:
	FString LastWarnedTrustKey;
};

UCLASS(meta = (DisplayName = "GoN Message Drop"))
class GONBLOCKOUT_API AGoNMessageDrop : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNMessageDrop();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Message")
	TObjectPtr<USphereComponent> Area;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Message")
	FName MessageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Message")
	FName NetworkId;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

// Sends EventName to UGoNNarrativeSubsystem when the player (or any pawn) walks in.
UCLASS(meta = (DisplayName = "GoN Narrative Trigger"))
class GONBLOCKOUT_API AGoNNarrativeTrigger : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNNarrativeTrigger();

	virtual void BeginPlay() override;
	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	// For code that spawns triggers.
	void SetEventName(FName InEventName);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoN|Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	// Narrative event, e.g. "reach_cover".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Trigger")
	FName EventName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Trigger")
	bool bPlayerOnly = true;

	// Disarm once the event completed a beat (not when no beat was waiting for it).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Trigger")
	bool bTriggerOnce = true;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;

	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bFired = false;
};

UCLASS(meta = (DisplayName = "GoN Checkpoint Marker"))
class GONBLOCKOUT_API AGoNCheckpointMarker : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNCheckpointMarker();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Checkpoint")
	FName CheckpointId;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};

UCLASS(meta = (DisplayName = "GoN Scene Point"))
class GONBLOCKOUT_API AGoNScenePoint : public AGoNMarker
{
	GENERATED_BODY()

public:
	AGoNScenePoint();

	virtual FString GetMarkerLabel() const override;
	virtual FColor GetMarkerColor() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Scene")
	FName SceneId;

	// Who stands here in the scene ("Role" would clash with AActor::Role).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoN|Scene", meta = (DisplayName = "Role"))
	FName SceneRole;

protected:
	virtual void DrawMarker(const UWorld& World, const FColor& Color) const override;
};
