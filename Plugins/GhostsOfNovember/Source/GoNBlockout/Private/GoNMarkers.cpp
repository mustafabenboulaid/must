#include "GoNMarkers.h"

#include "CollisionQueryParams.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GoNBlockoutLog.h"
#include "GoNNarrativeSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

namespace GoNMarkersDetail
{
	TAutoConsoleVariable<int32> CVarMarkersDebug(
		TEXT("gon.markers.debug"),
		0,
		TEXT("GoN Blockout: 1 = draw gameplay markers (spawns, patrols, view cones, zones, triggers) and their labels during play."));

	// Default ACharacter capsule; matches the gym metrics.
	constexpr float SpawnCapsuleRadius = 42.0f;
	constexpr float SpawnCapsuleHalfHeight = 96.0f;

	void SetupNoCollision(UPrimitiveComponent& Component)
	{
		Component.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component.SetGenerateOverlapEvents(false);
		Component.SetCanEverAffectNavigation(false);
	}

	FString NameOrDash(const FName& Value)
	{
		return Value.IsNone() ? FString(TEXT("-")) : Value.ToString();
	}
}

// --- AGoNMarker ---------------------------------------------------------------------------

AGoNMarker::AGoNMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.0, 0.0, 130.0));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetWorldSize(24.0f);
	LabelComponent->SetHiddenInGame(true);
	GoNMarkersDetail::SetupNoCollision(*LabelComponent);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent != nullptr)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteSprite(TEXT("/Engine/EditorResources/S_Note.S_Note"));
		SpriteComponent->Sprite = NoteSprite.Get();
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->SetRelativeLocation(FVector(0.0, 0.0, 60.0));
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif
}

void AGoNMarker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLabel();
}

bool AGoNMarker::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AGoNMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const bool bEditorWorld = World->WorldType == EWorldType::Editor;
	const bool bPlayDebug = !bEditorWorld && GoNMarkersDetail::CVarMarkersDebug.GetValueOnGameThread() > 0;
	if (!bEditorWorld && bPlayDebug != bLabelShownInPlay && LabelComponent != nullptr)
	{
		bLabelShownInPlay = bPlayDebug;
		LabelComponent->SetHiddenInGame(!bPlayDebug);
	}
	if (bEditorWorld || bPlayDebug)
	{
		DrawMarker(*World, GetMarkerColor());
	}
}

FString AGoNMarker::GetMarkerLabel() const
{
	return MakeLabel(TEXT("Marker"), FString());
}

FColor AGoNMarker::GetMarkerColor() const
{
	return FColor(200, 200, 200);
}

void AGoNMarker::DrawMarker(const UWorld& World, const FColor& Color) const
{
	DrawDebugSphere(&World, GetActorLocation() + FVector(0.0, 0.0, 20.0), 20.0f, 8, Color);
}

FString AGoNMarker::MakeLabel(const FString& Prefix, const FString& Detail) const
{
	FString Result = Prefix;
	if (!Detail.IsEmpty())
	{
		Result += TEXT(": ") + Detail;
	}
	if (!Note.IsEmpty())
	{
		Result += TEXT(" - ") + Note;
	}
	return Result;
}

void AGoNMarker::RefreshLabel()
{
	if (LabelComponent != nullptr)
	{
		LabelComponent->SetText(FText::FromString(GetMarkerLabel()));
		LabelComponent->SetTextRenderColor(GetMarkerColor());
	}
}

void AGoNMarker::SetMarkerSprite(UTexture2D* Texture)
{
#if WITH_EDITORONLY_DATA
	if (SpriteComponent != nullptr && Texture != nullptr)
	{
		SpriteComponent->Sprite = Texture;
	}
#endif
}

void AGoNMarker::AddFacingArrow()
{
#if WITH_EDITORONLY_DATA
	FacingArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (FacingArrow != nullptr)
	{
		FacingArrow->SetupAttachment(RootComponent);
		FacingArrow->SetRelativeLocation(FVector(0.0, 0.0, 30.0));
		FacingArrow->ArrowColor = FColor(150, 200, 255);
	}
#endif
}

// --- AGoNSpawnPoint -----------------------------------------------------------------------

AGoNSpawnPoint::AGoNSpawnPoint()
{
	AddFacingArrow();
#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinderOptional<UTexture2D> PlayerSprite(TEXT("/Engine/EditorResources/S_Player.S_Player"));
	SetMarkerSprite(PlayerSprite.Get());
#endif
}

FString AGoNSpawnPoint::GetMarkerLabel() const
{
	const FString Who = bPlayer ? FString(TEXT("player")) : GoNMarkersDetail::NameOrDash(CharacterId);
	return MakeLabel(TEXT("Spawn"), GoNMarkersDetail::NameOrDash(SpawnId) + TEXT(" (") + Who + TEXT(")"));
}

FColor AGoNSpawnPoint::GetMarkerColor() const
{
	return bPlayer ? FColor(0, 200, 255) : FColor(60, 220, 60);
}

void AGoNSpawnPoint::DrawMarker(const UWorld& World, const FColor& Color) const
{
	using namespace GoNMarkersDetail;
	const FVector Centre = GetActorLocation() + FVector(0.0, 0.0, SpawnCapsuleHalfHeight + 2.0f);
	const FCollisionQueryParams QueryParams(FName(TEXT("GoNSpawnPointCheck")), false, this);
	const bool bBlocked = World.OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(SpawnCapsuleRadius, SpawnCapsuleHalfHeight), QueryParams);
	DrawDebugCapsule(&World, Centre, SpawnCapsuleHalfHeight, SpawnCapsuleRadius, FQuat::Identity, bBlocked ? FColor::Red : Color);
	DrawDebugDirectionalArrow(&World, Centre, Centre + GetActorForwardVector() * 80.0, 30.0f, Color, false, -1.0f, 0, 2.0f);
}

// --- AGoNPatrolPath -----------------------------------------------------------------------

AGoNPatrolPath::AGoNPatrolPath()
{
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(RootComponent);
	Spline->SetCanEverAffectNavigation(false);
	Spline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Spline->AddSplinePoint(FVector(300.0 * Index, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	}
	Spline->UpdateSpline();
}

void AGoNPatrolPath::OnConstruction(const FTransform& Transform)
{
	if (Spline != nullptr)
	{
		Spline->SetClosedLoop(bClosedLoop);
	}
	Super::OnConstruction(Transform);
}

TArray<FVector> AGoNPatrolPath::GetPatrolPoints() const
{
	TArray<FVector> Points;
	if (Spline != nullptr)
	{
		const int32 Count = Spline->GetNumberOfSplinePoints();
		Points.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Points.Add(Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
		}
	}
	return Points;
}

FString AGoNPatrolPath::GetMarkerLabel() const
{
	const int32 Count = Spline != nullptr ? Spline->GetNumberOfSplinePoints() : 0;
	return MakeLabel(TEXT("Patrol"), FString::Printf(TEXT("%d points, %.0f cm/s%s"), Count, WalkSpeed, bClosedLoop ? TEXT(", loop") : TEXT("")));
}

FColor AGoNPatrolPath::GetMarkerColor() const
{
	return FColor(255, 160, 0);
}

void AGoNPatrolPath::DrawMarker(const UWorld& World, const FColor& Color) const
{
	const TArray<FVector> Points = GetPatrolPoints();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		DrawDebugSphere(&World, Points[Index] + FVector(0.0, 0.0, 20.0), 20.0f, 8, Color);
		if (Index + 1 < Points.Num())
		{
			DrawDebugLine(&World, Points[Index], Points[Index + 1], Color, false, -1.0f, 0, 2.0f);
		}
	}
	if (bClosedLoop && Points.Num() > 2)
	{
		DrawDebugLine(&World, Points.Last(), Points[0], Color, false, -1.0f, 0, 2.0f);
	}
}

// --- AGoNObserverPost ---------------------------------------------------------------------

AGoNObserverPost::AGoNObserverPost()
{
	AddFacingArrow();
#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TargetSprite(TEXT("/Engine/EditorMaterials/TargetIcon.TargetIcon"));
	SetMarkerSprite(TargetSprite.Get());
#endif
}

FString AGoNObserverPost::GetMarkerLabel() const
{
	return MakeLabel(TEXT("Observer"), FString::Printf(TEXT("%.0f deg, %.0f m"), FieldOfViewDeg, ViewDistance / 100.0f));
}

FColor AGoNObserverPost::GetMarkerColor() const
{
	return FColor(255, 60, 60);
}

void AGoNObserverPost::DrawMarker(const UWorld& World, const FColor& Color) const
{
	const FVector Eye = GetActorLocation() + FVector(0.0, 0.0, EyeHeight);
	DrawDebugLine(&World, GetActorLocation(), Eye, Color);
	DrawDebugSphere(&World, Eye, 12.0f, 8, Color);
	DrawDebugCone(&World, Eye, GetActorForwardVector(), ViewDistance, FMath::DegreesToRadians(FieldOfViewDeg * 0.5f), FMath::DegreesToRadians(VerticalFieldOfViewDeg * 0.5f), 16, Color);
}

// --- AGoNCrowdZone ------------------------------------------------------------------------

AGoNCrowdZone::AGoNCrowdZone()
{
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(RootComponent);
	Zone->InitBoxExtent(FVector(500.0, 500.0, 100.0));
	Zone->SetRelativeLocation(FVector(0.0, 0.0, 100.0)); // sits on the floor
	Zone->ShapeColor = FColor(180, 100, 255);
	GoNMarkersDetail::SetupNoCollision(*Zone);
}

FString AGoNCrowdZone::GetMarkerLabel() const
{
	const FString Profile = CrowdProfile.IsNone() ? FString(TEXT("default")) : CrowdProfile.ToString();
	return MakeLabel(TEXT("Crowd"), FString::Printf(TEXT("%s, %.2f / 10 m2"), *Profile, Density));
}

FColor AGoNCrowdZone::GetMarkerColor() const
{
	return FColor(180, 100, 255);
}

void AGoNCrowdZone::DrawMarker(const UWorld& World, const FColor& Color) const
{
	if (Zone != nullptr)
	{
		DrawDebugBox(&World, Zone->GetComponentLocation(), Zone->GetScaledBoxExtent(), Zone->GetComponentQuat(), Color, false, -1.0f, 0, 2.0f);
	}
}

// --- AGoNTrustZone ------------------------------------------------------------------------

AGoNTrustZone::AGoNTrustZone()
{
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(RootComponent);
	Zone->InitBoxExtent(FVector(500.0, 500.0, 100.0));
	Zone->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	Zone->ShapeColor = FColor(0, 200, 160);
	GoNMarkersDetail::SetupNoCollision(*Zone);
}

void AGoNTrustZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!TrustKey.StartsWith(TEXT("trust."), ESearchCase::CaseSensitive) && TrustKey != LastWarnedTrustKey)
	{
		LastWarnedTrustKey = TrustKey;
		UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: trust zone %s: TrustKey '%s' should start with \"trust.\""), *GetActorNameOrLabel(), *TrustKey);
	}
}

FString AGoNTrustZone::GetMarkerLabel() const
{
	const FString CommunityText = Community.IsEmpty() ? FString() : TEXT(" (") + Community.ToString() + TEXT(")");
	return MakeLabel(TEXT("Trust"), TrustKey + CommunityText);
}

FColor AGoNTrustZone::GetMarkerColor() const
{
	return FColor(0, 200, 160);
}

void AGoNTrustZone::DrawMarker(const UWorld& World, const FColor& Color) const
{
	if (Zone != nullptr)
	{
		DrawDebugBox(&World, Zone->GetComponentLocation(), Zone->GetScaledBoxExtent(), Zone->GetComponentQuat(), Color, false, -1.0f, 0, 2.0f);
	}
}

// --- AGoNMessageDrop ----------------------------------------------------------------------

AGoNMessageDrop::AGoNMessageDrop()
{
	Area = CreateDefaultSubobject<USphereComponent>(TEXT("Area"));
	Area->SetupAttachment(RootComponent);
	Area->InitSphereRadius(50.0f);
	Area->ShapeColor = FColor(255, 220, 0);
	GoNMarkersDetail::SetupNoCollision(*Area);
	// Sprite: the base S_Note.
}

FString AGoNMessageDrop::GetMarkerLabel() const
{
	return MakeLabel(TEXT("Message drop"), GoNMarkersDetail::NameOrDash(MessageId) + TEXT(" @ ") + GoNMarkersDetail::NameOrDash(NetworkId));
}

FColor AGoNMessageDrop::GetMarkerColor() const
{
	return FColor(255, 220, 0);
}

void AGoNMessageDrop::DrawMarker(const UWorld& World, const FColor& Color) const
{
	if (Area != nullptr)
	{
		DrawDebugSphere(&World, Area->GetComponentLocation(), Area->GetScaledSphereRadius(), 12, Color);
	}
}

// --- AGoNNarrativeTrigger -----------------------------------------------------------------

AGoNNarrativeTrigger::AGoNNarrativeTrigger()
{
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->InitBoxExtent(FVector(100.0, 100.0, 100.0));
	TriggerBox->SetRelativeLocation(FVector(0.0, 0.0, 100.0)); // sits on the floor
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetCanEverAffectNavigation(false);
	TriggerBox->ShapeColor = FColor(255, 0, 200);
#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerSprite(TEXT("/Engine/EditorResources/S_Trigger.S_Trigger"));
	SetMarkerSprite(TriggerSprite.Get());
#endif
}

void AGoNNarrativeTrigger::BeginPlay()
{
	Super::BeginPlay();
	if (TriggerBox != nullptr)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AGoNNarrativeTrigger::HandleTriggerOverlap);
	}
}

void AGoNNarrativeTrigger::SetEventName(FName InEventName)
{
	EventName = InEventName;
	RefreshLabel();
}

void AGoNNarrativeTrigger::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bFired)
	{
		return;
	}
	if (bPlayerOnly)
	{
		const APawn* OtherPawn = Cast<APawn>(OtherActor);
		if (OtherPawn == nullptr || !OtherPawn->IsPlayerControlled())
		{
			return;
		}
	}
	if (EventName.IsNone())
	{
		UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: trigger %s has no EventName; nothing sent"), *GetActorNameOrLabel());
		return;
	}
	UGoNNarrativeSubsystem* Narrative = UGoNNarrativeSubsystem::Get(this);
	if (Narrative == nullptr)
	{
		UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: trigger %s: no narrative subsystem; '%s' not sent"), *GetActorNameOrLabel(), *EventName.ToString());
		return;
	}
	const bool bCompleted = Narrative->SendEvent(EventName);
	UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: trigger %s sent '%s' (%s)"), *GetActorNameOrLabel(), *EventName.ToString(),
		bCompleted ? TEXT("completed a beat") : TEXT("not awaited by the current beat"));
	// Only disarm on success, so walking in before the mission reaches the beat doesn't waste it.
	if (bCompleted && bTriggerOnce)
	{
		bFired = true;
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

FString AGoNNarrativeTrigger::GetMarkerLabel() const
{
	const FString Event = EventName.IsNone() ? FString(TEXT("(no event)")) : EventName.ToString();
	return MakeLabel(TEXT("Trigger"), Event + (bTriggerOnce ? TEXT(" (once)") : TEXT("")));
}

FColor AGoNNarrativeTrigger::GetMarkerColor() const
{
	return FColor(255, 0, 200);
}

void AGoNNarrativeTrigger::DrawMarker(const UWorld& World, const FColor& Color) const
{
	if (TriggerBox != nullptr)
	{
		DrawDebugBox(&World, TriggerBox->GetComponentLocation(), TriggerBox->GetScaledBoxExtent(), TriggerBox->GetComponentQuat(), bFired ? FColor(90, 90, 90) : Color, false, -1.0f, 0, 2.0f);
	}
}

// --- AGoNCheckpointMarker -----------------------------------------------------------------

AGoNCheckpointMarker::AGoNCheckpointMarker()
{
	AddFacingArrow();
}

FString AGoNCheckpointMarker::GetMarkerLabel() const
{
	return MakeLabel(TEXT("Checkpoint"), GoNMarkersDetail::NameOrDash(CheckpointId));
}

FColor AGoNCheckpointMarker::GetMarkerColor() const
{
	return FColor(60, 120, 255);
}

void AGoNCheckpointMarker::DrawMarker(const UWorld& World, const FColor& Color) const
{
	using namespace GoNMarkersDetail;
	const FVector Centre = GetActorLocation() + FVector(0.0, 0.0, SpawnCapsuleHalfHeight + 2.0f);
	DrawDebugCapsule(&World, Centre, SpawnCapsuleHalfHeight, SpawnCapsuleRadius, FQuat::Identity, Color);
	DrawDebugDirectionalArrow(&World, Centre, Centre + GetActorForwardVector() * 80.0, 30.0f, Color, false, -1.0f, 0, 2.0f);
}

// --- AGoNScenePoint -----------------------------------------------------------------------

AGoNScenePoint::AGoNScenePoint()
{
	AddFacingArrow();
}

FString AGoNScenePoint::GetMarkerLabel() const
{
	return MakeLabel(TEXT("Scene"), GoNMarkersDetail::NameOrDash(SceneId) + TEXT(" / ") + GoNMarkersDetail::NameOrDash(SceneRole));
}

FColor AGoNScenePoint::GetMarkerColor() const
{
	return FColor(230, 230, 230);
}

void AGoNScenePoint::DrawMarker(const UWorld& World, const FColor& Color) const
{
	const FVector Head = GetActorLocation() + FVector(0.0, 0.0, 100.0);
	DrawDebugSphere(&World, Head, 25.0f, 8, Color);
	DrawDebugDirectionalArrow(&World, Head, Head + GetActorForwardVector() * 80.0, 30.0f, Color, false, -1.0f, 0, 2.0f);
}
