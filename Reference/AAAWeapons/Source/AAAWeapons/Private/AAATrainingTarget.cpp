#include "AAATrainingTarget.h"
#include "AAAShapes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AAAATrainingTarget::AAAATrainingTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);
	Hinge->SetRelativeLocation(FVector(0.0, 0.0, 112.0));
}

UStaticMeshComponent* AAAATrainingTarget::Part(USceneComponent* Parent, bool bCylinder, const FVector& Loc, const FRotator& Rot, const FVector& SizeCm, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* C = AAAShapes::Make(this, bCylinder, SizeCm, AAAShapes::Mat(this, Color), bCollide);
	C->SetupAttachment(Parent);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

int32 AAAATrainingTarget::OnHit(const FVector& WorldPoint, const FVector& Dir)
{
	// build lazily (spawned at runtime: components made in the first hit / tick)
	if (DownT >= 0.f) { SwayV += 2.f; return 0; }
	const FVector Center = Hinge->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, PlateRadius + 2.0));
	const FVector N = Hinge->GetForwardVector();
	const FVector P = WorldPoint - N * FVector::DotProduct(WorldPoint - Center, N);
	const float R = (float)FVector::Dist(P, Center) / PlateRadius;
	DownT = 0.f;
	return R < 0.2f ? 10 : R < 0.47f ? 8 : R < 0.73f ? 5 : 2;
}

void AAAATrainingTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Hinge && Hinge->GetNumChildrenComponents() == 0)
	{
		// stand + base
		Part(Root, false, FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator, FVector(40.0, 70.0, 6.0), FLinearColor(0.08f, 0.08f, 0.09f), true);
		Part(Root, false, FVector(-4.0, 0.0, 58.0), FRotator::ZeroRotator, FVector(6.0, 6.0, 110.0), FLinearColor(0.25f, 0.22f, 0.18f), true);
		// plate (faces +X) with rings
		const FRotator Face(90.0, 0.0, 0.0);
		const double Zc = PlateRadius + 2.0;
		Part(Hinge, true, FVector(0.0, 0.0, Zc), Face, FVector(PlateRadius * 2.0, PlateRadius * 2.0, 3.0), FLinearColor(0.92f, 0.92f, 0.9f), true);
		Part(Hinge, true, FVector(1.7, 0.0, Zc), Face, FVector(PlateRadius * 1.46, PlateRadius * 1.46, 0.4), FLinearColor(0.75f, 0.06f, 0.05f), false);
		Part(Hinge, true, FVector(1.9, 0.0, Zc), Face, FVector(PlateRadius * 0.94, PlateRadius * 0.94, 0.4), FLinearColor(0.92f, 0.92f, 0.9f), false);
		Part(Hinge, true, FVector(2.1, 0.0, Zc), Face, FVector(PlateRadius * 0.4, PlateRadius * 0.4, 0.4), FLinearColor(0.75f, 0.06f, 0.05f), false);
		Part(Hinge, false, FVector(-2.5, 0.0, 2.0), FRotator::ZeroRotator, FVector(4.0, 20.0, 6.0), FLinearColor(0.1f, 0.1f, 0.1f), false);
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	float Want = 0.f;
	if (DownT >= 0.f)
	{
		DownT += Dt;
		Want = 84.f;
		if (DownT > ResetSeconds) { DownT = -1.f; Want = 0.f; }
	}
	Angle = FMath::FInterpTo(Angle, Want, Dt, Want > Angle ? 14.f : 5.f);
	// a little wobble after it slams down / stands up
	SwayV += (-Sway * 60.f - SwayV * 6.f) * Dt;
	Sway += SwayV * Dt;
	Hinge->SetRelativeRotation(FRotator(Angle + Sway, 0.0, 0.0));
}
