#include "GoNGymBuilder.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GoNBlockoutLog.h"
#include "GoNStringConv.h"
#include "Logic/GoNGymLayout.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GoNGymBuilderDetail
{
	constexpr int32 PieceCount = static_cast<int32>(GoN::Blockout::EGymPiece::DoorWall) + 1;

	FLinearColor PieceColor(GoN::Blockout::EGymPiece Piece)
	{
		using GoN::Blockout::EGymPiece;
		switch (Piece)
		{
		case EGymPiece::Floor: return FLinearColor(0.35f, 0.35f, 0.33f);
		case EGymPiece::Wall: return FLinearColor(0.18f, 0.18f, 0.18f);
		case EGymPiece::LowCover: return FLinearColor(0.20f, 0.55f, 0.20f);
		case EGymPiece::HighCover: return FLinearColor(0.08f, 0.30f, 0.10f);
		case EGymPiece::Vault: return FLinearColor(0.90f, 0.50f, 0.10f);
		case EGymPiece::Ledge: return FLinearColor(0.75f, 0.15f, 0.12f);
		case EGymPiece::Stairs: return FLinearColor(0.15f, 0.35f, 0.80f);
		case EGymPiece::Ramp: return FLinearColor(0.85f, 0.80f, 0.20f);
		case EGymPiece::DoorWall: return FLinearColor(0.45f, 0.30f, 0.18f);
		}
		return FLinearColor::White;
	}

	std::vector<double> ToStdVector(const TArray<float>& In)
	{
		// Raw pointers: TArray's ranged-for iterators are not std iterators in checked builds.
		return std::vector<double>(In.GetData(), In.GetData() + In.Num());
	}

	GoN::Blockout::FGymMetrics ToPure(const FGoNGymMetrics& In)
	{
		GoN::Blockout::FGymMetrics Out;
		Out.CapsuleRadius = In.CapsuleRadius;
		Out.CapsuleHalfHeight = In.CapsuleHalfHeight;
		Out.MaxStepHeight = In.MaxStepHeight;
		Out.WalkableSlopeDeg = In.WalkableSlopeDeg;
		Out.LowCoverHeight = In.LowCoverHeight;
		Out.HighCoverHeight = In.HighCoverHeight;
		Out.StepRise = In.StepRise;
		Out.StepRun = In.StepRun;
		Out.StepCount = FMath::Max(0, In.StepCount);
		Out.VaultHeights = ToStdVector(In.VaultHeights);
		Out.LedgeHeights = ToStdVector(In.LedgeHeights);
		Out.RampAnglesDeg = ToStdVector(In.RampAnglesDeg);
		Out.DoorWidths = ToStdVector(In.DoorWidths);
		return Out;
	}

	FVector ToVector(const GoN::Blockout::FVec3& In)
	{
		return FVector(In.X, In.Y, In.Z);
	}
}

AGoNGymBuilder::AGoNGymBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	for (int32 Index = 0; Index < GoNGymBuilderDetail::PieceCount; ++Index)
	{
		const FString Name = FString::Printf(TEXT("Gym_%s"), ANSI_TO_TCHAR(GoN::Blockout::GymPieceName(static_cast<GoN::Blockout::EGymPiece>(Index))));
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(FName(*Name));
		Component->SetupAttachment(RootComponent);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		PieceComponents.Add(Component);
	}
}

void AGoNGymBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AGoNGymBuilder::BeginPlay()
{
	Super::BeginPlay();
	// Labels are transient (not saved or duplicated into PIE); regenerate everything.
	Rebuild();
}

void AGoNGymBuilder::Rebuild()
{
	using namespace GoNGymBuilderDetail;
	const GoN::Blockout::FGymMetrics PureMetrics = ToPure(Metrics);
	const GoN::Blockout::FGymLayout Layout = GoN::Blockout::BuildGymLayout(PureMetrics);
	for (const std::string& Issue : GoN::Blockout::ValidateGymLayout(Layout, PureMetrics))
	{
		UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: gym: %s"), *GoN::ToFString(Issue));
	}

	for (int32 Index = 0; Index < PieceComponents.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Component = PieceComponents[Index];
		if (Component == nullptr)
		{
			continue;
		}
		Component->ClearInstances();
		Component->SetStaticMesh(CubeMesh);
		if (BaseMaterial != nullptr)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), PieceColor(static_cast<GoN::Blockout::EGymPiece>(Index)));
			Component->SetMaterial(0, Material);
		}
	}

	for (UTextRenderComponent* Label : LabelComponents)
	{
		if (IsValid(Label))
		{
			Label->DestroyComponent();
		}
	}
	LabelComponents.Reset();

	const auto AddLabel = [this](const FString& Text, const FVector& Location)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
		Label->SetupAttachment(RootComponent);
		Label->SetText(FText::FromString(Text));
		Label->SetWorldSize(36.0f);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetTextRenderColor(FColor::White);
		Label->SetRelativeLocation(Location);
		Label->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // readable from the spawn side (-X)
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->RegisterComponent();
		LabelComponents.Add(Label);
	};

	const float CubeSize = 100.0f; // /Engine/BasicShapes/Cube is 100 cm, pivot at the centre
	for (const GoN::Blockout::FGymBox& Box : Layout.Boxes)
	{
		const int32 Index = static_cast<int32>(Box.Piece);
		if (!PieceComponents.IsValidIndex(Index) || PieceComponents[Index] == nullptr)
		{
			continue;
		}
		const FVector Scale(Box.Extent.X * 2.0 / CubeSize, Box.Extent.Y * 2.0 / CubeSize, Box.Extent.Z * 2.0 / CubeSize);
		const FTransform InstanceTransform(FRotator(Box.PitchDeg, Box.YawDeg, 0.0), ToVector(Box.Center), Scale);
		PieceComponents[Index]->AddInstance(InstanceTransform);
		if (bShowLabels && !Box.Label.empty())
		{
			AddLabel(GoN::ToFString(Box.Label), FVector(Box.Center.X - Box.BoundsExtent().X - 5.0, Box.Center.Y, Box.Top() + 30.0));
		}
	}
	if (bShowLabels)
	{
		for (const GoN::Blockout::FGymLabel& Label : Layout.Labels)
		{
			AddLabel(GoN::ToFString(Label.Text), ToVector(Label.Position));
		}
	}

	PlayerStartLocal = ToVector(Layout.PlayerStart);
	PlayerStartYaw = static_cast<float>(Layout.PlayerYawDeg);
	UE_LOG(LogGoNBlockout, Verbose, TEXT("GoN Blockout: gym rebuilt with %d boxes"), static_cast<int32>(Layout.Boxes.size()));
}

FTransform AGoNGymBuilder::GetPlayerStartTransform() const
{
	const FTransform Local(FRotator(0.0f, PlayerStartYaw, 0.0f), PlayerStartLocal);
	return Local * GetActorTransform();
}

GoN::Blockout::FGymMetrics AGoNGymBuilder::GetPureMetrics() const
{
	return GoNGymBuilderDetail::ToPure(Metrics);
}

FBox AGoNGymBuilder::GetLowCoverBounds() const
{
	const GoN::Blockout::FGymLayout Layout = GoN::Blockout::BuildGymLayout(GetPureMetrics());
	for (const GoN::Blockout::FGymBox& Box : Layout.Boxes)
	{
		if (Box.Piece == GoN::Blockout::EGymPiece::LowCover)
		{
			const FVector Centre = GetActorTransform().TransformPosition(GoNGymBuilderDetail::ToVector(Box.Center));
			const GoN::Blockout::FVec3 Extent = Box.BoundsExtent();
			return FBox::BuildAABB(Centre, FVector(Extent.X, Extent.Y, Extent.Z));
		}
	}
	return FBox(ForceInit);
}
