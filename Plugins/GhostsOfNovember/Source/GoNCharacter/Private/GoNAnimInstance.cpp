#include "GoNAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GoNCharacter.h"
#include "GoNCharacterLog.h"
#include "GoNMovementSettings.h"
#include "Misc/PackageName.h"
#include "TwoBoneIK.h"

namespace GoNAnimDetail
{
	float SeqLength(const UAnimSequence* Seq)
	{
		return Seq ? FMath::Max(static_cast<float>(Seq->GetPlayLength()), 0.01f) : 1.0f;
	}

	// Looping sample (the prototype's Sample).
	void SampleLoop(const UAnimSequence* Seq, float Time, FPoseContext& Out)
	{
		if (Seq == nullptr)
		{
			Out.ResetToRefPose();
			return;
		}
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Fmod(FMath::Max(Time, 0.0f), SeqLength(Seq))), false));
	}

	// One-shot sample held on the last frame.
	void SampleOnce(const UAnimSequence* Seq, float Time, FPoseContext& Out)
	{
		if (Seq == nullptr)
		{
			Out.ResetToRefPose();
			return;
		}
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.0f, SeqLength(Seq) - 0.001f)), false));
	}

	// Dst = lerp(Dst, Src, Alpha) per bone.
	void LerpPose(FCompactPose& Dst, const FCompactPose& Src, float Alpha)
	{
		if (Alpha <= 0.0f)
		{
			return;
		}
		for (const FCompactPoseBoneIndex BoneIndex : Dst.ForEachBoneIndex())
		{
			if (Alpha >= 1.0f)
			{
				Dst[BoneIndex] = Src[BoneIndex];
				continue;
			}
			FTransform Blended = Dst[BoneIndex];
			Blended.BlendWith(Src[BoneIndex], Alpha);
			Blended.NormalizeRotation();
			Dst[BoneIndex] = Blended;
		}
	}

	FCompactPoseBoneIndex Bone(const FBoneContainer& Container, const FName& Name)
	{
		const int32 MeshIndex = Container.GetPoseBoneIndexForBoneName(Name);
		if (MeshIndex == INDEX_NONE)
		{
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		return Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex BoneIndex, const FTransform& Transform, float Alpha = 1.0f)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(BoneIndex, Transform));
		CS.LocalBlendCSBoneTransforms(One, Alpha);
	}

	void RotateCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex BoneIndex, const FQuat& Rotation)
	{
		if (!BoneIndex.IsValid())
		{
			return;
		}
		FTransform Transform = CS.GetComponentSpaceTransform(BoneIndex);
		Transform.SetRotation((Rotation * Transform.GetRotation()).GetNormalized());
		SetCS(CS, BoneIndex, Transform);
	}

	// Two-bone IK in component space; KneeOrElbowDir chooses the bend plane.
	void ChainIK(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FTransform& Target, const FVector& BendDir, float Alpha, bool bKeepEndRotation)
	{
		if (!Upper.IsValid() || !Lower.IsValid() || !End.IsValid() || Alpha <= 0.0f)
		{
			return;
		}
		FTransform U = CS.GetComponentSpaceTransform(Upper);
		FTransform L = CS.GetComponentSpaceTransform(Lower);
		FTransform E = CS.GetComponentSpaceTransform(End);
		FVector Bend = L.GetLocation() - (U.GetLocation() + E.GetLocation()) * 0.5;
		Bend = (Bend * 0.25 + BendDir * 28.0).GetSafeNormal();
		const FVector Pole = (U.GetLocation() + Target.GetLocation()) * 0.5 + Bend * 52.0;
		const FQuat EndRotation = E.GetRotation();
		AnimationCore::SolveTwoBoneIK(U, L, E, Pole, Target.GetLocation(), false, 1.0, 1.0);
		E.SetRotation(bKeepEndRotation ? EndRotation : Target.GetRotation());
		TArray<FBoneTransform> Chain;
		Chain.Add(FBoneTransform(Upper, U));
		Chain.Add(FBoneTransform(Lower, L));
		Chain.Add(FBoneTransform(End, E));
		CS.LocalBlendCSBoneTransforms(Chain, Alpha);
	}

	// Hand IK guarded by IK/FK blending: out of reach, the arm fades back to the clip (FK).
	void GuardedHandIK(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex Hand, const FVector& TargetCS, const FVector& ElbowDir, float Alpha)
	{
		if (!Upper.IsValid() || !Lower.IsValid() || !Hand.IsValid() || Alpha <= 0.0f)
		{
			return;
		}
		const FVector U = CS.GetComponentSpaceTransform(Upper).GetLocation();
		const FVector L = CS.GetComponentSpaceTransform(Lower).GetLocation();
		const FVector H = CS.GetComponentSpaceTransform(Hand).GetLocation();
		const double ChainLength = FVector::Dist(U, L) + FVector::Dist(L, H);
		const float Guard = static_cast<float>(GoN::Character::IKReachAlpha(FVector::Dist(U, TargetCS), ChainLength));
		FTransform Target = CS.GetComponentSpaceTransform(Hand);
		Target.SetLocation(TargetCS);
		ChainIK(CS, Upper, Lower, Hand, Target, ElbowDir, Alpha * Guard, true);
	}

	struct FBones
	{
		FCompactPoseBoneIndex Pelvis = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine1 = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine5 = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Thigh[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex Calf[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex Foot[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex UpperArm[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex LowerArm[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex Hand[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
		FCompactPoseBoneIndex Clavicle[2] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};

		explicit FBones(const FBoneContainer& Container)
		{
			Pelvis = Bone(Container, TEXT("pelvis"));
			Spine1 = Bone(Container, TEXT("spine_01"));
			Spine5 = Bone(Container, TEXT("spine_05"));
			Neck = Bone(Container, TEXT("neck_01"));
			Head = Bone(Container, TEXT("head"));
			const TCHAR* const Sides[2] = {TEXT("_l"), TEXT("_r")};
			for (int32 Side = 0; Side < 2; ++Side)
			{
				Thigh[Side] = Bone(Container, FName(FString(TEXT("thigh")) + Sides[Side]));
				Calf[Side] = Bone(Container, FName(FString(TEXT("calf")) + Sides[Side]));
				Foot[Side] = Bone(Container, FName(FString(TEXT("foot")) + Sides[Side]));
				UpperArm[Side] = Bone(Container, FName(FString(TEXT("upperarm")) + Sides[Side]));
				LowerArm[Side] = Bone(Container, FName(FString(TEXT("lowerarm")) + Sides[Side]));
				Hand[Side] = Bone(Container, FName(FString(TEXT("hand")) + Sides[Side]));
				Clavicle[Side] = Bone(Container, FName(FString(TEXT("clavicle")) + Sides[Side]));
			}
		}

		bool HasLegs() const
		{
			return Pelvis.IsValid() && Thigh[0].IsValid() && Thigh[1].IsValid() && Calf[0].IsValid() && Calf[1].IsValid() && Foot[0].IsValid() && Foot[1].IsValid();
		}
	};
}

// ---- Game thread ----------------------------------------------------------------------------

FAnimInstanceProxy* UGoNAnimInstance::CreateAnimInstanceProxy()
{
	return new FGoNAnimInstanceProxy(this);
}

void UGoNAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FGoNAnimInstanceProxy*>(InProxy);
}

void UGoNAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const AGoNCharacter* Character = Cast<AGoNCharacter>(GetOwningActor());
	const UGoNMovementSettings& Settings = Character ? Character->GetMovementSettings() : *GetDefault<UGoNMovementSettings>();
	const FGoNAnimSettings& Anim = Settings.Animation;
	const USkeleton* Skeleton = CurrentSkeleton;

	LoadedClips.Reset();
	MissingClips.Reset();
	ResolvedAssets = FGoNAnimAssets();
	const auto Load = [this, Skeleton](const TSoftObjectPtr<UAnimSequence>& Slot, const TCHAR* SlotName, bool bRequired) -> const UAnimSequence*
	{
		if (Slot.IsNull())
		{
			if (bRequired)
			{
				MissingClips.Add(FString::Printf(TEXT("%s (no path set)"), SlotName));
			}
			return nullptr;
		}
		if (!bRequired && !FPackageName::DoesPackageExist(Slot.ToSoftObjectPath().GetLongPackageName()))
		{
			// Optional clip not imported yet: procedural fallback, no load error spam.
			MissingClips.Add(FString::Printf(TEXT("%s: %s (optional, not imported; procedural pose)"), SlotName, *Slot.ToString()));
			return nullptr;
		}
		UAnimSequence* Seq = Slot.LoadSynchronous();
		if (Seq == nullptr || (Skeleton != nullptr && Seq->GetSkeleton() != Skeleton))
		{
			MissingClips.Add(FString::Printf(TEXT("%s: %s %s"), SlotName, *Slot.ToString(), Seq == nullptr ? TEXT("not found") : TEXT("is for another skeleton")));
			return nullptr;
		}
		LoadedClips.Add(Seq);
		return Seq;
	};

	ResolvedAssets.Idle = Load(Anim.Idle, TEXT("Idle"), true);
	for (int32 Index = 0; Index < GoN::Character::LocomotionDirections; ++Index)
	{
		const FString Suffix = ANSI_TO_TCHAR(GoN::Character::LocomotionDirectionSuffix(Index));
		ResolvedAssets.Walk[Index] = Anim.Walk.IsValidIndex(Index) ? Load(Anim.Walk[Index], *(TEXT("Walk_") + Suffix), true) : nullptr;
		ResolvedAssets.Jog[Index] = Anim.Jog.IsValidIndex(Index) ? Load(Anim.Jog[Index], *(TEXT("Jog_") + Suffix), true) : nullptr;
	}
	ResolvedAssets.FallLoop = Load(Anim.FallLoop, TEXT("FallLoop"), false);
	ResolvedAssets.ProneIdle = Load(Anim.ProneIdle, TEXT("ProneIdle"), false);
	ResolvedAssets.ProneCrawl = Load(Anim.ProneCrawl, TEXT("ProneCrawl"), false);
	ResolvedAssets.CarryIdle = Load(Anim.CarryIdle, TEXT("CarryIdle"), false);
	ResolvedAssets.CarryWalk = Load(Anim.CarryWalk, TEXT("CarryWalk"), false);
	ResolvedAssets.Vault[0] = Load(Anim.VaultLow, TEXT("VaultLow"), false);
	ResolvedAssets.Vault[1] = Load(Anim.VaultMid, TEXT("VaultMid"), false);
	ResolvedAssets.Vault[2] = Load(Anim.VaultHigh, TEXT("VaultHigh"), false);
	ResolvedAssets.Climb = Load(Anim.Climb, TEXT("Climb"), false);
	ResolvedAssets.CrouchPelvisDrop = Anim.CrouchPelvisDrop;
	ResolvedAssets.WalkJogMaxPlayRate = Anim.WalkJogMaxPlayRate;
	ResolvedAssets.SprintMaxPlayRate = Anim.SprintMaxPlayRate;

	for (const FString& Missing : MissingClips)
	{
		if (Missing.Contains(TEXT("(optional")))
		{
			UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: animation clip %s"), *Missing);
		}
		else
		{
			UE_LOG(LogGoNCharacter, Warning, TEXT("GoN Character: animation clip missing: %s"), *Missing);
		}
	}
	UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: anim clips loaded (%d); procedural fallback for prone %s, carry %s, vault %s, climb %s"),
		LoadedClips.Num(),
		ResolvedAssets.ProneCrawl ? TEXT("no") : TEXT("yes"),
		ResolvedAssets.CarryWalk ? TEXT("no") : TEXT("yes"),
		ResolvedAssets.Vault[1] ? TEXT("no") : TEXT("yes"),
		ResolvedAssets.Climb ? TEXT("no") : TEXT("yes"));
}

void UGoNAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const AGoNCharacter* Character = Cast<AGoNCharacter>(GetOwningActor());
	if (Character == nullptr)
	{
		return;
	}
	FGoNAnimInputs& I = Inputs;
	const FTransform ActorTransform = Character->GetActorTransform();
	const FVector Velocity = Character->GetVelocity();
	I.LocalVelocity = ActorTransform.InverseTransformVectorNoScale(FVector(Velocity.X, Velocity.Y, 0.0));
	I.VelocityZ = static_cast<float>(Velocity.Z);
	I.bFalling = Character->GetCharacterMovement() != nullptr && Character->GetCharacterMovement()->IsFalling();
	I.CrouchTarget = Character->GetStance() == EGoNStance::Crouch ? 1.0f : 0.0f;
	I.ProneTarget = Character->GetStance() == EGoNStance::Prone ? 1.0f : 0.0f;
	I.bCarrying = Character->IsCarrying();
	I.bSprinting = Character->GetGait() == EGoNGait::Sprint;

	I.bTraversing = Character->IsTraversing();
	I.TraversalAlpha = Character->GetTraversalAlpha();
	const GoN::Character::FTraverseDecision Traversal = Character->GetActiveTraversal();
	I.TraversalKind = Traversal.Action == GoN::Character::ETraverseAction::Mantle ? 4 : (Traversal.Action == GoN::Character::ETraverseAction::Vault ? 1 + static_cast<int32>(Traversal.VaultKind) : 0);
	const FGoNTraverseDebug& Probe = Character->GetLastTraverseDebug();
	I.LedgeWorld = Probe.TopPoint.IsZero() ? Probe.FacePoint : FVector(Probe.FacePoint.X, Probe.FacePoint.Y, Probe.TopPoint.Z);
	I.TraverseForwardWorld = ActorTransform.GetUnitAxis(EAxis::X);

	const FRotator Look = Character->GetLookRotation();
	I.bLookAt = Character->GetMovementSettings().Animation.bHeadLookAtCamera && Character->IsPlayerControlled() && !I.bTraversing;
	I.LookYaw = FRotator::NormalizeAxis(Look.Yaw - ActorTransform.Rotator().Yaw);
	I.LookPitch = FRotator::NormalizeAxis(Look.Pitch);

	UpdateGroundTraces();
}

void UGoNAnimInstance::UpdateGroundTraces()
{
	// Prototype values: per foot, from 42 cm above the capsule bottom to 38 cm below.
	FGoNAnimInputs& I = Inputs;
	const AGoNCharacter* Character = Cast<AGoNCharacter>(GetOwningActor());
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	I.bGroundHitL = I.bGroundHitR = false;
	if (World == nullptr || Mesh == nullptr || I.bFalling || I.bTraversing || I.ProneTarget > 0.5f)
	{
		return;
	}
	const double FeetZ = Character->GetFeetLocation().Z;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GoNFootIK), false, Character);
	const auto Trace = [World, &Params, FeetZ](const FVector& FootWorld, bool& bOutHit, float& OutHeight, FVector& OutNormal)
	{
		FHitResult Hit;
		const FVector Start(FootWorld.X, FootWorld.Y, FeetZ + 42.0);
		const FVector End(FootWorld.X, FootWorld.Y, FeetZ - 38.0);
		bOutHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && !Hit.bStartPenetrating && Hit.ImpactNormal.Z > 0.55;
		OutHeight = bOutHit ? static_cast<float>(Hit.ImpactPoint.Z - FeetZ) : 0.0f;
		OutNormal = bOutHit ? Hit.ImpactNormal : FVector::UpVector;
	};
	Trace(Mesh->GetSocketLocation(TEXT("foot_l")), I.bGroundHitL, I.GroundHitL, I.GroundNormalL);
	Trace(Mesh->GetSocketLocation(TEXT("foot_r")), I.bGroundHitR, I.GroundHitR, I.GroundNormalR);
}

// ---- Animation thread -------------------------------------------------------------------------

void FGoNAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UGoNAnimInstance* Instance = CastChecked<UGoNAnimInstance>(InAnimInstance);
	In = Instance->Inputs;
	if (Assets.Idle != Instance->ResolvedAssets.Idle)
	{
		bAnalyzed = false;
	}
	Assets = Instance->ResolvedAssets;
}

void FGoNAnimInstanceProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	using namespace GoN::Character;
	Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	IdleTime += Dt;

	StepCrouchSpring(In.CrouchTarget, Dt, CrouchWeight, CrouchVelocity);
	ProneWeight = FMath::FInterpTo(ProneWeight, In.ProneTarget, Dt, 5.0f);
	CarryWeight = FMath::FInterpTo(CarryWeight, In.bCarrying ? 1.0f : 0.0f, Dt, 6.0f);
	TraverseWeight = In.bTraversing ? FMath::FInterpTo(TraverseWeight, 1.0f, Dt, 20.0f) : FMath::FInterpTo(TraverseWeight, 0.0f, Dt, 10.0f);
	FallWeight = FMath::FInterpTo(FallWeight, (In.bFalling && !In.bTraversing) ? 1.0f : 0.0f, Dt, In.bFalling ? 8.0f : 12.0f);
	FallTime = In.bFalling ? FallTime + Dt : 0.0f;

	// Speed and direction smoothed separately (no dead stops or sector pops).
	const double TargetSpeed = In.LocalVelocity.Size2D();
	SmoothSpeed = InterpTo(SmoothSpeed, TargetSpeed, Dt, TargetSpeed > SmoothSpeed ? 9.5 : 13.5);
	if (TargetSpeed > 10.0)
	{
		const double TargetAngle = DirectionAngle(In.LocalVelocity.X, In.LocalVelocity.Y);
		SmoothAngle = SmoothSpeed < 20.0 ? TargetAngle : FMath::Fmod(static_cast<double>(FMath::FixedTurn(static_cast<float>(SmoothAngle), static_cast<float>(TargetAngle), Dt * 520.0f)) + 360.0, 360.0);
	}
	FGaitRateParams Rates;
	Rates.MaxRate = In.bSprinting ? Assets.SprintMaxPlayRate : Assets.WalkJogMaxPlayRate;
	Gait = ComputeGaitBlend(SmoothSpeed, SmoothAngle, CrouchWeight, WalkSpeeds, JogSpeeds, WalkLengths, JogLengths, Rates);
	if (SmoothSpeed > 3.0)
	{
		Phase = AdvancePhase(Phase, Dt, Gait.PlayRate, Gait.ClipLength);
	}
	CrawlPhase = AdvancePhase(CrawlPhase, Dt, SmoothSpeed / 60.0, 1.0);
	MoveWeight = FMath::FInterpTo(MoveWeight, FMath::Clamp(static_cast<float>((SmoothSpeed - 5.0) / 40.0), 0.0f, 1.0f), Dt, 10.0f);

	// Turn in place drives faster planted-foot steps while the body turns.
	const bool bStill = SmoothSpeed < 22.0 && !In.bFalling && !In.bTraversing;
	TurnInPlace.Update(GetActorTransform().Rotator().Yaw, Dt, bStill);
	Planter.Params.StepSeconds = FMath::Lerp(0.24, 0.18, TurnInPlace.Weight);
	const bool bPlant = bStill && ProneWeight < 0.1f && TraverseWeight < 0.1f;
	const float WantPlant = bPlant ? (1.0f - FMath::Clamp(MoveWeight * 1.6f, 0.0f, 1.0f)) * (1.0f - FallWeight) : 0.0f;
	PlantWeight = FMath::FInterpTo(PlantWeight, WantPlant, Dt, WantPlant > PlantWeight ? 6.0f : 14.0f);

	// Ground IK targets (pure) eased in.
	const FGroundIKTarget Ground = ComputeGroundIK(In.bGroundHitL, In.GroundHitL, In.bGroundHitR, In.GroundHitR, FGroundIKParams());
	GroundOffsetL = FMath::FInterpTo(GroundOffsetL, static_cast<float>(Ground.OffsetL), Dt, 16.0f);
	GroundOffsetR = FMath::FInterpTo(GroundOffsetR, static_cast<float>(Ground.OffsetR), Dt, 16.0f);
	GroundPelvis = FMath::FInterpTo(GroundPelvis, static_cast<float>(Ground.Pelvis), Dt, 14.0f);
	GroundWeight = FMath::FInterpTo(GroundWeight, static_cast<float>(Ground.Weight), Dt, Ground.Weight > 0.0 ? 10.0f : 14.0f);

	const FLookAt Look = ComputeLookAt(In.LookYaw, In.LookPitch, FLookAtLimits());
	LookYaw = FMath::FInterpTo(LookYaw, static_cast<float>(Look.Yaw), Dt, 8.0f);
	LookPitch = FMath::FInterpTo(LookPitch, static_cast<float>(Look.Pitch), Dt, 8.0f);
	LookWeight = FMath::FInterpTo(LookWeight, In.bLookAt ? static_cast<float>(Look.Weight) : 0.0f, Dt, 5.0f);
}

void FGoNAnimInstanceProxy::Analyze(FPoseContext& Output)
{
	// Measure each clip's natural speed from its planted foot (prototype Analyze).
	bAnalyzed = true;
	const FBoneContainer& Container = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex FootL = GoNAnimDetail::Bone(Container, TEXT("foot_l"));
	const FCompactPoseBoneIndex FootR = GoNAnimDetail::Bone(Container, TEXT("foot_r"));
	FPoseContext Sample(Output);
	const auto Measure = [&Sample, FootL, FootR](const UAnimSequence* Seq, double& OutSpeed, double& OutLength)
	{
		OutLength = GoNAnimDetail::SeqLength(Seq);
		if (Seq == nullptr || !FootL.IsValid() || !FootR.IsValid())
		{
			OutSpeed = 150.0;
			return;
		}
		constexpr int32 Count = 32;
		std::vector<GoN::Character::FFootSample> Samples;
		Samples.reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			GoNAnimDetail::SampleLoop(Seq, static_cast<float>(OutLength * Index / Count), Sample);
			FCSPose<FCompactPose> CS;
			CS.InitPose(Sample.Pose);
			const FVector L = CS.GetComponentSpaceTransform(FootL).GetLocation();
			const FVector R = CS.GetComponentSpaceTransform(FootR).GetLocation();
			Samples.push_back(GoN::Character::FFootSample{L.X, L.Y, L.Z, R.X, R.Y, R.Z});
		}
		OutSpeed = GoN::Character::MeasureClipSpeed(Samples, OutLength);
	};
	for (int32 Index = 0; Index < GoN::Character::LocomotionDirections; ++Index)
	{
		Measure(Assets.Walk[Index], WalkSpeeds[Index], WalkLengths[Index]);
		Measure(Assets.Jog[Index], JogSpeeds[Index], JogLengths[Index]);
		JogSpeeds[Index] = FMath::Max(JogSpeeds[Index], WalkSpeeds[Index] * 1.25);
	}
	UE_LOG(LogGoNCharacter, Log, TEXT("GoN Character: measured clip speeds walk F %.0f R %.0f B %.0f L %.0f | jog F %.0f R %.0f B %.0f L %.0f cm/s"),
		WalkSpeeds[0], WalkSpeeds[2], WalkSpeeds[4], WalkSpeeds[6], JogSpeeds[0], JogSpeeds[2], JogSpeeds[4], JogSpeeds[6]);
}

bool FGoNAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	using namespace GoNAnimDetail;
	if (Assets.Idle == nullptr)
	{
		Output.ResetToRefPose();
		return true;
	}
	if (!bAnalyzed)
	{
		Analyze(Output);
	}

	// ---- Locomotion: idle <-> 8-way walk / jog on one synced gait phase.
	SampleLoop(Assets.Idle, IdleTime, Output);
	if (MoveWeight > 0.001f)
	{
		FPoseContext Move(Output);
		FPoseContext Other(Output);
		FPoseContext JogPose(Output);
		const auto DirectionPose = [this, &Other](const UAnimSequence* const* Set, FPoseContext& Result)
		{
			SampleLoop(Set[Gait.Dir0], static_cast<float>(Phase) * SeqLength(Set[Gait.Dir0]), Result);
			SampleLoop(Set[Gait.Dir1], static_cast<float>(Phase) * SeqLength(Set[Gait.Dir1]), Other);
			LerpPose(Result.Pose, Other.Pose, static_cast<float>(Gait.DirAlpha));
		};
		if (Gait.JogWeight < 1.0)
		{
			DirectionPose(Assets.Walk, Move);
		}
		if (Gait.JogWeight > 0.0)
		{
			if (Gait.JogWeight >= 1.0)
			{
				DirectionPose(Assets.Jog, Move);
			}
			else
			{
				DirectionPose(Assets.Jog, JogPose);
				LerpPose(Move.Pose, JogPose.Pose, static_cast<float>(Gait.JogWeight));
			}
		}
		LerpPose(Output.Pose, Move.Pose, MoveWeight);
	}

	// ---- Optional clips for states the project has no animation for yet.
	FPoseContext Extra(Output);
	if (FallWeight > 0.005f && Assets.FallLoop != nullptr)
	{
		SampleLoop(Assets.FallLoop, FallTime, Extra);
		LerpPose(Output.Pose, Extra.Pose, FallWeight);
	}
	if (ProneWeight > 0.005f && (Assets.ProneIdle != nullptr || Assets.ProneCrawl != nullptr))
	{
		const UAnimSequence* Seq = (MoveWeight > 0.5f && Assets.ProneCrawl != nullptr) ? Assets.ProneCrawl : (Assets.ProneIdle ? Assets.ProneIdle : Assets.ProneCrawl);
		SampleLoop(Seq, static_cast<float>(CrawlPhase) * SeqLength(Seq), Extra);
		LerpPose(Output.Pose, Extra.Pose, ProneWeight);
	}
	if (CarryWeight > 0.005f && (Assets.CarryIdle != nullptr || Assets.CarryWalk != nullptr))
	{
		const UAnimSequence* Seq = (MoveWeight > 0.5f && Assets.CarryWalk != nullptr) ? Assets.CarryWalk : (Assets.CarryIdle ? Assets.CarryIdle : Assets.CarryWalk);
		SampleLoop(Seq, Seq == Assets.CarryWalk ? static_cast<float>(Phase) * SeqLength(Seq) : IdleTime, Extra);
		LerpPose(Output.Pose, Extra.Pose, CarryWeight);
	}
	const UAnimSequence* TraverseClip = In.TraversalKind == 4 ? Assets.Climb : (In.TraversalKind >= 1 && In.TraversalKind <= 3 ? Assets.Vault[In.TraversalKind - 1] : nullptr);
	if (TraverseWeight > 0.005f && TraverseClip != nullptr)
	{
		SampleOnce(TraverseClip, In.TraversalAlpha * SeqLength(TraverseClip), Extra);
		LerpPose(Output.Pose, Extra.Pose, TraverseWeight);
	}

	ApplyBodyModifiers(Output);
	return true;
}

void FGoNAnimInstanceProxy::ApplyBodyModifiers(FPoseContext& Output)
{
	using namespace GoNAnimDetail;
	const FBoneContainer& Container = Output.Pose.GetBoneContainer();
	const FBones B(Container);
	if (!B.HasLegs())
	{
		return;
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);

	// Component-space axes from the actor (the mesh is rotated -90 in yaw).
	const FTransform& CompT = GetComponentTransform();
	const FTransform& ActorT = GetActorTransform();
	const FVector UpCS = CompT.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	const FVector FwdCS = CompT.InverseTransformVectorNoScale(ActorT.GetUnitAxis(EAxis::X)).GetSafeNormal();
	const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();

	const bool bHasProneClip = Assets.ProneIdle != nullptr || Assets.ProneCrawl != nullptr;
	const bool bHasCarryClip = Assets.CarryIdle != nullptr || Assets.CarryWalk != nullptr;
	const bool bHasTraverseClip = (In.TraversalKind == 4 && Assets.Climb != nullptr) || (In.TraversalKind >= 1 && In.TraversalKind <= 3 && Assets.Vault[In.TraversalKind - 1] != nullptr);

	// ---- Procedural prone (until a prone clip is imported): the body pivots forward at the
	//      hips and lies just above the ground.
	if (ProneWeight > 0.005f && !bHasProneClip)
	{
		const double GroundZ = FMath::Min(FVector::DotProduct(CS.GetComponentSpaceTransform(B.Foot[0]).GetLocation(), UpCS), FVector::DotProduct(CS.GetComponentSpaceTransform(B.Foot[1]).GetLocation(), UpCS)) - 8.0;
		FTransform Pelvis = CS.GetComponentSpaceTransform(B.Pelvis);
		const double PelvisZ = FVector::DotProduct(Pelvis.GetLocation(), UpCS);
		const double Crawl = FMath::Sin(CrawlPhase * 2.0 * UE_DOUBLE_PI) * 4.0 * FMath::Clamp(MoveWeight, 0.0f, 1.0f);
		Pelvis.SetRotation((FQuat(RightCS, FMath::DegreesToRadians(84.0 * ProneWeight)) * FQuat(UpCS, FMath::DegreesToRadians(Crawl)) * Pelvis.GetRotation()).GetNormalized());
		Pelvis.AddToTranslation(UpCS * ((GroundZ + 18.0 - PelvisZ) * ProneWeight));
		SetCS(CS, B.Pelvis, Pelvis);
		// Head up to look ahead while lying.
		RotateCS(CS, B.Neck, FQuat(RightCS, FMath::DegreesToRadians(-50.0 * ProneWeight)));
	}

	// ---- Legs: one pass combining crouch drop, planted feet, ground IK and the vault tuck.
	const float Prone = ProneWeight;
	const float LegAlpha = 1.0f - FMath::Clamp(Prone * 1.5f, 0.0f, 1.0f);
	if (LegAlpha > 0.001f)
	{
		FTransform FootTarget[2] = {CS.GetComponentSpaceTransform(B.Foot[0]), CS.GetComponentSpaceTransform(B.Foot[1])};
		FTransform Pelvis = CS.GetComponentSpaceTransform(B.Pelvis);
		bool bModified = false;

		// Planted feet: each foot stays at its world position and yaw, stepping when needed.
		if (PlantWeight > 0.01f)
		{
			double HomeX[2];
			double HomeY[2];
			double HomeYaw[2];
			FTransform HomeWorld[2];
			for (int32 Side = 0; Side < 2; ++Side)
			{
				HomeWorld[Side] = FootTarget[Side] * CompT;
				HomeX[Side] = HomeWorld[Side].GetLocation().X;
				HomeY[Side] = HomeWorld[Side].GetLocation().Y;
				HomeYaw[Side] = HomeWorld[Side].Rotator().Yaw;
			}
			GoN::Character::FFootPlanter::FTarget Planted[2];
			Planter.Update(HomeX, HomeY, HomeYaw, Dt, Planted);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				FTransform World = HomeWorld[Side];
				const FRotator HomeRotator = HomeWorld[Side].Rotator();
				World.SetLocation(FVector(Planted[Side].X, Planted[Side].Y, HomeWorld[Side].GetLocation().Z + Planted[Side].Lift));
				World.SetRotation((FQuat(FVector::UpVector, FMath::DegreesToRadians(FRotator::NormalizeAxis(static_cast<float>(Planted[Side].Yaw) - HomeRotator.Yaw))) * HomeWorld[Side].GetRotation()).GetNormalized());
				FTransform Blended = FootTarget[Side];
				Blended.BlendWith(World.GetRelativeTransform(CompT), PlantWeight);
				FootTarget[Side] = Blended;
			}
			bModified = true;
		}
		else
		{
			Planter.Reset();
		}

		// Ground IK on steps and slopes: feet to the surface under them, hips share the drop.
		if (GroundWeight > 0.01f)
		{
			const float Offsets[2] = {GroundOffsetL, GroundOffsetR};
			const FVector Normals[2] = {In.GroundNormalL, In.GroundNormalR};
			for (int32 Side = 0; Side < 2; ++Side)
			{
				FootTarget[Side].AddToTranslation(UpCS * static_cast<double>(Offsets[Side] * GroundWeight));
				FVector NormalCS = CompT.InverseTransformVectorNoScale(Normals[Side]).GetSafeNormal();
				if (FVector::DotProduct(NormalCS, UpCS) < 0.55)
				{
					NormalCS = (NormalCS + UpCS * 1.5).GetSafeNormal();
				}
				const FQuat Slope = FQuat::FindBetweenNormals(UpCS, NormalCS);
				FootTarget[Side].SetRotation((FQuat::Slerp(FQuat::Identity, Slope, GroundWeight * 0.8f) * FootTarget[Side].GetRotation()).GetNormalized());
			}
			Pelvis.AddToTranslation(UpCS * static_cast<double>(GroundPelvis * GroundWeight));
			bModified = true;
		}

		// Crouch: the same walk, only lower. Hips drop, feet stay where the clip put them.
		const float Crouch = FMath::Clamp(static_cast<float>(CrouchWeight), 0.0f, 1.08f) * LegAlpha;
		if (Crouch > 0.005f)
		{
			Pelvis.AddToTranslation(-UpCS * static_cast<double>(Assets.CrouchPelvisDrop * Crouch));
			bModified = true;
		}

		// Vault / climb without a clip: knees tucked up over the obstacle.
		if (TraverseWeight > 0.01f && !bHasTraverseClip)
		{
			const double Arc = FMath::Sin(In.TraversalAlpha * UE_DOUBLE_PI) * TraverseWeight;
			const double Tuck = In.TraversalKind == 4 ? 30.0 : 45.0;
			FootTarget[0].AddToTranslation(UpCS * (Tuck * Arc) + FwdCS * (12.0 * Arc));
			FootTarget[1].AddToTranslation(UpCS * (Tuck * 0.8 * Arc) + FwdCS * (6.0 * Arc));
			bModified = true;
		}

		if (bModified)
		{
			SetCS(CS, B.Pelvis, Pelvis);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector Out = Side == 0 ? -RightCS : RightCS;
				const FVector KneeDir = (FwdCS * 0.92 + Out * 0.12 + UpCS * 0.15).GetSafeNormal();
				ChainIK(CS, B.Thigh[Side], B.Calf[Side], B.Foot[Side], FootTarget[Side], KneeDir, LegAlpha, false);
			}
		}
	}

	// ---- Vault / climb without a clip: lean in, hands on the ledge (IK/FK guarded).
	if (TraverseWeight > 0.01f && !bHasTraverseClip)
	{
		const double Arc = FMath::Sin(In.TraversalAlpha * UE_DOUBLE_PI);
		RotateCS(CS, B.Spine1, FQuat(RightCS, FMath::DegreesToRadians(22.0 * Arc * TraverseWeight)));
		if (!In.LedgeWorld.IsZero())
		{
			const FVector LedgeCS = CompT.InverseTransformPosition(In.LedgeWorld);
			const float HandAlpha = TraverseWeight * (1.0f - FMath::SmoothStep(0.55f, 0.85f, In.TraversalAlpha));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector Target = LedgeCS + (Side == 0 ? -RightCS : RightCS) * 22.0 + UpCS * 3.0;
				GuardedHandIK(CS, B.UpperArm[Side], B.LowerArm[Side], B.Hand[Side], Target, -UpCS * 0.5 + (Side == 0 ? -RightCS : RightCS), HandAlpha);
			}
		}
	}

	// ---- Carrying without a clip: lean forward, hands hold the comrade on the shoulders.
	if (CarryWeight > 0.01f && !bHasCarryClip)
	{
		RotateCS(CS, B.Spine1, FQuat(RightCS, FMath::DegreesToRadians(9.0 * CarryWeight)));
		if (B.Clavicle[0].IsValid() && B.Clavicle[1].IsValid() && B.Spine5.IsValid())
		{
			const FVector Chest = CS.GetComponentSpaceTransform(B.Spine5).GetLocation();
			const FVector RightTarget = CS.GetComponentSpaceTransform(B.Clavicle[1]).GetLocation() + FwdCS * 22.0 + UpCS * 8.0;
			const FVector LeftTarget = Chest + FwdCS * 26.0 - RightCS * 12.0 - UpCS * 6.0;
			GuardedHandIK(CS, B.UpperArm[1], B.LowerArm[1], B.Hand[1], RightTarget, RightCS - UpCS, CarryWeight);
			GuardedHandIK(CS, B.UpperArm[0], B.LowerArm[0], B.Hand[0], LeftTarget, -RightCS - UpCS, CarryWeight);
		}
	}

	// ---- Head look-at (camera direction), split over neck and head.
	if (LookWeight > 0.01f && Prone < 0.5f)
	{
		const FQuat Yaw(UpCS, FMath::DegreesToRadians(LookYaw * LookWeight));
		const FQuat Pitch(RightCS, FMath::DegreesToRadians(-LookPitch * LookWeight));
		const FQuat Full = Yaw * Pitch;
		RotateCS(CS, B.Neck, FQuat::Slerp(FQuat::Identity, Full, 0.4f));
		RotateCS(CS, B.Head, FQuat::Slerp(FQuat::Identity, Full, 0.6f));
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}
