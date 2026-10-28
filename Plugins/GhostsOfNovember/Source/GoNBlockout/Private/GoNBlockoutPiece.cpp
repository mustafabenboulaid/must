#include "GoNBlockoutPiece.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GoNBlockoutLog.h"
#include "GoNStringConv.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GoNBlockoutPieceDetail
{
	FLinearColor RoleColor(GoN::Blockout::EKitRole Role)
	{
		using GoN::Blockout::EKitRole;
		switch (Role)
		{
		case EKitRole::Floor: return FLinearColor(0.35f, 0.35f, 0.33f);
		case EKitRole::Wall: return FLinearColor(0.40f, 0.25f, 0.14f);
		case EKitRole::Roof: return FLinearColor(0.72f, 0.58f, 0.38f);
		case EKitRole::Parapet: return FLinearColor(0.60f, 0.45f, 0.30f);
		case EKitRole::LowCover: return FLinearColor(0.20f, 0.55f, 0.20f);
		case EKitRole::HighCover: return FLinearColor(0.08f, 0.30f, 0.10f);
		case EKitRole::Stairs: return FLinearColor(0.15f, 0.35f, 0.80f);
		case EKitRole::Slope: return FLinearColor(0.85f, 0.80f, 0.20f);
		case EKitRole::SlopeTooSteep: return FLinearColor(0.80f, 0.12f, 0.10f);
		case EKitRole::Rock: return FLinearColor(0.40f, 0.36f, 0.30f);
		case EKitRole::Bridge: return FLinearColor(0.90f, 0.50f, 0.10f);
		}
		return FLinearColor::White;
	}

	FLinearColor CoverMarkerColor()
	{
		return FLinearColor(0.10f, 0.95f, 0.35f);
	}

	FVector ToVector(const GoN::Blockout::FVec3& In)
	{
		return FVector(In.X, In.Y, In.Z);
	}

	// Reuses the component's dynamic material so dragging a piece doesn't pile up MIDs.
	void ApplyColor(UInstancedStaticMeshComponent& Component, UMaterialInterface* BaseMaterial, const FLinearColor& Color, UObject* Outer)
	{
		if (BaseMaterial == nullptr)
		{
			return;
		}
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component.GetMaterial(0));
		if (Material == nullptr || Material->Parent != BaseMaterial)
		{
			Material = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
			Component.SetMaterial(0, Material);
		}
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

AGoNBlockoutPiece::AGoNBlockoutPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	ConeMesh = ConeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	for (int32 Index = 0; Index < GoN::Blockout::KitRoleCount; ++Index)
	{
		const FString ComponentName = FString::Printf(TEXT("Kit_%s"), ANSI_TO_TCHAR(GoN::Blockout::KitRoleName(static_cast<GoN::Blockout::EKitRole>(Index))));
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName(*ComponentName));
		Component->SetupAttachment(RootComponent);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Component->SetCanEverAffectNavigation(true);
		RoleComponents.Add(Component);
	}

	CoverMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CoverMarkers"));
	CoverMarkers->SetupAttachment(RootComponent);
	CoverMarkers->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CoverMarkers->SetCanEverAffectNavigation(false);
	CoverMarkers->SetHiddenInGame(true);
	CoverMarkers->SetCastShadow(false);
}

void AGoNBlockoutPiece::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AGoNBlockoutPiece::BeginPlay()
{
	Super::BeginPlay();
	// The label is transient (not saved or duplicated into PIE); regenerate everything.
	Rebuild();
}

GoN::Blockout::FCoverRules AGoNBlockoutPiece::GetPureCoverRules() const
{
	GoN::Blockout::FCoverRules Rules;
	Rules.CapsuleRadius = CoverRules.CapsuleRadius;
	Rules.CapsuleHalfHeight = CoverRules.CapsuleHalfHeight;
	Rules.Spacing = CoverRules.Spacing;
	Rules.EdgeMargin = CoverRules.EdgeMargin;
	Rules.Clearance = CoverRules.Clearance;
	Rules.LowMinHeight = CoverRules.LowMinHeight;
	Rules.LowMaxHeight = CoverRules.LowMaxHeight;
	Rules.HighMinHeight = CoverRules.HighMinHeight;
	Rules.MinFaceLength = CoverRules.MinFaceLength;
	Rules.MaxStepHeight = CoverRules.MaxStepHeight;
	return Rules;
}

int32 AGoNBlockoutPiece::CountCoverPoints(bool bHigh) const
{
	int32 Count = 0;
	for (const FGoNCoverPoint& Point : CoverPoints)
	{
		Count += Point.bHigh == bHigh ? 1 : 0;
	}
	return Count;
}

void AGoNBlockoutPiece::SetDebugVisibleInGame(bool bVisible)
{
	bDebugVisibleInGame = bVisible;
	if (CoverMarkers != nullptr)
	{
		CoverMarkers->SetHiddenInGame(!bVisible);
	}
	if (IsValid(LabelComponent))
	{
		LabelComponent->SetHiddenInGame(!bVisible);
	}
}

void AGoNBlockoutPiece::Rebuild()
{
	using namespace GoNBlockoutPieceDetail;
	const GoN::Blockout::FKitShape Shape = BuildShape();
	PieceLabel = GoN::ToFString(Shape.Label);

	TArray<FString> IssueLines;
	for (const std::string& Issue : Shape.Issues)
	{
		IssueLines.Add(GoN::ToFString(Issue));
	}
	if (!GetActorScale3D().Equals(FVector::OneVector, 0.001))
	{
		IssueLines.Add(TEXT("actor scale is not 1, so the piece no longer matches its metrics (change the parameters instead)"));
	}
	const FString IssuesText = FString::Join(IssueLines, TEXT("\n"));
	if (IssuesText != LastLoggedIssues)
	{
		LastLoggedIssues = IssuesText;
		for (const FString& Line : IssueLines)
		{
			UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: %s: %s"), *GetActorNameOrLabel(), *Line);
		}
	}

	for (int32 Index = 0; Index < RoleComponents.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Component = RoleComponents[Index];
		if (Component == nullptr)
		{
			continue;
		}
		Component->ClearInstances();
		Component->SetStaticMesh(CubeMesh);
		ApplyColor(*Component, BaseMaterial, RoleColor(static_cast<GoN::Blockout::EKitRole>(Index)), this);
	}

	const double CubeSize = 100.0; // /Engine/BasicShapes/Cube is 100 cm, pivot at the centre
	double HighestTop = 0.0;
	for (const GoN::Blockout::FKitBox& Box : Shape.Boxes)
	{
		HighestTop = FMath::Max(HighestTop, Box.Top());
		const int32 Index = static_cast<int32>(Box.Role);
		if (!RoleComponents.IsValidIndex(Index) || RoleComponents[Index] == nullptr)
		{
			continue;
		}
		const FVector Scale(Box.Extent.X * 2.0 / CubeSize, Box.Extent.Y * 2.0 / CubeSize, Box.Extent.Z * 2.0 / CubeSize);
		RoleComponents[Index]->AddInstance(FTransform(FRotator(Box.PitchDeg, Box.YawDeg, 0.0), ToVector(Box.Center), Scale));
	}

	if (IsValid(LabelComponent))
	{
		LabelComponent->DestroyComponent();
	}
	LabelComponent = nullptr;
	if (bShowLabel && !PieceLabel.IsEmpty())
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
		Label->SetupAttachment(RootComponent);
		Label->SetText(FText::FromString(PieceLabel));
		Label->SetWorldSize(28.0f);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetTextRenderColor(FColor::White);
		Label->SetRelativeLocation(FVector(0.0, 0.0, HighestTop + 40.0));
		Label->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // readable from -X
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCanEverAffectNavigation(false);
		Label->SetHiddenInGame(!bDebugVisibleInGame);
		Label->RegisterComponent();
		LabelComponent = Label;
	}

	// Cover points in world space for AI, plus cones in the editor: the cone points at the
	// cover face, low = 30 cm long, high = 50 cm long.
	CoverPoints.Reset();
	const FTransform ActorTransform = GetActorTransform();
	if (CoverMarkers != nullptr)
	{
		CoverMarkers->ClearInstances();
		CoverMarkers->SetStaticMesh(ConeMesh);
		ApplyColor(*CoverMarkers, BaseMaterial, CoverMarkerColor(), this);
	}
	for (const GoN::Blockout::FCoverPoint& Point : Shape.CoverPoints)
	{
		const FVector LocalPosition = ToVector(Point.Position);
		const FVector LocalNormal = ToVector(Point.Normal);
		const bool bHighCover = Point.Height == GoN::Blockout::ECoverHeight::High;

		FGoNCoverPoint& WorldPoint = CoverPoints.AddDefaulted_GetRef();
		WorldPoint.Location = ActorTransform.TransformPosition(LocalPosition);
		WorldPoint.Normal = ActorTransform.TransformVectorNoScale(LocalNormal);
		WorldPoint.bHigh = bHighCover;
		WorldPoint.CoverTop = static_cast<float>(ActorTransform.TransformPosition(FVector(Point.Position.X, Point.Position.Y, Point.CoverTop)).Z);

		if (CoverMarkers != nullptr)
		{
			const double ConeLength = bHighCover ? 50.0 : 30.0;
			const FRotator ConeRotation = FRotationMatrix::MakeFromZ(-LocalNormal).Rotator();
			CoverMarkers->AddInstance(FTransform(ConeRotation, LocalPosition + FVector(0.0, 0.0, 15.0), FVector(0.2, 0.2, ConeLength / CubeSize)));
		}
	}
	if (CoverMarkers != nullptr)
	{
		CoverMarkers->SetVisibility(bShowCoverPoints);
		CoverMarkers->SetHiddenInGame(!bDebugVisibleInGame);
	}
	UE_LOG(LogGoNBlockout, Verbose, TEXT("GoN Blockout: %s rebuilt: %d boxes, %d cover points"), *GetActorNameOrLabel(), static_cast<int32>(Shape.Boxes.size()), CoverPoints.Num());
}

GoN::Blockout::FKitShape AGoNBlockoutDouarHouse::BuildShape() const
{
	GoN::Blockout::FDouarHouseParams Params;
	Params.Rooms = Rooms;
	Params.RoomSizeX = RoomSizeX;
	Params.RoomSizeY = RoomSizeY;
	Params.WallHeight = WallHeight;
	Params.WallThickness = WallThickness;
	Params.DoorWidth = DoorWidth;
	Params.DoorHeight = DoorHeight;
	Params.FloorThickness = FloorThickness;
	Params.RoofThickness = RoofThickness;
	Params.ParapetHeight = ParapetHeight;
	Params.bExteriorStairs = bExteriorStairs;
	Params.StairWidth = StairWidth;
	Params.StairRise = StairRise;
	Params.StairRun = StairRun;
	return GoN::Blockout::BuildDouarHouse(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutAlley::BuildShape() const
{
	GoN::Blockout::FAlleyParams Params;
	Params.Length = Length;
	Params.Width = Width;
	Params.WallHeight = WallHeight;
	Params.WallThickness = WallThickness;
	Params.FloorThickness = FloorThickness;
	Params.StepRise = StepRise;
	Params.StepRun = StepRun;
	Params.DoorWidth = DoorWidth;
	Params.DoorHeight = DoorHeight;
	Params.DoorSpacing = DoorSpacing;
	Params.SabatSpacing = SabatSpacing;
	Params.SabatThickness = SabatThickness;
	Params.SabatDepth = SabatDepth;
	return GoN::Blockout::BuildAlley(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutRoofBridge::BuildShape() const
{
	GoN::Blockout::FRoofBridgeParams Params;
	Params.Length = Length;
	Params.Width = Width;
	Params.Thickness = Thickness;
	Params.RailHeight = RailHeight;
	return GoN::Blockout::BuildRoofBridge(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutWall::BuildShape() const
{
	GoN::Blockout::FWallParams Params;
	Params.Length = Length;
	Params.Height = Height;
	Params.Thickness = Thickness;
	return GoN::Blockout::BuildWall(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutStairs::BuildShape() const
{
	GoN::Blockout::FStairsParams Params;
	Params.Width = Width;
	Params.StepRise = StepRise;
	Params.StepRun = StepRun;
	Params.StepCount = StepCount;
	Params.LandingDepth = LandingDepth;
	return GoN::Blockout::BuildStairs(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutRock::BuildShape() const
{
	GoN::Blockout::FRockParams Params;
	Params.SizeX = SizeX;
	Params.SizeY = SizeY;
	Params.SizeZ = SizeZ;
	Params.Chunks = Chunks;
	Params.Seed = static_cast<uint32_t>(FMath::Max(0, Seed));
	return GoN::Blockout::BuildRock(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutSlope::BuildShape() const
{
	GoN::Blockout::FSlopeParams Params;
	Params.AngleDeg = AngleDeg;
	Params.Length = Length;
	Params.Width = Width;
	Params.Thickness = Thickness;
	Params.WalkableSlopeDeg = WalkableSlopeDeg;
	return GoN::Blockout::BuildSlope(Params, GetPureCoverRules());
}

GoN::Blockout::FKitShape AGoNBlockoutCover::BuildShape() const
{
	GoN::Blockout::FCoverParams Params;
	Params.Height = CoverHeight == EGoNCoverHeight::High ? GoN::Blockout::ECoverHeight::High : GoN::Blockout::ECoverHeight::Low;
	Params.Length = Length;
	Params.Thickness = Thickness;
	Params.HeightLow = HeightLow;
	Params.HeightHigh = HeightHigh;
	return GoN::Blockout::BuildCover(Params, GetPureCoverRules());
}
