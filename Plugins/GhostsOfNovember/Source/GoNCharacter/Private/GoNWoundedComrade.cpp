#include "GoNWoundedComrade.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GoNCharacter.h"
#include "GoNCharacterLog.h"
#include "GoNMovementSettings.h"
#include "UObject/ConstructorHelpers.h"

AGoNWoundedComrade::AGoNWoundedComrade()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractVolume"));
	InteractVolume->InitSphereRadius(60.0f);
	InteractVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractVolume->SetCanEverAffectNavigation(false);
	RootComponent = InteractVolume;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyFinder.Succeeded())
	{
		Body->SetSkeletalMeshAsset(MannyFinder.Object);
	}
	Body->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -60.0), FRotator(0.0, -90.0, 0.0));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 60.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(24.0f);
	Label->SetText(FText::FromString(TEXT("Wounded comrade (E)")));
	Label->SetHiddenInGame(true);
}

void AGoNWoundedComrade::BeginPlay()
{
	Super::BeginPlay();
	ApplyPose();
}

void AGoNWoundedComrade::ApplyPose()
{
	UAnimSequence* Pose = GetDefault<UGoNMovementSettings>()->Animation.WoundedPose.LoadSynchronous();
	if (Pose == nullptr || Body->GetSkeletalMeshAsset() == nullptr || Pose->GetSkeleton() != Body->GetSkeletalMeshAsset()->GetSkeleton())
	{
		UE_LOG(LogGoNCharacter, Warning, TEXT("GoN Character: wounded pose clip missing or for another skeleton; %s keeps the reference pose"), *GetName());
		return;
	}
	// Held on the clip's last frame: a limp body, lying or over the shoulders.
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->PlayAnimation(Pose, false);
	Body->SetPosition(FMath::Max(Pose->GetPlayLength() - 0.01f, 0.0f), false);
	Body->SetPlayRate(0.0f);
}

void AGoNWoundedComrade::SetCarried(AGoNCharacter* NewCarrier)
{
	Carrier = NewCarrier;
	if (NewCarrier == nullptr)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		InteractVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}
	InteractVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const UGoNMovementSettings& Settings = NewCarrier->GetMovementSettings();
	AttachToComponent(NewCarrier->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Settings.CarryBone);
	SetActorRelativeTransform(Settings.CarryOffset);
}

void AGoNWoundedComrade::PlaceOnGroundNear(const FVector& Location, float Yaw, const AActor* IgnoredActor)
{
	FVector Ground = Location;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GoNWoundedPlace), false, this);
		if (IgnoredActor != nullptr)
		{
			Params.AddIgnoredActor(IgnoredActor);
		}
		FHitResult Hit;
		// Brief rule 9: start at most 1.5 m above the point (or he lands on a roof or a tree).
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 150.0), Location - FVector(0.0, 0.0, 200.0), ECC_Visibility, Params))
		{
			Ground = Hit.ImpactPoint;
		}
	}
	SetActorLocationAndRotation(Ground + FVector(0.0, 0.0, InteractVolume->GetScaledSphereRadius()), FRotator(0.0, Yaw, 0.0));
}
