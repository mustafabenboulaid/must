#include "AAARifleAnimInstance.h"
#include "AAAClipBracedHang.h"
#include "AAAClipThrow.h"
#include "AAAClipSniperFire.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Misc/Paths.h"

namespace
{
	// crouch = the same walk / idle motion, only lower: hip drop (cm at full crouch) and a small forward hip tip (deg)
	constexpr float GCrouchDrop = 19.f;   // (reference: user's AI image - knees clearly bent, torso upright)
	constexpr float GCrouchTip = 0.f;     // (no hip tip: it bent the back forward)
	constexpr float GSneakLean = 0.f;     // unarmed: torso lean (deg) - 0 = upright back and head, as in the user's reference
	constexpr float GCrouchStride = 1.f;   // (same stride as the walk: nothing is rescaled)
	const TCHAR* const RIFLE_DIR = TEXT("/Game/Characters/Mannequins/Anims/Rifle/");
	const TCHAR* const PISTOL_DIR = TEXT("/Game/Characters/Mannequins/Anims/Pistol/");
	const TCHAR* const UNARMED_DIR = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/");
	// 8 directions, 45 degrees apart, clockwise from forward (angle = atan2(right, forward))
	const TCHAR* const DIRS[8] = { TEXT("Fwd"), TEXT("Fwd_Right"), TEXT("Right"), TEXT("Bwd_Right"), TEXT("Bwd"), TEXT("Bwd_Left"), TEXT("Left"), TEXT("Fwd_Left") };

	UAnimSequence* LoadSeq(const FString& Path)
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		return LoadObject<UAnimSequence>(nullptr, *(Path + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	struct FUnarmedAnimSet
	{
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Walk[8] = {};
		UAnimSequence* Jog[8] = {};
		UAnimSequence* Fall = nullptr;
		UAnimSequence* JumpStart = nullptr;
		UAnimSequence* Land = nullptr;
		bool bLoaded = false;
	};
	static FUnarmedAnimSet GUnarmed;

	void EnsureUnarmedLoaded()
	{
		if (GUnarmed.bLoaded) return;
		GUnarmed.bLoaded = true;
		const FString D(UNARMED_DIR);
		GUnarmed.Idle = LoadSeq(D + TEXT("MM_Idle"));
		GUnarmed.Fall = LoadSeq(D + TEXT("Jump/MM_Fall_Loop"));
		GUnarmed.JumpStart = LoadSeq(D + TEXT("Jump/MM_Jump"));
		GUnarmed.Land = LoadSeq(D + TEXT("Jump/MM_Land"));
		for (int32 K = 0; K < 8; ++K)
		{
			GUnarmed.Walk[K] = LoadSeq(D + TEXT("Walk/MF_Unarmed_Walk_") + DIRS[K]);
			GUnarmed.Jog[K] = LoadSeq(D + TEXT("Jog/MF_Unarmed_Jog_") + DIRS[K]);
		}
		if (GUnarmed.Idle) GUnarmed.Idle->AddToRoot();
		if (GUnarmed.Fall) GUnarmed.Fall->AddToRoot();
		if (GUnarmed.JumpStart) GUnarmed.JumpStart->AddToRoot();
		if (GUnarmed.Land) GUnarmed.Land->AddToRoot();
		for (int32 K = 0; K < 8; ++K)
		{
			if (GUnarmed.Walk[K]) GUnarmed.Walk[K]->AddToRoot();
			if (GUnarmed.Jog[K]) GUnarmed.Jog[K]->AddToRoot();
		}
	}

	struct FPistolAnimSet
	{
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Walk[8] = {};
		UAnimSequence* Jog[8] = {};
		UAnimSequence* Fall = nullptr;
		UAnimSequence* Reload = nullptr;
		UAnimSequence* Equip = nullptr;
		UAnimSequence* JumpStart = nullptr;
		UAnimSequence* Land = nullptr;
		bool bLoaded = false;
	};
	static FPistolAnimSet GPistol;

	void EnsurePistolLoaded()
	{
		if (GPistol.bLoaded) return;
		GPistol.bLoaded = true;
		const FString D(PISTOL_DIR);
		GPistol.Idle = LoadSeq(D + TEXT("MF_Pistol_Idle_ADS"));
		GPistol.Fall = LoadSeq(D + TEXT("Jump/MM_Pistol_Jump_Fall_Loop"));
		GPistol.Reload = LoadSeq(D + TEXT("MM_Pistol_Reload"));
		GPistol.Equip = LoadSeq(D + TEXT("MM_Pistol_Equip"));
		GPistol.JumpStart = LoadSeq(D + TEXT("Jump/MM_Pistol_Jump_Start"));
		GPistol.Land = LoadSeq(D + TEXT("Jump/MM_Pistol_Jump_RecoveryAdditive"));
		for (int32 K = 0; K < 8; ++K)
		{
			GPistol.Walk[K] = LoadSeq(D + TEXT("Walk/MF_Pistol_Walk_") + DIRS[K]);
			GPistol.Jog[K] = LoadSeq(D + TEXT("Jog/MF_Pistol_Jog_") + DIRS[K]);
		}
		if (GPistol.Idle) GPistol.Idle->AddToRoot();
		if (GPistol.Fall) GPistol.Fall->AddToRoot();
		if (GPistol.Reload) GPistol.Reload->AddToRoot();
		if (GPistol.Equip) GPistol.Equip->AddToRoot();
		if (GPistol.JumpStart) GPistol.JumpStart->AddToRoot();
		if (GPistol.Land) GPistol.Land->AddToRoot();
		for (int32 K = 0; K < 8; ++K)
		{
			if (GPistol.Walk[K]) GPistol.Walk[K]->AddToRoot();
			if (GPistol.Jog[K]) GPistol.Jog[K]->AddToRoot();
		}
	}

	// clips the base sets above do not cover (shared by every character using this anim instance, loaded once)
	struct FExtraAnimSet
	{
		UAnimSequence* HitLgt[4] = {};    // MM_HitReact_Front_Lgt_01..04
		UAnimSequence* HitMed[2] = {};    // MM_HitReact_Front_Med_01..02
		UAnimSequence* HitHvy = nullptr;  // MM_HitReact_Front_Hvy_01
		UAnimSequence* HitBack = nullptr; // MM_HitReact_Back_Med_01
		UAnimSequence* Death[6] = {};     // Back_01, Front_01, Front_02, Front_03, Left_01, Right_01
		UAnimSequence* RifleRise = nullptr, *RifleApex = nullptr, *RifleLandAdd = nullptr;
		bool bLoaded = false;
	};
	static FExtraAnimSet GExtra;

	void EnsureExtraLoaded()
	{
		if (GExtra.bLoaded) return;
		GExtra.bLoaded = true;
		const FString R(RIFLE_DIR);
		auto Keep = [](UAnimSequence* S) { if (S) S->AddToRoot(); return S; };
		for (int32 K = 0; K < 4; ++K) GExtra.HitLgt[K] = Keep(LoadSeq(R + FString::Printf(TEXT("HitReact/MM_HitReact_Front_Lgt_0%d"), K + 1)));
		for (int32 K = 0; K < 2; ++K) GExtra.HitMed[K] = Keep(LoadSeq(R + FString::Printf(TEXT("HitReact/MM_HitReact_Front_Med_0%d"), K + 1)));
		GExtra.HitHvy = Keep(LoadSeq(R + TEXT("HitReact/MM_HitReact_Front_Hvy_01")));
		GExtra.HitBack = Keep(LoadSeq(R + TEXT("HitReact/MM_HitReact_Back_Med_01")));
		const TCHAR* const DeathNames[6] = { TEXT("Back_01"), TEXT("Front_01"), TEXT("Front_02"), TEXT("Front_03"), TEXT("Left_01"), TEXT("Right_01") };
		for (int32 K = 0; K < 6; ++K) GExtra.Death[K] = Keep(LoadSeq(FString(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_")) + DeathNames[K]));
		GExtra.RifleRise = Keep(LoadSeq(R + TEXT("Jump/MM_Rifle_Jump_Start_Loop")));
		GExtra.RifleApex = Keep(LoadSeq(R + TEXT("Jump/MM_Rifle_Jump_Apex")));
		GExtra.RifleLandAdd = Keep(LoadSeq(R + TEXT("Jump/MM_Rifle_Jump_RecoveryAdditive")));
		if (GExtra.RifleLandAdd && !GExtra.RifleLandAdd->IsValidAdditive()) GExtra.RifleLandAdd = nullptr;
	}

	/** the flinch for a hit: from behind -> back reaction, hard hits -> heavy / medium, else one of the light ones */
	const UAnimSequence* PickHit(float HitDir, float Strength, int32 Variant)
	{
		EnsureExtraLoaded();
		const int32 V = FMath::Abs(Variant);
		if (FMath::Abs(FRotator::NormalizeAxis(HitDir)) > 115.f && GExtra.HitBack) return GExtra.HitBack;
		if (Strength > 0.75f && GExtra.HitHvy) return GExtra.HitHvy;
		if (Strength > 0.4f && GExtra.HitMed[V % 2]) return GExtra.HitMed[V % 2];
		return GExtra.HitLgt[V % 4];
	}

	float SeqLen(const UAnimSequence* Seq) { return Seq ? FMath::Max((float)Seq->GetPlayLength(), 0.01f) : 1.f; }

	/** sample a one-shot clip (held on its last frame, no wrap-around) */
	void SampleOnce(const UAnimSequence* Seq, float Time, FPoseContext& Out)
	{
		if (!Seq) { Out.ResetToRefPose(); return; }
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext((double)FMath::Clamp(Time, 0.f, SeqLen(Seq) - 0.001f), false));
	}

	void Sample(const UAnimSequence* Seq, float Time, FPoseContext& Out)
	{
		if (!Seq) { Out.ResetToRefPose(); return; }
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext((double)FMath::Fmod(FMath::Max(Time, 0.f), SeqLen(Seq)), false));
	}

	/** Dst = lerp(Dst, Src, Alpha) per bone (Mask: per compact bone weight, optional) */
	void LerpPose(FCompactPose& Dst, const FCompactPose& Src, float Alpha, const TArray<float>* Mask = nullptr)
	{
		if (Alpha <= 0.f) return;
		for (const FCompactPoseBoneIndex B : Dst.ForEachBoneIndex())
		{
			const float A = Mask ? Alpha * (*Mask)[B.GetInt()] : Alpha;
			if (A <= 0.f) continue;
			if (A >= 1.f) { Dst[B] = Src[B]; continue; }
			FTransform T = Dst[B];
			T.BlendWith(Src[B], A);
			T.NormalizeRotation();
			Dst[B] = T;
		}
	}

	FCompactPoseBoneIndex Bone(const FBoneContainer& BC, const FName& Name)
	{
		const int32 MeshIdx = BC.GetPoseBoneIndexForBoneName(Name);
		if (MeshIdx == INDEX_NONE) return FCompactPoseBoneIndex(INDEX_NONE);
		return BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIdx));
	}

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex B, const FTransform& T, float Alpha = 1.f)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(B, T));
		CS.LocalBlendCSBoneTransforms(One, Alpha);
	}

	/** two-bone IK in component space: Upper / Lower / End to the end transform Target */
	void ArmIK(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Up, FCompactPoseBoneIndex Lo, FCompactPoseBoneIndex End, const FTransform& Target, float Alpha, bool bIsLeftArm = false)
	{
		if (!Up.IsValid() || !Lo.IsValid() || !End.IsValid() || Alpha <= 0.f) return;
		FTransform U = CS.GetComponentSpaceTransform(Up);
		FTransform L = CS.GetComponentSpaceTransform(Lo);
		FTransform E = CS.GetComponentSpaceTransform(End);
		FVector BendDir = L.GetLocation() - (U.GetLocation() + E.GetLocation()) * 0.5;
		if (BendDir.SizeSquared() < 9.0)
		{
			const FVector ArmAxis = (Target.GetLocation() - U.GetLocation()).GetSafeNormal();
			const FVector FallbackDown(0.0, 0.0, -1.0);
			const FVector SideDir = bIsLeftArm ? FVector(0.0, -1.0, 0.0) : FVector(0.0, 1.0, 0.0);
			BendDir = (FallbackDown * 0.65f + SideDir * 0.35f - ArmAxis * FVector::DotProduct(FallbackDown, ArmAxis)).GetSafeNormal() * 15.0;
		}
		const FVector Pole = L.GetLocation() + BendDir.GetSafeNormal() * 35.0;
		AnimationCore::SolveTwoBoneIK(U, L, E, Pole, Target.GetLocation(), false, 1.0, 1.0);
		E.SetRotation(Target.GetRotation());
		TArray<FBoneTransform> Bt;
		Bt.Add(FBoneTransform(Up, U));
		Bt.Add(FBoneTransform(Lo, L));
		Bt.Add(FBoneTransform(End, E));
		// (already in ascending bone order: parent before child)
		CS.LocalBlendCSBoneTransforms(Bt, Alpha);
	}

	/** Two-bone IK reach guard: how far a target must move (toward the shoulder) so the Upper / Lower / End chain can
	 *  really reach it. Without it SolveTwoBoneIK stops short (no stretching) and the hand comes OFF the weapon. */
	FVector ReachShift(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Up, FCompactPoseBoneIndex Lo, FCompactPoseBoneIndex End, const FVector& Target, double Slack = 0.985)
	{
		if (!Up.IsValid() || !Lo.IsValid() || !End.IsValid()) return FVector::ZeroVector;
		const FVector U = CS.GetComponentSpaceTransform(Up).GetLocation();
		const FVector L = CS.GetComponentSpaceTransform(Lo).GetLocation();
		const FVector E = CS.GetComponentSpaceTransform(End).GetLocation();
		const double Reach = (FVector::Dist(U, L) + FVector::Dist(L, E)) * Slack;
		const FVector D = Target - U;
		const double Dist = D.Size();
		if (Dist <= Reach || Dist < 1.0) return FVector::ZeroVector;
		return -D / Dist * (Dist - Reach);
	}

	/** FK after IK: the weapon frame placed from where the left hand REALLY is (hand_l = LeftRel * W  =>  W = LeftRel^-1 * hand_l) */
	FTransform WeaponFromLeftHand(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex HandL, const FTransform& LeftRel)
	{
		FTransform W = LeftRel.Inverse() * CS.GetComponentSpaceTransform(HandL);
		W.NormalizeRotation();
		return W;
	}

	/** two-bone leg IK in component space: Thigh / Calf / Foot to Target, preserving natural forward knee bend */
	void LegIK(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Thigh, FCompactPoseBoneIndex Calf, FCompactPoseBoneIndex Foot, const FTransform& Target, const FVector& KneeForwardDir, float Alpha)
	{
		if (!Thigh.IsValid() || !Calf.IsValid() || !Foot.IsValid() || Alpha <= 0.f) return;
		FTransform U = CS.GetComponentSpaceTransform(Thigh);
		FTransform L = CS.GetComponentSpaceTransform(Calf);
		FTransform E = CS.GetComponentSpaceTransform(Foot);
		FVector BendDir = L.GetLocation() - (U.GetLocation() + E.GetLocation()) * 0.5;
		BendDir = (BendDir * 0.25 + KneeForwardDir * 28.0).GetSafeNormal();
		const FVector Mid = (U.GetLocation() + Target.GetLocation()) * 0.5;
		const FVector Pole = Mid + BendDir * 52.0;
		AnimationCore::SolveTwoBoneIK(U, L, E, Pole, Target.GetLocation(), false, 1.0, 1.0);
		E.SetRotation(Target.GetRotation());
		TArray<FBoneTransform> Bt;
		Bt.Add(FBoneTransform(Thigh, U));
		Bt.Add(FBoneTransform(Calf, L));
		Bt.Add(FBoneTransform(Foot, E));
		CS.LocalBlendCSBoneTransforms(Bt, Alpha);
	}

	FVector Palm(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Hand, FCompactPoseBoneIndex Finger)
	{
		const FVector H = CS.GetComponentSpaceTransform(Hand).GetLocation();
		if (!Finger.IsValid()) return H;
		return FMath::Lerp(H, CS.GetComponentSpaceTransform(Finger).GetLocation(), 0.55);
	}
}

/* ============================================================================================ instance */

void UAAARifleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const FString D(RIFLE_DIR);
	Idle = LoadSeq(D + TEXT("MF_Rifle_Idle_ADS"));
	Fall = LoadSeq(D + TEXT("Jump/MM_Rifle_Jump_Fall_Loop"));
	Reload = LoadSeq(D + TEXT("MM_Rifle_Reload"));
	Equip = LoadSeq(D + TEXT("MM_Rifle_Equip"));
	JumpStart = LoadSeq(D + TEXT("Jump/MM_Rifle_Jump_Start"));
	Land = LoadSeq(D + TEXT("Jump/MM_Rifle_Jump_Fall_Land"));
	HitReacts.Reset();
	for (const TCHAR* H : { TEXT("MM_HitReact_Front_Lgt_01"), TEXT("MM_HitReact_Front_Lgt_02"), TEXT("MM_HitReact_Front_Med_01"), TEXT("MM_HitReact_Front_Lgt_03") })
		if (UAnimSequence* S = LoadSeq(D + TEXT("HitReact/") + H)) HitReacts.Add(S);
	Walk.SetNum(8); Jog.SetNum(8);
	for (int32 K = 0; K < 8; ++K)
	{
		Walk[K] = LoadSeq(D + TEXT("Walk/MF_Rifle_Walk_") + DIRS[K]);
		Jog[K] = LoadSeq(D + TEXT("Jog/MF_Rifle_Jog_") + DIRS[K]);
	}
	// the set only works on the skeleton it was made for (SK_Mannequin: Manny / Quinn)
	const USkeleton* Sk = CurrentSkeleton;
	auto Fits = [Sk](const UAnimSequence* S) { return S && (!Sk || S->GetSkeleton() == Sk); };
	bAssetsOk = Fits(Idle);
	for (int32 K = 0; K < 8; ++K) bAssetsOk = bAssetsOk && Fits(Walk[K]) && Fits(Jog[K]);
	if (!Fits(Fall)) Fall = nullptr;
	if (!Fits(Reload)) Reload = nullptr;
	if (!Fits(Equip)) Equip = nullptr;
	if (!Fits(JumpStart)) JumpStart = nullptr;
	if (!Fits(Land)) Land = nullptr;
	HitReacts.RemoveAll([&Fits](const TObjectPtr<UAnimSequence>& S) { return !Fits(S.Get()); });
	if (!bAssetsOk) UE_LOG(LogTemp, Warning, TEXT("AAA Weapons: the rifle animations (%s) are missing or made for another skeleton"), RIFLE_DIR);
}

void UAAARifleAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	const FAAARifleProxy& P = GetProxyOnGameThread<FAAARifleProxy>();
	bCalibrated = P.bCalibrated;
	GripRel = P.bBoltOut ? P.GripRelOut : P.GripRel;   // during the Kar98k bolt cycle the rifle stays on the left hand while the right hand works the bolt
	LeftDist = P.LeftDist;
}

float UAAARifleAnimInstance::GetReloadLength() const
{
	if (Inputs.bIsPistol)
	{
		EnsurePistolLoaded();
		return GPistol.Reload ? FMath::Max((float)GPistol.Reload->GetPlayLength(), 0.5f) : 1.8f;
	}
	return Reload ? FMath::Max((float)Reload->GetPlayLength(), 0.5f) : 2.2f;
}

float UAAARifleAnimInstance::GetEquipLength() const
{
	if (Inputs.bIsPistol)
	{
		EnsurePistolLoaded();
		return GPistol.Equip ? FMath::Clamp((float)GPistol.Equip->GetPlayLength(), 0.2f, 2.f) : 0.35f;
	}
	return Equip ? FMath::Clamp((float)Equip->GetPlayLength(), 0.2f, 2.f) : 0.35f;
}

int32 UAAARifleAnimInstance::PickDeath(float HitDir)
{
	EnsureExtraLoaded();
	const float A = FRotator::NormalizeAxis(HitDir);
	int32 K;
	if (FMath::Abs(A) <= 50.f) K = 1 + FMath::RandRange(0, 2);   // shot from the front: one of the three front deaths
	else if (FMath::Abs(A) >= 130.f) K = 0;                      // from behind
	else K = A > 0.f ? 5 : 4;                                     // from the right / left
	if (!GExtra.Death[K]) { for (K = 0; K < 6 && !GExtra.Death[K]; ++K) {} }
	return K < 6 ? K : INDEX_NONE;
}

float UAAARifleAnimInstance::GetDeathLength(int32 Index)
{
	EnsureExtraLoaded();
	return (Index >= 0 && Index < 6 && GExtra.Death[Index]) ? (float)GExtra.Death[Index]->GetPlayLength() : 0.f;
}

float UAAARifleAnimInstance::DirFrom(const AActor* Self, const FVector& From)
{
	if (!Self) return 0.f;
	const FVector D = (From - Self->GetActorLocation()).GetSafeNormal2D();
	const FVector F = Self->GetActorForwardVector().GetSafeNormal2D(), R = Self->GetActorRightVector().GetSafeNormal2D();
	return FMath::RadiansToDegrees(FMath::Atan2((float)FVector::DotProduct(D, R), (float)FVector::DotProduct(D, F)));
}

FAnimInstanceProxy* UAAARifleAnimInstance::CreateAnimInstanceProxy() { return new FAAARifleProxy(this); }

void UAAARifleAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete static_cast<FAAARifleProxy*>(InProxy); }

/* ============================================================================================ proxy */

void FAAARifleProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UAAARifleAnimInstance* A = CastChecked<UAAARifleAnimInstance>(InAnimInstance);
	In = A->Inputs;
	if (In.bIsPistol != bActivePistol || In.bUnarmed != bActiveUnarmed)
	{
		bActivePistol = In.bIsPistol;
		bActiveUnarmed = In.bUnarmed;
		if (!In.bUnarmed) bCalibrated = false;
		bAnalyzed = false;
		WeaponSwapBlend = 1.0f;
		SwapFrom = LastPose;   // crossfade from the pose of the old set
	}
	EnsureExtraLoaded();
	AirRise = nullptr; AirApex = nullptr; LandAdd = nullptr;
	if (In.bUnarmed)
	{
		EnsureUnarmedLoaded();
		Idle = GUnarmed.Idle;
		Fall = GUnarmed.Fall;
		Reload = nullptr;
		Equip = nullptr;
		JumpStart = GUnarmed.JumpStart;
		Land = GUnarmed.Land;
		for (int32 K = 0; K < 8; ++K)
		{
			Walk[K] = GUnarmed.Walk[K];
			Jog[K] = GUnarmed.Jog[K];
		}
	}
	else if (In.bIsPistol)
	{
		EnsurePistolLoaded();
		Idle = GPistol.Idle;
		Fall = GPistol.Fall;
		Reload = GPistol.Reload;
		Equip = GPistol.Equip;
		JumpStart = GPistol.JumpStart;
		Land = GPistol.Land;
		LandAdd = (GPistol.Land && GPistol.Land->IsValidAdditive()) ? GPistol.Land : nullptr;
		for (int32 K = 0; K < 8; ++K)
		{
			Walk[K] = GPistol.Walk[K];
			Jog[K] = GPistol.Jog[K];
		}
	}
	else
	{
		if (!A->bAssetsOk) { Idle = nullptr; return; }
		Idle = A->Idle; Fall = A->Fall; Reload = A->Reload; Equip = A->Equip; JumpStart = A->JumpStart; Land = A->Land;
		AirRise = GExtra.RifleRise; AirApex = GExtra.RifleApex; LandAdd = GExtra.RifleLandAdd;
		for (int32 K = 0; K < 8; ++K)
		{
			Walk[K] = A->Walk.IsValidIndex(K) ? A->Walk[K].Get() : nullptr;
			Jog[K] = A->Jog.IsValidIndex(K) ? A->Jog[K].Get() : nullptr;
		}
	}
	for (int32 K = 0; K < 4; ++K) Hit[K] = A->HitReacts.Num() ? A->HitReacts[K % A->HitReacts.Num()].Get() : nullptr;
}

void FAAARifleProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);
	LastDt = Dt;
	IdleT += Dt;

	// crouch: a stiff, slightly under-damped spring (ducking is quick and sinks a touch past the pose, standing
	// up to fire over cover is a quick push); lean: plain smoothing
	{
		const float Ct = FMath::Clamp(In.CrouchWeight, 0.f, 1.f);
		const float SubC = Dt * 0.5f;
		for (int32 S = 0; S < 2; ++S)
		{
			CrouchV += ((Ct - CrouchW) * 150.f - CrouchV * 19.f) * SubC;
			CrouchW += CrouchV * SubC;
		}
		CrouchW = FMath::Clamp(CrouchW, 0.f, 1.08f);
		LeanW = FMath::FInterpTo(LeanW, FMath::Clamp(In.LeanAmount, -1.f, 1.f), Dt, 7.f);
	}

	// Smooth weapon swap transition
	if (WeaponSwapBlend > 0.001f)
	{
		WeaponSwapBlend = FMath::FInterpTo(WeaponSwapBlend, 0.f, Dt, 6.f);
	}

	// Track character world yaw for procedural Turn In Place foot movement
	const float CurrentYaw = FRotator::NormalizeAxis((float)GetComponentTransform().Rotator().Yaw);
	if (!bTurnYawInit)
	{
		LastTurnYaw = CurrentYaw;
		bTurnYawInit = true;
	}
	const float TurnDelta = FRotator::NormalizeAxis(CurrentYaw - LastTurnYaw);
	LastTurnYaw = CurrentYaw;

	const float TurnRate = (Dt > 0.0001f) ? (TurnDelta / Dt) : 0.f;
	const bool bStanding = (SmoothSpeed < 22.f && !In.bFalling);
	const bool bIsTurning = bStanding && (FMath::Abs(TurnDelta) > 0.04f || FMath::Abs(TurnRate) > 8.f);

	if (bIsTurning)
	{
		const float TargetDir = (TurnDelta >= 0.f) ? 1.0f : -1.0f;
		SmoothTurnDir = FMath::FInterpTo(SmoothTurnDir, TargetDir, Dt, 16.f);

		// 64 degrees of turn corresponds to 1 full 2-step cycle (32 deg per single foot step)
		constexpr float DegPerCycle = 64.0f;
		const float StepAdvance = FMath::Max(FMath::Abs(TurnDelta) / DegPerCycle, Dt * 0.85f);
		TurnPhase = FMath::Frac(TurnPhase + StepAdvance);
		TurnWeight = FMath::FInterpTo(TurnWeight, 1.0f, Dt, 16.f);
	}
	else
	{
		// Complete the current half-step toward foot touchdown (0.5 or 1.0) so the lifted foot plants cleanly on the ground
		if (TurnWeight > 0.01f && bStanding)
		{
			const float TargetTouchdown = (TurnPhase < 0.5f) ? 0.5f : 1.0f;
			const float Rem = TargetTouchdown - TurnPhase;
			if (Rem > 0.01f && Rem < 0.48f)
			{
				TurnPhase = FMath::Min(TargetTouchdown, TurnPhase + Dt * 2.2f);
				if (TurnPhase >= 1.0f) TurnPhase = 0.0f;
			}
		}
		TurnWeight = FMath::FInterpTo(TurnWeight, 0.0f, Dt, bStanding ? 9.f : 20.f);
	}

	// 1. Separate Speed and Direction Angle to eliminate dead-stops and sector pops
	const float TargetSpeed = (float)In.LocalVel.Size();
	const float AccelRate = (TargetSpeed > SmoothSpeed) ? 9.5f : 13.5f;
	SmoothSpeed = FMath::FInterpTo(SmoothSpeed, TargetSpeed, Dt, AccelRate);

	// Angle smoothing
	if (TargetSpeed > 10.f)
	{
		float TargetAng = FMath::RadiansToDegrees(FMath::Atan2((float)In.LocalVel.Y, (float)In.LocalVel.X));
		if (TargetAng < 0.f) TargetAng += 360.f;

		if (SmoothSpeed < 20.f)
		{
			// Instant direction orientation from idle to prevent spinning footwork
			SmoothAngle = TargetAng;
		}
		else
		{
			// Continuous shortest-turn angle interpolation across 360 deg
			SmoothAngle = FMath::FixedTurn(SmoothAngle, TargetAng, Dt * 520.f);
			if (SmoothAngle < 0.f) SmoothAngle += 360.f;
			if (SmoothAngle >= 360.f) SmoothAngle -= 360.f;
		}
	}

	// 8-way locomotion sectors with smooth cosine/hermite interpolation
	DirK0 = FMath::FloorToInt(SmoothAngle / 45.f) % 8;
	DirK1 = (DirK0 + 1) % 8;
	const float RawDirF = SmoothAngle / 45.f - FMath::FloorToFloat(SmoothAngle / 45.f);
	DirF = FMath::SmoothStep(0.f, 1.f, RawDirF);

	// 2. Walk <-> Jog blend with S-curve
	const float Vw = FMath::Lerp(WalkSpd[DirK0], WalkSpd[DirK1], DirF);
	const float Vj = FMath::Lerp(JogSpd[DirK0], JogSpd[DirK1], DirF);
	JogW = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((SmoothSpeed - Vw) / FMath::Max(Vj - Vw, 35.f), 0.f, 1.f));
	MoveSpd = FMath::Lerp(Vw, Vj, JogW);
	if (CrouchW > 0.01f)
	{
		// crouched: only the walk set, and the clip's natural speed is lowered so the cadence stays natural while
		// the strides are shortened in Evaluate (feet gathered under the lowered hips)
		const float Cw = FMath::Clamp(CrouchW, 0.f, 1.f);
		JogW *= 1.f - Cw;
		MoveSpd = FMath::Lerp(Vw, Vj, JogW) * FMath::Lerp(1.f, GCrouchStride, Cw);
	}

	// 3. Play rate strictly matching ground velocity (eliminating foot sliding)
	const float Lw = FMath::Lerp(SeqLen(Walk[DirK0]), SeqLen(Walk[DirK1]), DirF);
	const float Lj = FMath::Lerp(SeqLen(Jog[DirK0]), SeqLen(Jog[DirK1]), DirF);
	const float Len = FMath::Max(FMath::Lerp(Lw, Lj, JogW), 0.05f);
	// Above the clip's natural speed, raise the cadence only gently (a real runner lengthens the stride
	// rather than spinning the legs); a linear rate made the squad's run look like fast-forward.
	const float RawRate = SmoothSpeed / FMath::Max(MoveSpd, 1.f);
	float Rate = RawRate <= 1.f ? FMath::Max(RawRate, 0.f) : FMath::Min(1.f + (RawRate - 1.f) * 0.45f, 1.22f);
	// crouched: the strides are GCrouchStride of the clip's, so the cadence must follow the speed exactly (no sliding)
	if (CrouchW > 0.01f) Rate = FMath::Lerp(Rate, FMath::Clamp(RawRate, 0.f, 2.4f), FMath::Clamp(CrouchW, 0.f, 1.f));
	if (SmoothSpeed > 3.f)
	{
		Phase = FMath::Frac(Phase + Dt * Rate / Len);
	}

	// 3b. Grenade throw secondary motion. The torso does not follow the arm rigidly: two under-damped springs
	//     (yaw / pitch) chase keyframed targets, so the wind-up coils the chest, the whip overshoots past
	//     square and the follow-through settles back with a small rebound. Timeline (s): 0-0.18 bring the
	//     grenade to the chest, 0.18-0.40 wind-up behind the ear, 0.40-0.52 whip (release 0.44),
	//     0.52-0.75 follow-through across the body, 0.75-0.95 hand back to the gun.
	{
		CookW = FMath::FInterpTo(CookW, FMath::Clamp(In.CookWeight, 0.f, 1.f), Dt, 9.f);
		const float Tt = In.ThrowTime;
		if (Tt >= 0.f && PrevThrowT < 0.f) ThrowStartCook = CookW;
		PrevThrowT = Tt;

		auto Key = [](float T, const float* Ks, const float* Vs, int32 N)
		{
			if (T <= Ks[0]) return Vs[0];
			for (int32 I = 1; I < N; ++I)
			{
				if (T <= Ks[I])
				{
					const float A = (T - Ks[I - 1]) / FMath::Max(Ks[I] - Ks[I - 1], 0.001f);
					return FMath::Lerp(Vs[I - 1], Vs[I], FMath::SmoothStep(0.f, 1.f, A));
				}
			}
			return Vs[N - 1];
		};
		float YawT = -5.f * CookW, PitchT = 0.f;
		if (false && Tt >= 0.f && Tt < 0.95f)   // (the baked throw clip drives the spine now)
		{
			static const float YK[] = { 0.f, 0.18f, 0.40f, 0.50f, 0.64f, 0.85f };
			static const float YV[] = { 0.f, -7.f, -24.f, 22.f, 12.f, 0.f };
			static const float PK[] = { 0.f, 0.18f, 0.40f, 0.50f, 0.68f, 0.88f };
			static const float PV[] = { 0.f, 1.f, 7.f, -14.f, -6.f, 0.f };
			YawT = Key(Tt, YK, YV, 6) + (1.f - FMath::Clamp(Tt / 0.18f, 0.f, 1.f)) * -5.f * ThrowStartCook;
			PitchT = Key(Tt, PK, PV, 6);
		}
		// w^2 = 240, c = 19 -> zeta ~0.61: one visible overshoot, settled in ~0.4 s
		const float SubDt = Dt * 0.5f;
		for (int32 S = 0; S < 2; ++S)
		{
			ThrowYawV += ((YawT - ThrowYaw) * 240.f - ThrowYawV * 19.f) * SubDt;
			ThrowYaw += ThrowYawV * SubDt;
			ThrowPitchV += ((PitchT - ThrowPitch) * 240.f - ThrowPitchV * 19.f) * SubDt;
			ThrowPitch += ThrowPitchV * SubDt;
		}
		ThrowYaw = FMath::Clamp(ThrowYaw, -40.f, 40.f);
		ThrowPitch = FMath::Clamp(ThrowPitch, -25.f, 20.f);
	}

	// 3c. Kar98k handling secondary motion: every bolt stroke and every round pressed in gives the rifle a small jolt
	//     (it dips, rolls, the muzzle nods), then it settles on under-damped springs - the hands absorb it, nothing is rigid
	{
		auto Crossed = [](float Prev, float Cur, float At) { return Prev >= 0.f && Prev < At && Cur >= At; };
		const float St = In.bKarRifle ? In.SniperFireTime : -1.f;
		if (Crossed(PrevKarShotT, St, 0.80f)) { KarDipV += 30.f; KarPitchV -= 50.f; }                   // bolt hits the rear stop
		if (Crossed(PrevKarShotT, St, 1.02f)) { KarDipV += 45.f; KarRollV += 70.f; KarPitchV += 40.f; }  // bolt slammed home
		if (Crossed(PrevKarShotT, St, 1.15f)) { KarRollV -= 90.f; }                                       // handle turned down: locked
		PrevKarShotT = St;
		const float Rt = In.bKarRifle ? In.ReloadTime : -1.f;
		const int32 Nr = FMath::Clamp(In.KarReloadN, 0, 5);
		if (Crossed(PrevKarReloadT, Rt, 0.36f)) { KarDipV += 35.f; KarPitchV -= 40.f; }                  // bolt open
		for (int32 K = 0; K < Nr; ++K)
			if (Crossed(PrevKarReloadT, Rt, AAAKarReload::PushTime(K))) { KarDipV += 95.f; KarRollV += 80.f; KarPitchV -= 70.f; }   // thumb presses a round down
		const float C0 = AAAKarReload::CloseStart(Nr);
		if (Crossed(PrevKarReloadT, Rt, C0 + 0.18f)) { KarDipV += 45.f; KarRollV += 60.f; }              // bolt forward
		if (Crossed(PrevKarReloadT, Rt, C0 + 0.28f)) { KarRollV -= 90.f; }                                // handle down
		PrevKarReloadT = Rt;
		// the body's movement reaches the rifle: every footstep (two per locomotion cycle; harder when running or
		// crouch-walking) and a landing jolt it; in the air it floats up a little (see the reload pose)
		if (In.bKarRifle)
		{
			const float StepK = FMath::Clamp(MoveWeight, 0.f, 1.f) * FMath::Clamp(SmoothSpeed / 300.f, 0.3f, 1.8f) * (In.bFalling ? 0.f : 1.f);
			const float Ph = FMath::Frac(Phase * 2.f);
			if (StepK > 0.05f && Ph < PrevKarStepPh)
			{
				KarDipV += 26.f * StepK;
				KarRollV += (bKarStepL ? 28.f : -28.f) * StepK;
				KarPitchV -= 10.f * StepK;
				bKarStepL = !bKarStepL;
			}
			PrevKarStepPh = Ph;
			if (bKarWasAir && !In.bFalling) { KarDipV += 160.f; KarPitchV -= 90.f; KarRollV += (bKarStepL ? 40.f : -40.f); }
			bKarWasAir = In.bFalling;
		}
		const float SubK = Dt * 0.5f;
		for (int32 S = 0; S < 2; ++S)
		{
			KarDipV += (-KarDip * 420.f - KarDipV * 24.f) * SubK;     KarDip += KarDipV * SubK;
			KarRollV += (-KarRoll * 380.f - KarRollV * 22.f) * SubK;  KarRoll += KarRollV * SubK;
			KarPitchV += (-KarPitch * 400.f - KarPitchV * 24.f) * SubK; KarPitch += KarPitchV * SubK;
		}
		KarDip = FMath::Clamp(KarDip, -4.f, 4.f); KarRoll = FMath::Clamp(KarRoll, -8.f, 8.f); KarPitch = FMath::Clamp(KarPitch, -6.f, 6.f);
	}

	// 4. Smooth Idle <-> Move weight curve (eliminating start/stop popping)
	const float TargetMoveW = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((SmoothSpeed - 6.f) / 80.f, 0.f, 1.f));
	MoveWeight = FMath::FInterpTo(MoveWeight, TargetMoveW, Dt, 14.f);

	// 5. ADS Smooth Aim Weight
	AimWeight = FMath::FInterpTo(AimWeight, In.bAiming ? 1.f : 0.f, Dt, 12.f);
	BinocularsWeight = FMath::FInterpTo(BinocularsWeight, In.bBinoculars ? 1.f : 0.f, Dt, 8.f);

	// 5b. Multi-Stage Secondary Motion Shooting Reaction (Delayed Coupled Harmonic Springs)
	{
		// Detect new shot impulse when Kick jumps upward
		const float KickJump = FMath::Max(0.f, In.Kick - PrevKick);
		PrevKick = In.Kick;

		if (KickJump > 0.02f)
		{
			const float ShotImpulse = FMath::Clamp(KickJump * 2.2f, 0.25f, 1.8f);
			ShoulderRecoilV += ShotImpulse * 18.0f;
			TorsoRecoilV    += ShotImpulse * 9.5f;
			HeadWhipV       += ShotImpulse * 12.0f;
			PelvisBraceV    += ShotImpulse * 5.5f;
			SustainedFire    = FMath::Min(1.0f, SustainedFire + 0.28f);
			// reference-matched kick (handgun full size; a shouldered rifle flips and lifts much less)
			// A follow-up shot (< 0.4 s after the last one) finds the shooter already braced and pulling the muzzle
			// down: only ~30% of the flip / rise. Without this the impulses stack in rapid fire and the weapon
			// climbs and stays pointed up (on top of the camera recoil).
			const float Follow = RefSince < 0.4f ? 0.3f : 1.f;
			const float Sn = FMath::Clamp(KickJump / 0.2f, 0.6f, 1.3f) * (In.bAiming ? 0.85f : 1.f) * Follow;
			RefPitchV += 237.f * Sn * (In.bIsPistol ? 1.f : 0.22f);   // shouldered rifle: ~3 deg flip (the camera does the climb)
			RefLiftV  += 22.f * Sn * (In.bIsPistol ? 1.f : 0.45f);
			RefBackT   = FMath::Min(RefBackT + 4.0f * Sn * (In.bIsPistol ? 1.f : 0.7f), In.bIsPistol ? 6.f : 3.5f);
			RefSince   = 0.f;
			RefYawV   += FMath::FRandRange(-1.f, 1.f) * 38.f * Sn * (In.bIsPistol ? 1.f : 0.55f);
			RefRollV  += FMath::FRandRange(0.3f, 1.f) * (FMath::RandBool() ? 1.f : -1.f) * 55.f * Sn * (In.bIsPistol ? 1.f : 0.45f);
		}
		else
		{
			SustainedFire = FMath::FInterpTo(SustainedFire, (In.Kick > 0.08f) ? 0.65f : 0.0f, Dt, 5.5f);
		}

		RecoilSideSmooth = FMath::FInterpTo(RecoilSideSmooth, In.KickSide, Dt, 18.f);
		FireTremorPhase  = FMath::Frac(FireTremorPhase + Dt * 19.0f); // 19 Hz muscular/weapon vibration during sustained fire

		RefSince += Dt;
		if (RefSince > 1.2f)   // at rest: snap the leftovers to exactly zero
		{
			if (FMath::Abs(RefPitch) < 0.03f && FMath::Abs(RefPitchV) < 0.3f) { RefPitch = RefPitchV = 0.f; }
			if (FMath::Abs(RefLift) < 0.02f && FMath::Abs(RefLiftV) < 0.1f) { RefLift = RefLiftV = 0.f; }
			if (RefBackT < 0.01f && RefBack < 0.01f && FMath::Abs(RefHead) < 0.02f) { RefBackT = RefBack = RefHead = RefHeadV = 0.f; }
		}
		// Sub-step coupled springs so they stay unconditionally stable at any frame rate
		const int32 SubSteps = (Dt > 0.025f) ? 3 : 2;
		const float SubDt = Dt / (float)SubSteps;
		for (int32 S = 0; S < SubSteps; ++S)
		{
			// Stage 1: Fast Shoulder & Clavicle Buttstock Compression (w^2 = 340, c = 21)
			const float ShoulderForce = (In.Kick * 0.65f - ShoulderRecoil) * 340.f - ShoulderRecoilV * 21.f;
			ShoulderRecoilV += ShoulderForce * SubDt;
			ShoulderRecoil  = FMath::Clamp(ShoulderRecoil + ShoulderRecoilV * SubDt, -0.6f, 2.0f);

			// Stage 2: Delayed Upper/Mid Torso Wave with Underdamped Rebound (w^2 = 175, c = 13)
			const float TorsoForce = (ShoulderRecoil * 0.85f - TorsoRecoil) * 175.f - TorsoRecoilV * 13.f;
			TorsoRecoilV += TorsoForce * SubDt;
			TorsoRecoil  = FMath::Clamp(TorsoRecoil + TorsoRecoilV * SubDt, -0.7f, 2.0f);

			// Stage 3: Delayed Neck/Head Whip (inertial lag + snap-back, w^2 = 125, c = 10.5)
			const float HeadForce = ((TorsoRecoil - In.Kick * 0.35f) - HeadWhip) * 125.f - HeadWhipV * 10.5f;
			HeadWhipV += HeadForce * SubDt;
			HeadWhip  = FMath::Clamp(HeadWhip + HeadWhipV * SubDt, -0.8f, 2.0f);

			// Stage 4: Lower-Core & Pelvis Shock Absorption (w^2 = 95, c = 11)
			const float PelvisForce = (TorsoRecoil * 0.60f - PelvisBrace) * 95.f - PelvisBraceV * 11.f;
			PelvisBraceV += PelvisForce * SubDt;
			PelvisBrace  = FMath::Clamp(PelvisBrace + PelvisBraceV * SubDt, -0.5f, 1.5f);

			// reference-matched springs (fitted to the clip: see the header). The first swing keeps the clip's
			// flip / undershoot / sag; after it the damping goes to (near) critical, so nothing keeps rocking.
			// sustained fire (2nd shot on): the grip is tense, the springs are critically damped from the start
			const float Braced = FMath::Clamp((SustainedFire - 0.3f) / 0.4f, 0.f, 1.f);
			const float SetP = FMath::Max(Braced, FMath::Clamp((RefSince - 0.28f) / 0.25f, 0.f, 1.f));
			const float SetL = FMath::Max(Braced, FMath::Clamp((RefSince - 0.75f) / 0.35f, 0.f, 1.f));
			RefPitchV += (-267.f * RefPitch - FMath::Lerp(9.1f, 34.f, SetP) * RefPitchV) * SubDt;
			RefPitch   = FMath::Clamp(RefPitch + RefPitchV * SubDt, -8.f, 18.f);
			RefLiftV  += (-54.2f * RefLift - FMath::Lerp(4.4f, 16.f, SetL) * RefLiftV) * SubDt;
			RefLift    = FMath::Clamp(RefLift + RefLiftV * SubDt, -4.f, 5.f);
			RefBack   += (RefBackT - RefBack) * FMath::Min(1.f, SubDt / 0.025f);                     // ~25 ms attack
			RefBackT  -= RefBackT * FMath::Min(1.f, SubDt / (RefSince < 0.3f ? 0.6f : 0.3f));      // recovery
			RefHeadV  += ((RefBack - RefHead) * 49.f - 10.5f * RefHeadV) * SubDt;                    // head lags the hands
			RefHead    = FMath::Clamp(RefHead + RefHeadV * SubDt, -2.f, 6.f);
			RefYawV   += (-196.f * RefYaw - FMath::Lerp(15.4f, 28.f, SetP) * RefYawV) * SubDt;   // 14 rad/s, zeta 0.55 -> 1
			RefYaw     = FMath::Clamp(RefYaw + RefYawV * SubDt, -4.f, 4.f);
			RefRollV  += (-196.f * RefRoll - FMath::Lerp(15.4f, 28.f, SetP) * RefRollV) * SubDt;
			RefRoll    = FMath::Clamp(RefRoll + RefRollV * SubDt, -6.f, 6.f);
		}
	}

	// 5c. Weapon inertia: it lags behind turning, looking up / down and speeding up / braking, then catches up
	{
		const float Yaw = (float)GetActorTransform().Rotator().Yaw;
		if (!bSwayInit) { PrevYaw = Yaw; PrevAimPitch = In.AimPitch; PrevVel = In.LocalVel; bSwayInit = true; }
		const float InvDt = 1.f / FMath::Max(Dt, 0.001f);
		const float YawRate = FMath::Clamp(FMath::FindDeltaAngleDegrees(PrevYaw, Yaw) * InvDt, -720.f, 720.f);
		const float PitchRate = FMath::Clamp((In.AimPitch - PrevAimPitch) * InvDt, -720.f, 720.f);
		const FVector2D Acc = (In.LocalVel - PrevVel) * InvDt;   // cm/s^2 (forward, right)
		PrevYaw = Yaw; PrevAimPitch = In.AimPitch; PrevVel = In.LocalVel;
		const float Heavy = In.bIsPistol ? 0.7f : 1.f;          // the rifle has more mass to drag along
		const float TYaw = FMath::Clamp(-YawRate * 0.010f * Heavy, -5.f, 5.f);
		const float TPitch = FMath::Clamp(-PitchRate * 0.008f * Heavy, -4.f, 4.f);
		const float TX = FMath::Clamp(-(float)Acc.X * 0.0012f * Heavy, -2.5f, 2.5f);
		const float TY = FMath::Clamp(-(float)Acc.Y * 0.0012f * Heavy, -2.5f, 2.5f);
		const int32 N = Dt > 0.025f ? 3 : 2;
		const float Sd = Dt / N;
		for (int32 K = 0; K < N; ++K)
		{
			// angles: 11 rad/s, zeta 0.7 - linear: 9 rad/s, zeta 0.7 (no ringing)
			SwayYawV   += ((TYaw - SwayYaw) * 121.f - 15.4f * SwayYawV) * Sd;     SwayYaw   += SwayYawV * Sd;
			SwayPitchV += ((TPitch - SwayPitch) * 121.f - 15.4f * SwayPitchV) * Sd; SwayPitch += SwayPitchV * Sd;
			SwayXV     += ((TX - SwayX) * 81.f - 12.6f * SwayXV) * Sd;           SwayX     += SwayXV * Sd;
			SwayYV     += ((TY - SwayY) * 81.f - 12.6f * SwayYV) * Sd;           SwayY     += SwayYV * Sd;
		}
	}

	// 6. Air: take-off -> rising -> apex -> falling (each with a short crossfade) -> landing recovery
	if (In.bFalling && !bWasFalling)
	{
		StartT = In.bJumpUp ? 0.f : -1.f;
		FallT = 0.f;
		AirState = (In.bJumpUp && JumpStart) ? 0 : 3;
		AirPrev = -1; AirT = 0.f; AirBlend = 0.f;
	}
	if (In.bFalling)
	{
		AirT += Dt; AirPrevT += Dt;
		int32 Want = AirState;
		if (AirState == 0 && AirT >= SeqLen(JumpStart) - 0.12f) Want = (AirRise && In.VelZ > 140.f) ? 1 : ((AirApex && In.VelZ > -260.f) ? 2 : 3);
		else if (AirState == 1 && In.VelZ < 140.f) Want = AirApex ? 2 : 3;
		else if (AirState == 2 && (AirT >= SeqLen(AirApex) - 0.1f || In.VelZ < -380.f)) Want = 3;
		if (Want != AirState) { AirPrev = AirState; AirPrevT = AirT; AirState = Want; AirT = 0.f; AirBlend = 1.f; }
		AirBlend = FMath::Max(0.f, AirBlend - Dt / 0.18f);
	}
	if (!In.bFalling && bWasFalling)
	{
		LandT = 0.f;
	}
	bWasFalling = In.bFalling;

	if (StartT >= 0.f)
	{
		StartT += Dt;
		if (!JumpStart || StartT >= SeqLen(JumpStart))
		{
			StartT = -1.f;
		}
	}
	if (In.bFalling) FallT += Dt;
	if (LandT >= 0.f)
	{
		LandT += Dt;
		const float MaxLandDuration = FMath::Max(Land ? FMath::Min(SeqLen(Land), 0.55f) : 0.55f, LandAdd ? FMath::Min(SeqLen(LandAdd), 0.7f) : 0.f);
		if (LandT >= MaxLandDuration) LandT = -1.f;
	}
	FallW = FMath::FInterpTo(FallW, In.bFalling ? 1.f : 0.f, Dt, In.bFalling ? 10.f : 14.f);
	ReloadW = FMath::FInterpTo(ReloadW, In.ReloadTime >= 0.f ? 1.f : 0.f, Dt, 12.f);
	EquipW = FMath::FInterpTo(EquipW, In.EquipTime >= 0.f ? 1.f : 0.f, Dt, 14.f);
	// left hand to the magazine (IK): on while the hand goes for the magazine, off once it holds it
	MagIKW = FMath::FInterpTo(MagIKW, (In.ReloadTime >= 0.f && In.ReloadStage == 0 && !In.bKarRifle && !In.MagRelLoc.IsNearlyZero()) ? 1.f : 0.f, Dt, 14.f);
}

/** measure every walk / jog clip once: animated speed (planted foot) and the cycle position of "left foot in front" */
void FAAARifleProxy::Analyze(FPoseContext& Output)
{
	bAnalyzed = true;
	struct FCache { const UAnimSequence* Key = nullptr; float WS[8], JS[8], WO[8], JO[8]; bool bValid = false; };
	static FCache CacheRifle, CachePistol, CacheUnarmed;
	FCache& Cache = In.bUnarmed ? CacheUnarmed : (In.bIsPistol ? CachePistol : CacheRifle);
	if (Cache.bValid && Idle && Cache.Key == Idle)
	{
		for (int32 K = 0; K < 8; ++K) { WalkSpd[K] = Cache.WS[K]; JogSpd[K] = Cache.JS[K]; WalkOff[K] = Cache.WO[K]; JogOff[K] = Cache.JO[K]; }
		return;
	}
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();
	static const FName NFootL(TEXT("foot_l")), NFootR(TEXT("foot_r"));
	const FCompactPoseBoneIndex FootL = Bone(BC, NFootL), FootR = Bone(BC, NFootR);
	if (!FootL.IsValid() || !FootR.IsValid()) return;
	const FVector Up = GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	FPoseContext Pc(Output);
	auto Measure = [&](const UAnimSequence* S, float& OutSpd, float& OutOff)
	{
		if (!S) return;
		constexpr int32 N = 32;
		const float L = SeqLen(S), Step = L / N;
		FVector Lp[N], Rp[N];
		for (int32 I = 0; I < N; ++I)
		{
			Sample(S, Step * I, Pc);
			FCSPose<FCompactPose> Cs;
			Cs.InitPose(Pc.Pose);
			Lp[I] = Cs.GetComponentSpaceTransform(FootL).GetLocation();
			Rp[I] = Cs.GetComponentSpaceTransform(FootR).GetLocation();
		}
		TArray<float> V;
		FVector Dir = FVector::ZeroVector;
		for (int32 I = 0; I < N; ++I)
		{
			const int32 J = (I + 1) % N;
			const bool bLeft = FVector::DotProduct(Lp[I], Up) < FVector::DotProduct(Rp[I], Up);   // the lower foot is on the ground
			FVector Dp = bLeft ? Lp[J] - Lp[I] : Rp[J] - Rp[I];
			Dp -= Up * FVector::DotProduct(Dp, Up);
			V.Add((float)Dp.Size() / Step);
			Dir -= Dp;   // the planted foot slides backwards
		}
		V.Sort();
		OutSpd = FMath::Clamp(V[N / 2], 40.f, 900.f);
		Dir = Dir.GetSafeNormal();
		int32 Best = 0; double BestD = -1e9;
		for (int32 I = 0; I < N; ++I) { const double Dd = FVector::DotProduct(Lp[I] - Rp[I], Dir); if (Dd > BestD) { BestD = Dd; Best = I; } }
		OutOff = 0.f; // Keep all 8 directions and walk/jog perfectly synchronized on frame 0
	};
	for (int32 K = 0; K < 8; ++K)
	{
		Measure(Walk[K], WalkSpd[K], WalkOff[K]);
		Measure(Jog[K], JogSpd[K], JogOff[K]);
		JogSpd[K] = FMath::Max(JogSpd[K], WalkSpd[K] * 1.25f);
		WalkOff[K] = 0.f;
		JogOff[K] = 0.f;
	}
	Cache.Key = Idle;
	Cache.bValid = true;
	for (int32 K = 0; K < 8; ++K) { Cache.WS[K] = WalkSpd[K]; Cache.JS[K] = JogSpd[K]; Cache.WO[K] = 0.f; Cache.JO[K] = 0.f; }
	UE_LOG(LogTemp, Log, TEXT("AAA Weapons timing: walk %.0f/%.0f/%.0f/%.0f cm/s, jog %.0f/%.0f/%.0f/%.0f cm/s (fwd/right/back/left)"),
		WalkSpd[0], WalkSpd[2], WalkSpd[4], WalkSpd[6], JogSpd[0], JogSpd[2], JogSpd[4], JogSpd[6]);
}

// Retarget the baked Mixamo clip onto the mannequin: every driven bone gets  delta(t) * align, where align turns the
// reference bone to point like the Mixamo bind (T-pose) bone. Rotations only: the capsule follows the clip's hips.
void FAAARifleProxy::ApplyClimbClip(FPoseContext& Output, const FVector& FwdCS, const FVector& UpCS)
{
	namespace Clip = AAAClipBracedHang;
	const float W = FMath::Clamp(In.ClimbClipWeight, 0.f, 1.f);
	if (In.ClimbClipTime < 0.f || W <= 0.001f) return;
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	const int32 NB = Pose.GetNumBones();

	// (re)build the bone map and the reference pose data when the bone set changes (LOD)
	if (ClipMapNum != NB)
	{
		ClipMapNum = NB;
		ClipBoneOfCompact.Init(-1, NB);
		ClipRefRot.Init(FQuat::Identity, Clip::NumBones);
		ClipRefDir.Init(FVector::ZeroVector, Clip::NumBones);
		FCompactPose RefPose = Pose;
		RefPose.ResetToRefPose();
		FCSPose<FCompactPose> RefCS;
		RefCS.InitPose(RefPose);
		for (int32 K = 0; K < Clip::NumBones; ++K)
		{
			const FCompactPoseBoneIndex Bi = Bone(BC, FName(Clip::Bone[K]));
			if (!Bi.IsValid()) continue;
			ClipBoneOfCompact[Bi.GetInt()] = K;
			const FTransform T = RefCS.GetComponentSpaceTransform(Bi);
			ClipRefRot[K] = T.GetRotation();
			FVector Dir = FVector::ZeroVector;
			const FCompactPoseBoneIndex Ci = Clip::DirChild[K][0] ? Bone(BC, FName(Clip::DirChild[K])) : FCompactPoseBoneIndex(INDEX_NONE);
			if (Ci.IsValid()) Dir = RefCS.GetComponentSpaceTransform(Ci).GetLocation() - T.GetLocation();
			else
			{
				const FCompactPoseBoneIndex Pi = RefPose.GetParentBoneIndex(Bi);
				if (Pi.IsValid()) Dir = T.GetLocation() - RefCS.GetComponentSpaceTransform(Pi).GetLocation();
			}
			ClipRefDir[K] = Dir.GetSafeNormal();
		}
	}

	// character frame (X forward, Y right, Z up) -> component space
	const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();
	const FQuat QB = FMatrix(FwdCS, RightCS, UpCS, FVector::ZeroVector).ToQuat();
	const FQuat QBi = QB.Inverse();

	const float Ft = FMath::Clamp(In.ClimbClipTime * Clip::FPS, 0.f, (float)(Clip::NumFrames - 1));
	const int32 F0 = FMath::Min((int32)Ft, Clip::NumFrames - 1), F1 = FMath::Min(F0 + 1, Clip::NumFrames - 1);
	const float Fa = Ft - (float)F0;

	TArray<FTransform> CSArr;
	CSArr.SetNum(NB);
	for (const FCompactPoseBoneIndex Bi : Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Pi = Pose.GetParentBoneIndex(Bi);
		const FTransform ParentCS = Pi.IsValid() ? CSArr[Pi.GetInt()] : FTransform::Identity;
		FTransform& L = Pose[Bi];
		const int32 K = ClipBoneOfCompact.IsValidIndex(Bi.GetInt()) ? ClipBoneOfCompact[Bi.GetInt()] : -1;
		if (K >= 0)
		{
			const float* A = Clip::Delta[F0][K];
			const float* B = Clip::Delta[F1][K];
			const FQuat D = FQuat::Slerp(FQuat(A[0], A[1], A[2], A[3]), FQuat(B[0], B[1], B[2], B[3]), Fa);
			const FQuat Dcs = QB * D * QBi;
			const FVector MixDir = QB.RotateVector(FVector(Clip::BindDir[K][0], Clip::BindDir[K][1], Clip::BindDir[K][2]));
			const FQuat Align = (ClipRefDir[K].IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(ClipRefDir[K], MixDir)) * ClipRefRot[K];
			const FQuat Desired = (Dcs * Align).GetNormalized();
			const FQuat LocalTarget = (ParentCS.GetRotation().Inverse() * Desired).GetNormalized();
			L.SetRotation(FQuat::Slerp(L.GetRotation(), LocalTarget, W).GetNormalized());
		}
		CSArr[Bi.GetInt()] = L * ParentCS;
	}
}

// Same retarget as ApplyClimbClip, for any baked clip header, restricted to a bone group (rotations only).
FTransform FAAARifleProxy::KarJolt() const
{
	// rifle-local: dip down, roll about the barrel, + pitch = muzzle up
	return FTransform(FQuat(FVector(1.0, 0.0, 0.0), FMath::DegreesToRadians(KarRoll)) * FQuat(FVector(0.0, 1.0, 0.0), FMath::DegreesToRadians(-KarPitch)),
		FVector(0.0, 0.0, -KarDip));
}

void FAAARifleProxy::ApplyBaked(FPoseContext& Output, FBakedMap& Map, const FBakedClipView& Clip, float Time, float W, int32 MaskKind, const FVector& FwdCS, const FVector& UpCS)
{
	W = FMath::Clamp(W, 0.f, 1.f);
	if (W <= 0.001f || Clip.NumFrames <= 0) return;
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& BC = Pose.GetBoneContainer();
	const int32 NB = Pose.GetNumBones();

	// which clip bones this group drives
	auto InGroup = [MaskKind](const FString& N) -> bool
	{
		const bool bFinger = N.StartsWith(TEXT("thumb_")) || N.StartsWith(TEXT("index_")) || N.StartsWith(TEXT("middle_")) || N.StartsWith(TEXT("ring_")) || N.StartsWith(TEXT("pinky_"));
		switch (MaskKind)
		{
		case 1: return N.StartsWith(TEXT("thigh_")) || N.StartsWith(TEXT("calf_")) || N.StartsWith(TEXT("foot_")) || N.StartsWith(TEXT("ball_"));
		case 2: return N.StartsWith(TEXT("spine_")) || N == TEXT("clavicle_l") || N == TEXT("upperarm_l") || N == TEXT("lowerarm_l") || N == TEXT("hand_l") || (bFinger && N.EndsWith(TEXT("_l")));
		default: return N.StartsWith(TEXT("spine_")) || N.StartsWith(TEXT("neck_")) || N == TEXT("head") || N.StartsWith(TEXT("clavicle_")) || N.StartsWith(TEXT("upperarm_")) || N.StartsWith(TEXT("lowerarm_")) || N.StartsWith(TEXT("hand_")) || bFinger;
		}
	};

	if (Map.Num != NB)
	{
		Map.Num = NB;
		Map.BoneOf.Init(-1, NB);
		Map.RefRot.Init(FQuat::Identity, Clip.NumBones);
		Map.RefDir.Init(FVector::ZeroVector, Clip.NumBones);
		FCompactPose RefPose = Pose;
		RefPose.ResetToRefPose();
		FCSPose<FCompactPose> RefCS;
		RefCS.InitPose(RefPose);
		for (int32 K = 0; K < Clip.NumBones; ++K)
		{
			const FCompactPoseBoneIndex Bi = Bone(BC, FName(Clip.Bone[K]));
			if (!Bi.IsValid()) continue;
			const FTransform T = RefCS.GetComponentSpaceTransform(Bi);
			Map.RefRot[K] = T.GetRotation();
			FVector Dir = FVector::ZeroVector;
			const FCompactPoseBoneIndex Ci = Clip.DirChild[K][0] ? Bone(BC, FName(Clip.DirChild[K])) : FCompactPoseBoneIndex(INDEX_NONE);
			if (Ci.IsValid()) Dir = RefCS.GetComponentSpaceTransform(Ci).GetLocation() - T.GetLocation();
			else
			{
				const FCompactPoseBoneIndex Pi = RefPose.GetParentBoneIndex(Bi);
				if (Pi.IsValid()) Dir = T.GetLocation() - RefCS.GetComponentSpaceTransform(Pi).GetLocation();
			}
			Map.RefDir[K] = Dir.GetSafeNormal();
			if (InGroup(FString(Clip.Bone[K]))) Map.BoneOf[Bi.GetInt()] = K;
		}
	}

	const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();
	const FQuat QB = FMatrix(FwdCS, RightCS, UpCS, FVector::ZeroVector).ToQuat();
	const FQuat QBi = QB.Inverse();

	const float Ft = FMath::Clamp(Time * Clip.FPS, 0.f, (float)(Clip.NumFrames - 1));
	const int32 F0 = FMath::Min((int32)Ft, Clip.NumFrames - 1), F1 = FMath::Min(F0 + 1, Clip.NumFrames - 1);
	const float Fa = Ft - (float)F0;

	TArray<FTransform> CSArr;
	CSArr.SetNum(NB);
	for (const FCompactPoseBoneIndex Bi : Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Pi = Pose.GetParentBoneIndex(Bi);
		const FTransform ParentCS = Pi.IsValid() ? CSArr[Pi.GetInt()] : FTransform::Identity;
		FTransform& L = Pose[Bi];
		const int32 K = Map.BoneOf.IsValidIndex(Bi.GetInt()) ? Map.BoneOf[Bi.GetInt()] : -1;
		if (K >= 0)
		{
			const float* A = Clip.Delta + ((size_t)F0 * Clip.NumBones + K) * 4;
			const float* B = Clip.Delta + ((size_t)F1 * Clip.NumBones + K) * 4;
			const FQuat D = FQuat::Slerp(FQuat(A[0], A[1], A[2], A[3]), FQuat(B[0], B[1], B[2], B[3]), Fa);
			const FQuat Dcs = QB * D * QBi;
			const FVector MixDir = QB.RotateVector(FVector(Clip.BindDir[K][0], Clip.BindDir[K][1], Clip.BindDir[K][2]));
			const FQuat Align = (Map.RefDir[K].IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(Map.RefDir[K], MixDir)) * Map.RefRot[K];
			const FQuat Desired = (Dcs * Align).GetNormalized();
			const FQuat LocalTarget = (ParentCS.GetRotation().Inverse() * Desired).GetNormalized();
			L.SetRotation(FQuat::Slerp(L.GetRotation(), LocalTarget, W).GetNormalized());
		}
		CSArr[Bi.GetInt()] = L * ParentCS;
	}
}

bool FAAARifleProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle) { Output.ResetToRefPose(); return true; }
	const FBoneContainer& BC = Output.Pose.GetBoneContainer();

	if (!bAnalyzed) Analyze(Output);

	// ---- locomotion: idle <-> 8-way walk / jog, all clips on one synced gait phase
	Sample(Idle, IdleT, Output);

	const float ClimbAtten = FMath::Clamp(1.0f - In.ClimbWeight * 0.92f, 0.0f, 1.0f);
	const float MoveW = MoveWeight * ClimbAtten;
	if (MoveW > 0.001f)
	{
		FPoseContext A(Output), B(Output), Move(Output);
		auto DirPose = [&](const UAnimSequence* const* Set, const float* Off, FPoseContext& Res)
		{
			Sample(Set[DirK0], FMath::Frac(Phase + Off[DirK0]) * SeqLen(Set[DirK0]), Res);
			Sample(Set[DirK1], FMath::Frac(Phase + Off[DirK1]) * SeqLen(Set[DirK1]), B);
			LerpPose(Res.Pose, B.Pose, DirF);
		};
		if (JogW < 1.f) DirPose(Walk, WalkOff, Move);
		if (JogW > 0.f)
		{
			if (JogW >= 1.f) DirPose(Jog, JogOff, Move);
			else { DirPose(Jog, JogOff, A); LerpPose(Move.Pose, A.Pose, JogW); }
		}
		LerpPose(Output.Pose, Move.Pose, MoveW);
	}
	// in the air: take-off with smooth crossfade into fall loop; landing recovery on touch-down
	const float EffectiveFallW = FallW * ClimbAtten;
	if (EffectiveFallW > 0.005f && (Fall || JumpStart))
	{
		// the clip of an air state: take-off / apex play once, rising / falling loop
		auto AirPose = [&](int32 St, float T, FPoseContext& Res)
		{
			if (St == 0 && JumpStart) SampleOnce(JumpStart, T, Res);
			else if (St == 1 && AirRise) Sample(AirRise, T, Res);
			else if (St == 2 && AirApex) SampleOnce(AirApex, T, Res);
			else if (Fall) Sample(Fall, FallT, Res);
			else SampleOnce(JumpStart, SeqLen(JumpStart), Res);
		};
		FPoseContext FC(Output);
		AirPose(AirState, AirT, FC);
		if (AirBlend > 0.001f && AirPrev >= 0)
		{
			FPoseContext PrevCtx(Output);
			AirPose(AirPrev, AirPrevT, PrevCtx);
			LerpPose(FC.Pose, PrevCtx.Pose, FMath::SmoothStep(0.f, 1.f, AirBlend));
		}
		LerpPose(Output.Pose, FC.Pose, EffectiveFallW);
	}
	// landing recovery additive, MASKED: the legs / hips take the impact fully, the upper spine partly, the arms not at
	// all (the additive was made on another base pose: on the aim pose it threw the pistol up over the head and folded
	// the body into a deep squat - user video 00:04)
	auto LandAdditiveMasked = [&](const UAnimSequence* Seq, float T, float Alpha)
	{
		if (!Seq || Alpha <= 0.001f) return;
		FPoseContext LC(Output);
		SampleOnce(Seq, T, LC);
		FPoseContext Full(Output);
		Full.Pose.CopyBonesFrom(Output.Pose);
		Full.Curve.CopyFrom(Output.Curve);
		FAnimationPoseData BaseData(Full);
		const FAnimationPoseData AdditiveData(LC);
		FAnimationRuntime::AccumulateAdditivePose(BaseData, AdditiveData, Alpha, Seq->AdditiveAnimType);
		static const FName NClavLm(TEXT("clavicle_l")), NClavRm(TEXT("clavicle_r")), NSpine3m(TEXT("spine_03"));
		const FCompactPoseBoneIndex ClL = Bone(BC, NClavLm), ClR = Bone(BC, NClavRm), Sp3 = Bone(BC, NSpine3m);
		TArray<float> LMask;
		LMask.Init(1.f, Output.Pose.GetNumBones());
		for (const FCompactPoseBoneIndex Bi : Output.Pose.ForEachBoneIndex())
			for (FCompactPoseBoneIndex P = Bi; P.IsValid(); P = Output.Pose.GetParentBoneIndex(P))
			{
				if (P == ClL || P == ClR) { LMask[Bi.GetInt()] = 0.f; break; }
				if (P == Sp3) { LMask[Bi.GetInt()] = 0.4f; break; }
			}
		LerpPose(Output.Pose, Full.Pose, 1.f, &LMask);
	};
	if (LandT >= 0.f && LandAdd && MoveW > 0.35f)
	{
		const float K = FMath::Clamp(LandT / FMath::Min(SeqLen(LandAdd), 0.7f), 0.f, 1.f);
		LandAdditiveMasked(LandAdd, LandT, 0.9f * (1.f - K * K));
	}
	else if (LandT >= 0.f && Land)
	{
		const float LandDur = FMath::Min(SeqLen(Land), 0.55f);
		const float K = FMath::Clamp(LandT / LandDur, 0.f, 1.f);
		const float LandAlpha = 0.85f * (1.f - K * K) * (1.f - MoveW * 0.55f);
		if (LandAlpha > 0.001f)
		{
			if (Land->IsValidAdditive())
			{
				LandAdditiveMasked(Land, LandT, LandAlpha);
			}
			else
			{
				FPoseContext LC(Output);
				Sample(Land, LandT, LC);
				LerpPose(Output.Pose, LC.Pose, LandAlpha);
			}
		}
	}

	// weapon / stance change: ease out of the old set's pose (holster, draw, pistol <-> rifle)
	const int32 NumB = Output.Pose.GetNumBones();
	if (WeaponSwapBlend > 0.001f && SwapFrom.Num() == NumB)
	{
		const float Wsw = FMath::SmoothStep(0.f, 1.f, WeaponSwapBlend);
		for (const FCompactPoseBoneIndex Bi : Output.Pose.ForEachBoneIndex())
		{
			FTransform T = Output.Pose[Bi];
			T.BlendWith(SwapFrom[Bi.GetInt()], Wsw);
			T.NormalizeRotation();
			Output.Pose[Bi] = T;
		}
	}
	LastPose.SetNum(NumB, EAllowShrinking::No);
	for (const FCompactPoseBoneIndex Bi : Output.Pose.ForEachBoneIndex()) LastPose[Bi.GetInt()] = Output.Pose[Bi];

	// dead: the death clip for the direction of the fatal hit takes the whole body (the ragdoll takes over after it)
	// (DeathIndex 100 + i = the player's death: he ends LYING ON HIS BACK - the clips stop mid-fall, made for the ragdoll)
	const bool bMartyr = In.DeathIndex >= 100;
	const int32 DeathI = In.DeathIndex % 100;
	if (In.DeathTime >= 0.f && DeathI >= 0 && DeathI < 6 && GExtra.Death[DeathI])
	{
		const UAnimSequence* DeathSeq = GExtra.Death[DeathI];
		FPoseContext DC(Output);
		SampleOnce(DeathSeq, In.DeathTime, DC);
		LerpPose(Output.Pose, DC.Pose, FMath::SmoothStep(0.f, 0.18f, In.DeathTime));

		// ---- lying on his back: from the second half of the clip the body is carried into a lying pose built from the
		//      reference pose - turned 90 deg backwards about his right axis round the point between his feet (the feet
		//      stay, the head goes back), the hips 14 cm above the floor, the legs a little open and bent, the arms by his
		//      sides, the chest and head a little raised (like a wounded man held up), the hips rolled 6 deg
		if (bMartyr)
		{
			const float Len = SeqLen(DeathSeq);
			const float Wl = FMath::SmoothStep(Len * 0.45f, Len + 0.2f, In.DeathTime);
			static const FName NPelL(TEXT("pelvis")), NSp3L(TEXT("spine_03")), NSp4L(TEXT("spine_04")), NSp5L(TEXT("spine_05")), NNeckL(TEXT("neck_01")), NHeadL(TEXT("head"));
			static const FName NThLL(TEXT("thigh_l")), NThRL(TEXT("thigh_r")), NCaLL(TEXT("calf_l")), NCaRL(TEXT("calf_r")), NUaLL(TEXT("upperarm_l")), NUaRL(TEXT("upperarm_r"));
			const FCompactPoseBoneIndex PelB = Bone(BC, NPelL);
			if (Wl > 0.001f && PelB.IsValid())
			{
				const FTransform& CT = GetComponentTransform();
				const FVector UpD = CT.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
				FVector FwdD = CT.InverseTransformVectorNoScale(GetActorTransform().GetUnitAxis(EAxis::X));
				FwdD = (FwdD - UpD * FVector::DotProduct(FwdD, UpD)).GetSafeNormal();
				FPoseContext LC(Output);
				LC.ResetToRefPose();
				FCSPose<FCompactPose> LCS;
				LCS.InitPose(LC.Pose);
				// the whole body falls backwards: Up -> -Fwd (head behind where he stood), Fwd -> Up (face to the sky)
				const FQuat FallQ = FQuat::FindBetweenNormals(UpD, -FwdD);
				const FQuat Roll(-FwdD, FMath::DegreesToRadians(6.f));   // (a little onto one side: not a statue)
				const FTransform P0 = LCS.GetComponentSpaceTransform(PelB);
				FTransform P1(Roll * FallQ * P0.GetRotation(), FallQ.RotateVector(P0.GetLocation()) + UpD * 14.0);
				P1.NormalizeRotation();
				SetCS(LCS, PelB, P1);
				// a bone turned about an axis (component space)
				auto Turn = [&](const FName& N, const FVector& Ax, float Deg)
				{
					const FCompactPoseBoneIndex B = Bone(BC, N);
					if (!B.IsValid()) return;
					FTransform T = LCS.GetComponentSpaceTransform(B);
					T.SetRotation((FQuat(Ax.GetSafeNormal(), FMath::DegreesToRadians(Deg)) * T.GetRotation()).GetNormalized());
					SetCS(LCS, B, T);
				};
				// lifting toward the sky = turning the head end (toward -Fwd) up: about Lift = (-Fwd) x Up by + degrees
				const FVector Lift = FVector::CrossProduct(-FwdD, UpD);
				Turn(NSp3L, Lift, 5.f); Turn(NSp4L, Lift, 6.f); Turn(NSp5L, Lift, 6.f);
				Turn(NNeckL, Lift, 14.f); Turn(NHeadL, Lift, 10.f);
				// the legs lie toward +Fwd: about -Lift (= Fwd x Up) + lifts them from the floor; about Up + turns toward his right.
				// knees a little bent (thighs up, shins back down to the floor), feet a little apart
				Turn(NThLL, -Lift, 14.f); Turn(NThRL, -Lift, 9.f);
				Turn(NCaLL, -Lift, -26.f); Turn(NCaRL, -Lift, -18.f);
				Turn(NThLL, UpD, -6.f); Turn(NThRL, UpD, 6.f);
				// the arms (A-pose: toward the feet and out) drawn in by his sides, on the floor
				Turn(NUaLL, UpD, 28.f); Turn(NUaRL, UpD, -28.f);
				FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(LCS), LC.Pose);
				LerpPose(Output.Pose, LC.Pose, Wl);
			}
		}

		// ---- the martyr's testimony (الشهادة): lying where he fell, the hero raises his right hand, the index finger
		//      pointing to the sky while he says it, holds it (a faint tremble), then the arm sinks back and he is gone.
		//      Timeline in s after the fall: raise 0..1.4, hold to 7.4, lower 7.4..9.0.
		const float Sh = In.ShahadaT;
		if (Sh >= 0.f && bMartyr)
		{
			// 1) the hand: a fist (the fingers of the idle clip's grip) with the index finger straight (reference pose)
			const float Wf = FMath::SmoothStep(0.3f, 1.0f, Sh) * (1.f - FMath::SmoothStep(8.4f, 9.4f, Sh));
			if (Wf > 0.001f)
			{
				FPoseContext IC(Output);
				SampleOnce(Idle, 0.f, IC);
				static const TCHAR* const Fingers[5] = { TEXT("thumb_0"), TEXT("index_0"), TEXT("middle_0"), TEXT("ring_0"), TEXT("pinky_0") };
				for (int32 F = 0; F < 5; ++F)
					for (int32 J = 1; J <= 3; ++J)
					{
						const FCompactPoseBoneIndex Fb = Bone(BC, FName(*FString::Printf(TEXT("%s%d_r"), Fingers[F], J)));
						if (!Fb.IsValid()) continue;
						FQuat Want = F == 1 ? BC.GetRefPoseTransform(Fb).GetRotation() : IC.Pose[Fb].GetRotation();
						if (F >= 2) Want = FQuat::Slerp(BC.GetRefPoseTransform(Fb).GetRotation(), Want, 1.4f).GetNormalized();   // a tighter fist than the grip
						Output.Pose[Fb].SetRotation(FQuat::Slerp(Output.Pose[Fb].GetRotation(), Want, Wf).GetNormalized());
					}
			}
			// 2) the arm: two-bone IK, hand above the shoulder and a little in over the chest, elbow out to the side,
			//    fingers turned to point up (world up); lying face down he points ahead of his head instead
			const float Wa = FMath::SmoothStep(0.3f, 1.6f, Sh) * (1.f - FMath::SmoothStep(7.4f, 9.0f, Sh));
			static const FName NUpRs(TEXT("upperarm_r")), NUpLs(TEXT("upperarm_l")), NLowRs(TEXT("lowerarm_r")), NHandRs(TEXT("hand_r"));
			static const FName NMidRs(TEXT("middle_01_r")), NSp5s(TEXT("spine_05")), NPelS(TEXT("pelvis")), NHeadS(TEXT("head"));
			const FCompactPoseBoneIndex UpRs = Bone(BC, NUpRs), UpLs = Bone(BC, NUpLs), LowRs = Bone(BC, NLowRs), HandRs = Bone(BC, NHandRs);
			const FCompactPoseBoneIndex MidRs = Bone(BC, NMidRs), Sp5s = Bone(BC, NSp5s), PelS = Bone(BC, NPelS), HeadS = Bone(BC, NHeadS);
			if (Wa > 0.001f && UpRs.IsValid() && UpLs.IsValid() && LowRs.IsValid() && HandRs.IsValid() && Sp5s.IsValid() && PelS.IsValid())
			{
				FCSPose<FCompactPose> CSd;
				CSd.InitPose(Output.Pose);
				const FVector UpW = GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
				const FVector Sh0 = CSd.GetComponentSpaceTransform(UpRs).GetLocation();
				const FVector ShL = CSd.GetComponentSpaceTransform(UpLs).GetLocation();
				const FVector Chest = CSd.GetComponentSpaceTransform(Sp5s).GetLocation();
				const FVector Pel = CSd.GetComponentSpaceTransform(PelS).GetLocation();
				const FTransform HandT = CSd.GetComponentSpaceTransform(HandRs);
				const double ArmLen = FVector::Dist(Sh0, CSd.GetComponentSpaceTransform(LowRs).GetLocation()) + FVector::Dist(CSd.GetComponentSpaceTransform(LowRs).GetLocation(), HandT.GetLocation());
				const FVector BodyRight = (Sh0 - ShL).GetSafeNormal();
				const FVector SpineDir = (Chest - Pel).GetSafeNormal();
				const FVector BodyFwd = FVector::CrossProduct(BodyRight, SpineDir).GetSafeNormal();
				const bool bFaceDown = FVector::DotProduct(BodyFwd, UpW) < -0.35;
				FVector PointDir = UpW;
				FVector Target;
				if (!bFaceDown)
				{
					FVector In2Chest = Chest - Sh0; In2Chest -= UpW * FVector::DotProduct(In2Chest, UpW);
					Target = Sh0 + UpW * (ArmLen * 0.80) + In2Chest * 0.30;
				}
				else
				{
					PointDir = (SpineDir - UpW * FVector::DotProduct(SpineDir, UpW)).GetSafeNormal();
					Target = Sh0 + PointDir * (ArmLen * 0.72) + UpW * 10.0;
				}
				// a faint tremble while he holds it
				Target += UpW * (double)(0.5f * FMath::Sin(Sh * 9.1f)) + BodyRight * (double)(0.35f * FMath::Sin(Sh * 6.7f + 1.3f));
				FTransform HandGoal = HandT;
				HandGoal.SetLocation(Target);
				if (MidRs.IsValid())
				{
					const FVector Fd = (CSd.GetComponentSpaceTransform(MidRs).GetLocation() - HandT.GetLocation()).GetSafeNormal();
					HandGoal.SetRotation((FQuat::FindBetweenNormals(Fd, PointDir) * HandT.GetRotation()).GetNormalized());
				}
				// the arm (two-bone IK, elbow out to his right side and a little down)
				FTransform U = CSd.GetComponentSpaceTransform(UpRs), L = CSd.GetComponentSpaceTransform(LowRs), E = HandT;
				const FVector Pole = (Sh0 + Target) * 0.5 + BodyRight * 40.0 - SpineDir * 12.0;
				AnimationCore::SolveTwoBoneIK(U, L, E, Pole, Target, false, 1.0, 1.0);
				E.SetRotation(HandGoal.GetRotation());
				TArray<FBoneTransform> Bt;
				Bt.Add(FBoneTransform(UpRs, U)); Bt.Add(FBoneTransform(LowRs, L)); Bt.Add(FBoneTransform(HandRs, E));
				CSd.LocalBlendCSBoneTransforms(Bt, Wa);
				// the head turns a little toward the raised hand
				if (HeadS.IsValid() && !bFaceDown)
				{
					FTransform Ht = CSd.GetComponentSpaceTransform(HeadS);
					const FVector ToHand = (Target - Ht.GetLocation()).GetSafeNormal();
					const FQuat Look = FQuat::FindBetweenNormals(BodyFwd, (BodyFwd * 0.6 + ToHand * 0.4).GetSafeNormal());
					Ht.SetRotation((Look * Ht.GetRotation()).GetNormalized());
					TArray<FBoneTransform> Bh; Bh.Add(FBoneTransform(HeadS, Ht));
					CSd.LocalBlendCSBoneTransforms(Bh, Wa * 0.7f);
				}
				FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSd), Output.Pose);
			}
		}
		return true;
	}

	static const FName NSpine1(TEXT("spine_01")), NSpine3(TEXT("spine_03")), NSpine4(TEXT("spine_04")), NSpine5(TEXT("spine_05"));
	static const FName NHandR(TEXT("hand_r")), NHandL(TEXT("hand_l")), NLowR(TEXT("lowerarm_r")), NLowL(TEXT("lowerarm_l"));
	static const FName NUpR(TEXT("upperarm_r")), NUpL(TEXT("upperarm_l")), NMidR(TEXT("middle_01_r")), NMidL(TEXT("middle_01_l"));
	const FCompactPoseBoneIndex Spine1 = Bone(BC, NSpine1);

	// Kar98k reload: the rifle stays where it is aimed (held by the left hand) while the right hand leaves the grip for the
	// magazine well - remember the rifle frame from the pose BEFORE the reload clip moves the arms
	if (false && In.bKarRifle && ReloadW > 0.01f && bCalibrated && !In.bUnarmed)   // (latched in the IK stage now, once, at the start of the reload)
	{
		const FCompactPoseBoneIndex Hr0 = Bone(BC, NHandR);
		if (Hr0.IsValid())
		{
			FCSPose<FCompactPose> P0;
			P0.InitPose(Output.Pose);
			KarW0 = GripRel * P0.GetComponentSpaceTransform(Hr0);
		}
	}

	// ---- reload / drawing the rifle on the upper body (spine_01 and everything under it)
	if (((ReloadW > 0.01f && Reload && !In.bKarRifle) || (EquipW > 0.01f && Equip)) && Spine1.IsValid())   // (the Kar98k loads through the open bolt: no magazine clip)
	{
		TArray<float> Mask;
		Mask.SetNumZeroed(Output.Pose.GetNumBones());
		for (const FCompactPoseBoneIndex Bi : Output.Pose.ForEachBoneIndex())
		{
			for (FCompactPoseBoneIndex P = Bi; P.IsValid(); P = Output.Pose.GetParentBoneIndex(P))
				if (P == Spine1) { Mask[Bi.GetInt()] = 1.f; break; }
		}
		FPoseContext RC(Output);
		if (ReloadW > 0.01f && Reload) { Sample(Reload, FMath::Max(In.ReloadTime, 0.f), RC); LerpPose(Output.Pose, RC.Pose, ReloadW, &Mask); }
		if (EquipW > 0.01f && Equip) { Sample(Equip, FMath::Max(In.EquipTime, 0.f), RC); LerpPose(Output.Pose, RC.Pose, EquipW, &Mask); }
	}
	// ---- flinch when hit (upper body, fades out over the clip)
	const UAnimSequence* HitSeq = PickHit(In.HitDir, In.HitStrength, In.HitIndex);
	if (!HitSeq) HitSeq = Hit[FMath::Abs(In.HitIndex) % 4];
	static const FName NClavL(TEXT("clavicle_l")), NClavR(TEXT("clavicle_r"));
	const FCompactPoseBoneIndex ClavL = Bone(BC, NClavL), ClavR = Bone(BC, NClavR);
	auto UnderArms = [&](FCompactPoseBoneIndex Bi) -> bool
	{
		if (!In.bUnarmed) return false;
		for (FCompactPoseBoneIndex P = Bi; P.IsValid(); P = Output.Pose.GetParentBoneIndex(P)) if (P == ClavL || P == ClavR) return true;
		return false;
	};
	if (In.HitTime >= 0.f && HitSeq && Spine1.IsValid() && In.HitTime < SeqLen(HitSeq))
	{
		FPoseContext HC(Output);
		Sample(HitSeq, In.HitTime, HC);
		const float K = In.HitTime / SeqLen(HitSeq);
		const float Weight = 0.85f * FMath::Min(1.f, K * 8.f) * (1.f - K);
		if (HitSeq->IsValidAdditive())
		{
			for (const FCompactPoseBoneIndex Bi : HC.Pose.ForEachBoneIndex())
			{
				bool bInUpper = false;
				for (FCompactPoseBoneIndex P = Bi; P.IsValid(); P = HC.Pose.GetParentBoneIndex(P))
				{
					if (P == Spine1) { bInUpper = true; break; }
				}
				if (!bInUpper || UnderArms(Bi))
				{
					HC.Pose[Bi].SetIdentityZeroScale();
				}
			}
			FAnimationPoseData BaseData(Output);
			const FAnimationPoseData AdditiveData(HC);
			FAnimationRuntime::AccumulateAdditivePose(BaseData, AdditiveData, Weight, HitSeq->AdditiveAnimType);
		}
		else
		{
			TArray<float> HMask;
			HMask.SetNumZeroed(Output.Pose.GetNumBones());
			for (const FCompactPoseBoneIndex Bi : Output.Pose.ForEachBoneIndex())
				for (FCompactPoseBoneIndex P = Bi; P.IsValid(); P = Output.Pose.GetParentBoneIndex(P))
					if (P == Spine1) { HMask[Bi.GetInt()] = UnderArms(Bi) ? 0.f : 1.f; break; }
			LerpPose(Output.Pose, HC.Pose, Weight, &HMask);
		}
	}

	const FCompactPoseBoneIndex HandR = Bone(BC, NHandR), HandL = Bone(BC, NHandL);
	const FCompactPoseBoneIndex LowR = Bone(BC, NLowR), LowL = Bone(BC, NLowL), UpR = Bone(BC, NUpR), UpL = Bone(BC, NUpL);
	if (!HandR.IsValid() || !HandL.IsValid()) return true;

	// character axes in component space
	const FTransform& CompT = GetComponentTransform();
	const FVector UpCS = CompT.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	FVector FwdCS = CompT.InverseTransformVectorNoScale(GetActorTransform().GetUnitAxis(EAxis::X));
	FwdCS = (FwdCS - UpCS * FVector::DotProduct(FwdCS, UpCS)).GetSafeNormal();

	// ---- calibration (once): where the rifle sits in the hands of the idle pose
	if (!bCalibrated && !In.bUnarmed)
	{
		FPoseContext Cal(Output);
		Sample(Idle, 0.f, Cal);
		FCSPose<FCompactPose> C0;
		C0.InitPose(Cal.Pose);
		const FVector PR = Palm(C0, HandR, Bone(BC, NMidR)), PL = Palm(C0, HandL, Bone(BC, NMidL));
		// the rifle points where the character aims (the idle pose is "aiming down sights"): barrel = character forward,
		// grip = right palm; only the distance to the left palm along the barrel comes from the hands
		const FVector X = FwdCS, Z = (UpCS - FwdCS * FVector::DotProduct(UpCS, FwdCS)).GetSafeNormal();
		const FVector Y = FVector::CrossProduct(Z, X);
		const FTransform W(FRotationMatrix::MakeFromXZ(X, Z).ToQuat(), PR);
		GripRel = W.GetRelativeTransform(C0.GetComponentSpaceTransform(HandR));
		LeftRel = C0.GetComponentSpaceTransform(HandL).GetRelativeTransform(W);
		const FVector D = PL - PR;
		LeftDist = FMath::Clamp((float)FVector::DotProduct(D, X), In.bIsPistol ? 0.5f : 15.f, 50.f);
		bCalibrated = true;
		// diagnostics (Output Log): hands and the animated weapon bone in character axes (forward / right / up)
		auto Fru = [&](const FVector& V) { return FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), FVector::DotProduct(V, X), FVector::DotProduct(V, Y), FVector::DotProduct(V, Z)); };
		static const FName NWeapon(TEXT("weapon_r"));
		const FCompactPoseBoneIndex Wb = Bone(BC, NWeapon);
		FString Wtxt = TEXT("none");
		if (Wb.IsValid())
		{
			const FTransform Wt = C0.GetComponentSpaceTransform(Wb);
			Wtxt = FString::Printf(TEXT("pos %s  X %s  Y %s  Z %s"), *Fru(Wt.GetLocation() - PR), *Fru(Wt.GetUnitAxis(EAxis::X)), *Fru(Wt.GetUnitAxis(EAxis::Y)), *Fru(Wt.GetUnitAxis(EAxis::Z)));
		}
		UE_LOG(LogTemp, Log, TEXT("AAA Weapons calibration: left palm - right palm %s cm, weapon_r %s"), *Fru(D), *Wtxt);
	}

	// ---- Pre-pass in Local Space: smoothly open curled fingers toward Reference Pose when touching a surface or climbing/mantling
	const float FingerOpenL = FMath::Max((In.bUnarmed && BinocularsWeight < 0.01f) ? In.TouchWeightL : 0.f, In.ClimbWeight * In.ClimbHandWeightL);
	const float FingerOpenR = FMath::Max((In.bUnarmed && BinocularsWeight < 0.01f) ? In.TouchWeightR : 0.f, In.ClimbWeight * In.ClimbHandWeightR);
	if (FingerOpenL > 0.005f || FingerOpenR > 0.005f)
	{
		auto OpenHandFingersLocal = [&](float Weight, bool bIsLeft)
		{
			if (Weight <= 0.005f) return;
			const TCHAR* Side = bIsLeft ? TEXT("_l") : TEXT("_r");
			const TCHAR* Prefixes[5] = { TEXT("thumb_0"), TEXT("index_0"), TEXT("middle_0"), TEXT("ring_0"), TEXT("pinky_0") };
			for (int32 F = 0; F < 5; ++F)
			{
				// Thumb opens 70% toward RefPose; 4 fingers open 92% toward RefPose for a clean, untwisted, flat open palm
				const float FingerAlpha = Weight * (F == 0 ? 0.70f : 0.92f);
				for (int32 J = 1; J <= 3; ++J)
				{
					const FName BoneName(*FString::Printf(TEXT("%s%d%s"), Prefixes[F], J, Side));
					const FCompactPoseBoneIndex BIdx = Bone(BC, BoneName);
					if (BIdx.IsValid())
					{
						const FQuat RefRot = BC.GetRefPoseTransform(BIdx).GetRotation();
						const FQuat CurRot = Output.Pose[BIdx].GetRotation();
						Output.Pose[BIdx].SetRotation(FQuat::Slerp(CurRot, RefRot, FingerAlpha));
					}
				}
			}
		};
		OpenHandFingersLocal(FingerOpenL, true);
		OpenHandFingersLocal(FingerOpenR, false);
	}

	// Relax leg twist bones toward Reference Pose in local space when crouching so skin never twists against IK legs
	if (false && CrouchW > 0.005f)   // (off: the crouch now keeps the walk clip's own legs, twist bones included)
	{
		const float TwistAlpha = FMath::Clamp(CrouchW, 0.f, 1.f) * 0.75f;
		static const FName NTwistBones[6] = {
			TEXT("thigh_twist_01_l"), TEXT("thigh_twist_01_r"),
			TEXT("calf_twist_01_l"),  TEXT("calf_twist_01_r"),
			TEXT("ball_l"),           TEXT("ball_r")
		};
		for (const FName& TName : NTwistBones)
		{
			const FCompactPoseBoneIndex TB = Bone(BC, TName);
			if (TB.IsValid())
			{
				const FQuat RefRot = BC.GetRefPoseTransform(TB).GetRotation();
				const FQuat CurRot = Output.Pose[TB].GetRotation();
				Output.Pose[TB].SetRotation(FQuat::Slerp(CurRot, RefRot, TwistAlpha).GetNormalized());
			}
		}
	}

	// climbing clip (hang -> pull up -> crouch on top), before everything that works in component space (IK)
	ApplyClimbClip(Output, FwdCS, UpCS);

	// ---- baked clips: grenade throw (spine + left arm), Kar98k fire / bolt cycle (upper body)
	CrouchClipW = 0.f; ThrowClipW = 0.f; SniperW = 0.f; bBoltOut = false; ClipDropCm = 0.f;
	if (In.ClimbWeight < 0.1f && In.DeathTime < 0.f)
	{
		if (In.ThrowTime >= 0.f && In.ThrowTime < 1.60f)
		{
			namespace C = AAAClipThrow;
			ThrowClipW = FMath::SmoothStep(0.f, 0.12f, In.ThrowTime) * (1.f - FMath::SmoothStep(1.35f, 1.60f, In.ThrowTime));
			const FBakedClipView V{ C::NumFrames, C::FPS, C::NumBones, C::Bone, C::DirChild, C::BindDir, &C::Delta[0][0][0] };
			ApplyBaked(Output, MapThrow, V, In.ThrowTime, ThrowClipW, 2, FwdCS, UpCS);
		}
		if (In.SniperFireTime >= 0.f && In.SniperFireTime < 1.45f && !In.bUnarmed)
		{
			// The Kar98k clip is NOT applied to the body any more: it was recorded in a bladed stance turned 90 degrees
			// to the right, so on this aim pose it threw the elbows up and the stock into the face (user video 23:02).
			// The body keeps the normal aim; only the right hand works the bolt (below), timed like the clip.
			SniperW = FMath::SmoothStep(0.f, 0.05f, In.SniperFireTime) * (1.f - FMath::SmoothStep(1.32f, 1.45f, In.SniperFireTime));
		}
	}

	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);

	// ---- Feet PLANTED on the ground while standing (idle, aiming, crouched, turning in place)  [rebuilt 2026-10-27]
	//      Each foot is held at its WORLD position and yaw (it no longer turns / slides with the capsule). When the body
	//      turns, or the pose wants the foot elsewhere, and a foot is more than 8 cm / 25 deg off, it STEPS there: one foot
	//      at a time, 0.24 s, a low arc; after the turn a small settle step squares the stance. Two-bone IK with the pole
	//      in the foot's own direction, so the knee follows the foot (no crossed legs). Fades out as soon as he walks.
	{
		static const FName NThighR(TEXT("thigh_r")), NThighL(TEXT("thigh_l"));
		static const FName NCalfR(TEXT("calf_r")), NCalfL(TEXT("calf_l"));
		static const FName NFootR(TEXT("foot_r")), NFootL(TEXT("foot_l"));
		const FCompactPoseBoneIndex Th[2] = { Bone(BC, NThighL), Bone(BC, NThighR) };
		const FCompactPoseBoneIndex Ca[2] = { Bone(BC, NCalfL), Bone(BC, NCalfR) };
		const FCompactPoseBoneIndex Fo[2] = { Bone(BC, NFootL), Bone(BC, NFootR) };
		const bool bBones = Th[0].IsValid() && Th[1].IsValid() && Ca[0].IsValid() && Ca[1].IsValid() && Fo[0].IsValid() && Fo[1].IsValid();
		const float Dt = FMath::Clamp(LastDt, 0.f, 0.05f);
		const bool bOk = bBones && In.ClimbWeight < 0.1f && !In.bMantling && !In.bClimbing && In.DeathTime < 0.f;
		const float WantW = bOk ? (1.f - FMath::Clamp(MoveWeight * 1.6f, 0.f, 1.f)) * (1.f - FallW) : 0.f;
		PlantWeight = FMath::FInterpTo(PlantWeight, WantW, Dt, WantW > PlantWeight ? 6.f : 14.f);
		if (bOk)
		{
			FTransform HomeCS[2], HomeW[2];
			for (int32 i = 0; i < 2; ++i) { HomeCS[i] = CS.GetComponentSpaceTransform(Fo[i]); HomeW[i] = HomeCS[i] * CompT; }
			if (!bPlantInit || PlantWeight < 0.02f)
			{
				for (int32 i = 0; i < 2; ++i) { PlantW[i] = HomeW[i]; StepT[i] = -1.f; }
				bPlantInit = true; SettleT = 0.f;
			}
			else
			{
				auto Score = [&](int32 i) -> double
				{
					const double D = FVector::Dist2D(PlantW[i].GetLocation(), HomeW[i].GetLocation());
					const double A = FMath::RadiansToDegrees(PlantW[i].GetRotation().AngularDistance(HomeW[i].GetRotation()));
					return FMath::Max(D / 8.0, A / 25.0);
				};
				SettleT += Dt;
				if (StepT[0] < 0.f && StepT[1] < 0.f)
				{
					const double S0 = Score(0), S1 = Score(1);
					const double Need = SettleT > 0.35f ? 0.30 : 1.0;   // after a pause: a small settle step squares the stance
					const int32 i = S0 >= S1 ? 0 : 1;
					if (FMath::Max(S0, S1) > Need) { StepFromW[i] = PlantW[i]; StepT[i] = 0.f; }
				}
				for (int32 i = 0; i < 2; ++i)
				{
					if (StepT[i] < 0.f) continue;
					StepT[i] = FMath::Min(StepT[i] + Dt / 0.24f, 1.f);
					if (StepT[i] >= 1.f) { PlantW[i] = HomeW[i]; StepT[i] = -1.f; SettleT = 0.f; }
				}
			}
			const FVector RightP = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();
			for (int32 i = 0; i < 2; ++i)
			{
				FTransform TW = PlantW[i];
				if (StepT[i] >= 0.f)
				{
					const float K = FMath::SmoothStep(0.f, 1.f, StepT[i]);
					const double Span = FVector::Dist2D(StepFromW[i].GetLocation(), HomeW[i].GetLocation());
					TW.SetLocation(FMath::Lerp(StepFromW[i].GetLocation(), HomeW[i].GetLocation(), (double)K)
						+ FVector::UpVector * (FMath::Clamp(Span / 10.0, 0.4, 1.0) * 5.0 * FMath::Sin(StepT[i] * UE_PI)));
					TW.SetRotation(FQuat::Slerp(StepFromW[i].GetRotation(), HomeW[i].GetRotation(), K).GetNormalized());
				}
				else
				{
					FVector L = TW.GetLocation();
					L.Z = HomeW[i].GetLocation().Z;   // height always from the pose (crouch / ground IK / steps of the clip)
					TW.SetLocation(L);
				}
				const FTransform TCS = TW.GetRelativeTransform(CompT);
				const FQuat Twist = TCS.GetRotation() * HomeCS[i].GetRotation().Inverse();
				FVector F = Twist.RotateVector(FwdCS);
				F = (F - UpCS * FVector::DotProduct(F, UpCS)).GetSafeNormal();
				const FVector Out = i == 0 ? -RightP : RightP;
				const FVector KneeDirP = (F * 0.92 + Out * 0.12 + UpCS * 0.15).GetSafeNormal();
				LegIK(CS, Th[i], Ca[i], Fo[i], TCS, KneeDirP, PlantWeight);
			}
		}
		else bPlantInit = false;
	}

	static const FName NHead(TEXT("head"));
	const FCompactPoseBoneIndex HeadB = Bone(BC, NHead);
	const FVector Axis = FVector::CrossProduct(FwdCS, UpCS).GetSafeNormal();

	// ---- Two-Hand IK for Jumelle (Binoculars) - works both armed and unarmed
	if (BinocularsWeight > 0.005f && UpR.IsValid() && LowR.IsValid() && HandR.IsValid() && UpL.IsValid() && LowL.IsValid() && HandL.IsValid())
	{
		const FTransform HeadCS = HeadB.IsValid() ? CS.GetComponentSpaceTransform(HeadB) : CS.GetComponentSpaceTransform(Spine1);
		const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();

		// Position at eye level in front of the face
		const FVector EyeLoc = HeadCS.GetLocation() + FwdCS * 12.0f + UpCS * 2.0f;
		const FVector RightGrip = EyeLoc + RightCS * 4.4f - FwdCS * 3.0f - UpCS * 2.5f;
		const FVector LeftGrip = EyeLoc - RightCS * 4.4f - FwdCS * 3.0f - UpCS * 2.5f;

		// Grip orientation: hands rotated inwards, fingers wrapping around binoculars
		const FQuat BaseGrip = FRotationMatrix::MakeFromXZ(FwdCS, UpCS).ToQuat();
		const FQuat RightRot = BaseGrip * FQuat(FwdCS, FMath::DegreesToRadians(-35.f)) * FQuat(UpCS, FMath::DegreesToRadians(-20.f));
		const FQuat LeftRot = BaseGrip * FQuat(FwdCS, FMath::DegreesToRadians(35.f)) * FQuat(UpCS, FMath::DegreesToRadians(20.f));

		ArmIK(CS, UpR, LowR, HandR, FTransform(RightRot, RightGrip), BinocularsWeight, false);
		ArmIK(CS, UpL, LowL, HandL, FTransform(LeftRot, LeftGrip), BinocularsWeight, true);
	}

	// ---- Adaptive Ground / Slope / Vehicle-Roof Foot, Ankle & Pelvis IK (Conforms legs & hips to slopes, stairs, and curbs without squatting on flat surfaces)
	const float CrW = FMath::Clamp(CrouchW, 0.f, 1.08f) * (1.f - FMath::Clamp(In.ClimbWeight * 2.f, 0.f, 1.f)) * (1.f - FallW) * (In.DeathTime >= 0.f ? 0.f : 1.f);
	const float LnW = LeanW * (1.f - FMath::Clamp(In.ClimbWeight * 2.f, 0.f, 1.f)) * (In.DeathTime >= 0.f ? 0.f : 1.f);
	const bool bInCrouchOrLean = (CrW > 0.005f || FMath::Abs(LnW) > 0.005f);

	if (In.GroundFootWeight > 0.005f && In.ClimbWeight < 0.5f && FallW < 0.25f)
	{
		const float GW = FMath::Clamp(In.GroundFootWeight * (1.f - In.ClimbWeight * 2.f) * (1.f - FallW), 0.f, 1.f);
		static const FName NPelvisG(TEXT("pelvis"));
		static const FName NThighRG(TEXT("thigh_r")), NThighLG(TEXT("thigh_l"));
		static const FName NCalfRG(TEXT("calf_r")), NCalfLG(TEXT("calf_l"));
		static const FName NFootRG(TEXT("foot_r")), NFootLG(TEXT("foot_l"));

		const FCompactPoseBoneIndex PelvisG = Bone(BC, NPelvisG);
		const FCompactPoseBoneIndex ThighR = Bone(BC, NThighRG), ThighL = Bone(BC, NThighLG);
		const FCompactPoseBoneIndex CalfR = Bone(BC, NCalfRG), CalfL = Bone(BC, NCalfLG);
		const FCompactPoseBoneIndex FootR = Bone(BC, NFootRG), FootL = Bone(BC, NFootLG);
		const FCompactPoseBoneIndex Sp3G = Bone(BC, NSpine3);
		const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();

		if (ThighR.IsValid() && CalfR.IsValid() && FootR.IsValid() && ThighL.IsValid() && CalfL.IsValid() && FootL.IsValid())
		{
			const FTransform OrigFootL = CS.GetComponentSpaceTransform(FootL);
			const FTransform OrigFootR = CS.GetComponentSpaceTransform(FootR);

			const float FootDiff = In.GroundOffsetL - In.GroundOffsetR;
			const float HipRollDeg = FMath::Clamp(FootDiff * 0.18f, -6.0f, 6.0f);

			if (PelvisG.IsValid())
			{
				FTransform PT = CS.GetComponentSpaceTransform(PelvisG);
				PT.AddToTranslation(UpCS * (double)(In.GroundPelvisOffsetZ * GW));
				PT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(HipRollDeg * GW)) * PT.GetRotation());
				SetCS(CS, PelvisG, PT, GW);
			}
			if (Sp3G.IsValid() && FMath::Abs(HipRollDeg) > 0.1f)
			{
				FTransform ST = CS.GetComponentSpaceTransform(Sp3G);
				ST.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-HipRollDeg * 0.70f * GW)) * ST.GetRotation());
				SetCS(CS, Sp3G, ST, GW);
			}

			// Only solve LegIK here if Crouch IK won't solve it immediately after (avoid double-solving on crouched legs)
			if (!bInCrouchOrLean)
			{
				auto SolveAdaptiveGroundFoot = [&](FCompactPoseBoneIndex ThighB, FCompactPoseBoneIndex CalfB, FCompactPoseBoneIndex FootB, const FTransform& BaseFoot, float ZOffset, const FVector& WorldNorm, bool bIsLeft)
				{
					FTransform TargetFoot = BaseFoot;
					TargetFoot.AddToTranslation(UpCS * (double)(ZOffset * GW));

					FVector NormCS = CompT.InverseTransformVectorNoScale(WorldNorm).GetSafeNormal();
					if (NormCS.Z < 0.55) NormCS = (NormCS + UpCS * 1.5).GetSafeNormal();
					const FQuat SlopeQ = FQuat::FindBetweenNormals(UpCS, NormCS);
					TargetFoot.SetRotation(FQuat::Slerp(FQuat::Identity, SlopeQ, GW * 0.80f) * BaseFoot.GetRotation());

					const FVector OutSide = bIsLeft ? -RightCS : RightCS;
					const FVector KneeDir = (FwdCS * 0.92 + OutSide * 0.12 + UpCS * 0.15).GetSafeNormal();
					LegIK(CS, ThighB, CalfB, FootB, TargetFoot, KneeDir, GW);
				};

				SolveAdaptiveGroundFoot(ThighL, CalfL, FootL, OrigFootL, In.GroundOffsetL, In.GroundNormL, true);
				SolveAdaptiveGroundFoot(ThighR, CalfR, FootR, OrigFootR, In.GroundOffsetR, In.GroundNormR, false);
			}
		}
	}

	// ---- CROUCH = the SAME walk / idle motion, only lower (v3, 2026-10-27) + corner lean
	//      The clip's pose is kept as it is (steps, direction, timing, foot roll, arms, weapon). Only the hips go down by
	//      h = GCrouchDrop * CrouchW and every leg is re-solved with two-bone IK onto the SAME foot transform the clip had:
	//        hip-to-ankle distance  d' = |(Hip - h*Up) - Ankle|
	//        knee angle (law of cosines)  cos(K) = (a^2 + b^2 - d'^2) / (2ab)   (a = thigh, b = shin)
	//      so the knees bend just enough for the drop, the knee plane stays the clip's own (pole = the clip's knee
	//      direction), and the feet stay planted exactly where the walk put them (no sliding, no new stride).
	//      The chest keeps its orientation (aim / weapon / hands IK unchanged).
	if (bInCrouchOrLean)
	{
		static const FName NPelvisC(TEXT("pelvis")), NSpine2C(TEXT("spine_02"));
		static const FName NThighRC(TEXT("thigh_r")), NThighLC(TEXT("thigh_l"));
		static const FName NCalfRC(TEXT("calf_r")), NCalfLC(TEXT("calf_l"));
		static const FName NFootRC(TEXT("foot_r")), NFootLC(TEXT("foot_l"));
		const FCompactPoseBoneIndex PelvisC = Bone(BC, NPelvisC), Spine2C = Bone(BC, NSpine2C);
		const FCompactPoseBoneIndex ThighRC = Bone(BC, NThighRC), ThighLC = Bone(BC, NThighLC);
		const FCompactPoseBoneIndex CalfRC = Bone(BC, NCalfRC), CalfLC = Bone(BC, NCalfLC);
		const FCompactPoseBoneIndex FootRC = Bone(BC, NFootRC), FootLC = Bone(BC, NFootLC);
		const FVector RightC = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();

		if (PelvisC.IsValid() && ThighRC.IsValid() && CalfRC.IsValid() && FootRC.IsValid() && ThighLC.IsValid() && CalfLC.IsValid() && FootLC.IsValid())
		{
			const float Cw = FMath::Min(CrW, 1.f);

			// the clip's legs before anything moves: foot targets and each knee's bend direction
			FTransform FL = CS.GetComponentSpaceTransform(FootLC);
			FTransform FR = CS.GetComponentSpaceTransform(FootRC);
			auto KneeDir = [&](FCompactPoseBoneIndex Th, FCompactPoseBoneIndex Ca, FCompactPoseBoneIndex Fo, bool bIsLeft) -> FVector
			{
				const FVector H = CS.GetComponentSpaceTransform(Th).GetLocation();
				const FVector K = CS.GetComponentSpaceTransform(Ca).GetLocation();
				const FVector A = CS.GetComponentSpaceTransform(Fo).GetLocation();
				const FVector Ax = (A - H).GetSafeNormal();
				FVector B = (K - H) - Ax * FVector::DotProduct(K - H, Ax);   // knee offset off the hip-ankle line
				if (B.SizeSquared() < 1.0) B = FwdCS + (bIsLeft ? -RightC : RightC) * 0.1;   // straight leg: knee forward
				return B.GetSafeNormal();
			};
			const FVector KneeL = KneeDir(ThighLC, CalfLC, FootLC, true);
			const FVector KneeR = KneeDir(ThighRC, CalfRC, FootRC, false);

			// standing still crouched: both soles flat on the ground (the idle / aim clips lift a heel); walking keeps the
			// clip's heel-toe roll. Flat = the reference-pose foot turned to the clip foot's yaw.
			{
				static const FName NBallLC(TEXT("ball_l")), NBallRC(TEXT("ball_r"));
				const FCompactPoseBoneIndex BallLC = Bone(BC, NBallLC), BallRC = Bone(BC, NBallRC);
				const float Still = 1.f - FMath::Clamp(MoveWeight, 0.f, 1.f);
				auto Flatten = [&](FTransform& F, FCompactPoseBoneIndex FoB, FCompactPoseBoneIndex BaB)
				{
					if (!BaB.IsValid() || Still * Cw < 0.01f) return;
					FTransform RefF = BC.GetRefPoseTransform(FoB);   // reference foot in component space
					for (FCompactPoseBoneIndex P = Output.Pose.GetParentBoneIndex(FoB); P.IsValid(); P = Output.Pose.GetParentBoneIndex(P))
						RefF = RefF * BC.GetRefPoseTransform(P);
					const FTransform RefB = BC.GetRefPoseTransform(BaB) * RefF;   // (ball is a child of foot)
					auto Hz = [&](const FVector& V) { return (V - UpCS * FVector::DotProduct(V, UpCS)).GetSafeNormal(); };
					const FVector RefDir = Hz(RefB.GetLocation() - RefF.GetLocation());
					const FVector NowDir = Hz(CS.GetComponentSpaceTransform(BaB).GetLocation() - CS.GetComponentSpaceTransform(FoB).GetLocation());
					if (RefDir.IsNearlyZero() || NowDir.IsNearlyZero()) return;
					const FQuat Flat = FQuat::FindBetweenNormals(RefDir, NowDir) * RefF.GetRotation();
					F.SetRotation(FQuat::Slerp(F.GetRotation(), Flat, Still * Cw).GetNormalized());
				};
				Flatten(FL, FootLC, BallLC);
				Flatten(FR, FootRC, BallRC);
			}

			// ground / slope (the standing ground IK is skipped while crouched, it is done here)
			if (In.GroundFootWeight > 0.005f)
			{
				const float GW = FMath::Clamp(In.GroundFootWeight * (1.f - In.ClimbWeight * 2.f) * (1.f - FallW), 0.f, 1.f);
				FL.AddToTranslation(UpCS * (double)(In.GroundOffsetL * GW));
				FR.AddToTranslation(UpCS * (double)(In.GroundOffsetR * GW));
				auto ApplySlope = [&](FTransform& FootTarget, const FVector& WorldNorm)
				{
					FVector NormCS = CompT.InverseTransformVectorNoScale(WorldNorm).GetSafeNormal();
					if (NormCS.Z < 0.55) NormCS = (NormCS + UpCS * 1.5).GetSafeNormal();
					const FQuat SlopeQ = FQuat::FindBetweenNormals(UpCS, NormCS);
					FootTarget.SetRotation(FQuat::Slerp(FQuat::Identity, SlopeQ, GW * 0.80f) * FootTarget.GetRotation());
				};
				ApplySlope(FL, In.GroundNormL);
				ApplySlope(FR, In.GroundNormR);
			}

			// hips: down h, a small forward tip; lean shift
			const FTransform ChestBefore = Spine1.IsValid() ? CS.GetComponentSpaceTransform(Spine1) : FTransform::Identity;
			FTransform PT = CS.GetComponentSpaceTransform(PelvisC);
			PT.SetRotation(FQuat(Axis, FMath::DegreesToRadians(-GCrouchTip * Cw)) * PT.GetRotation());
			PT.AddToTranslation(-UpCS * (double)(GCrouchDrop * CrW) + RightC * (double)(9.f * LnW));
			// hip HEIGHT guard: measured from the planted ankles, the hips are never lower than the standing height
			// (reference pose) minus the drop (+4 cm for the walk's own bob) - whatever else lowered the pose before
			const double AnkleZ = FMath::Min(FVector::DotProduct(FL.GetLocation(), UpCS), FVector::DotProduct(FR.GetLocation(), UpCS));
			double StandH = 0.0;
			{
				FTransform RefP = BC.GetRefPoseTransform(PelvisC), RefF = BC.GetRefPoseTransform(FootLC);
				for (FCompactPoseBoneIndex P = Output.Pose.GetParentBoneIndex(PelvisC); P.IsValid(); P = Output.Pose.GetParentBoneIndex(P)) RefP = RefP * BC.GetRefPoseTransform(P);
				for (FCompactPoseBoneIndex P = Output.Pose.GetParentBoneIndex(FootLC); P.IsValid(); P = Output.Pose.GetParentBoneIndex(P)) RefF = RefF * BC.GetRefPoseTransform(P);
				StandH = FVector::DotProduct(RefP.GetLocation() - RefF.GetLocation(), UpCS);
			}
			if (StandH > 40.0)
			{
				const double MinZ = AnkleZ + StandH - (double)(GCrouchDrop * CrW) - 4.0;
				const double NowZ = FVector::DotProduct(PT.GetLocation(), UpCS);
				if (NowZ < MinZ) PT.AddToTranslation(UpCS * (MinZ - NowZ));
			}
			SetCS(CS, PelvisC, PT);
			// diagnostics (once a second while crouched): hip height above the ankles, and where the mesh sits in the capsule
			{
				static double LastLog = 0.0;
				const double Now = FPlatformTime::Seconds();
				if (Cw > 0.5f && Now - LastLog > 1.0)
				{
					LastLog = Now;
					UE_LOG(LogTemp, Log, TEXT("AAA crouch: CrW %.2f  hip above ankle %.1f (standing %.1f, drop %.1f)  mesh-actor Z %.1f  move %.2f"),
						CrW, FVector::DotProduct(PT.GetLocation(), UpCS) - AnkleZ, StandH, GCrouchDrop * CrW,
						CompT.GetLocation().Z - GetActorTransform().GetLocation().Z, MoveWeight);
				}
			}

			// chest back to its orientation (net zero over the spine: the weapon does not move with the hips)
			if (Spine1.IsValid())
			{
				FTransform ST = CS.GetComponentSpaceTransform(Spine1);
				ST.SetRotation(ChestBefore.GetRotation());
				SetCS(CS, Spine1, ST);
			}
			const FCompactPoseBoneIndex SpC[5] = { Spine1, Spine2C, Bone(BC, NSpine3), Bone(BC, NSpine4), Bone(BC, NSpine5) };
			static const float LnRollW[5] = { 0.12f, 0.18f, 0.22f, 0.24f, 0.24f };
			const float LeanDeg = 17.f * LnW;
			if (FMath::Abs(LnW) > 0.005f)
			{
				for (int32 K = 0; K < 5; ++K)
				{
					if (!SpC[K].IsValid()) continue;
					FTransform T = CS.GetComponentSpaceTransform(SpC[K]);
					T.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-LeanDeg * LnRollW[K])) * T.GetRotation());
					SetCS(CS, SpC[K], T);
				}
				if (HeadB.IsValid())
				{
					FTransform Ht = CS.GetComponentSpaceTransform(HeadB);
					Ht.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(LeanDeg * 0.55f)) * Ht.GetRotation());
					SetCS(CS, HeadB, Ht);
				}
			}

			// unarmed sneak (reference image): torso upright with a small forward lean, head level, arms a little forward
			if (In.bUnarmed && Cw > 0.01f)
			{
				static const float SneakW[5] = { 0.30f, 0.25f, 0.20f, 0.15f, 0.10f };
				for (int32 Si = 0; Si < 5; ++Si)
				{
					if (!SpC[Si].IsValid()) continue;
					FTransform Ts = CS.GetComponentSpaceTransform(SpC[Si]);
					Ts.SetRotation(FQuat(Axis, FMath::DegreesToRadians(-GSneakLean * Cw * SneakW[Si])) * Ts.GetRotation());
					SetCS(CS, SpC[Si], Ts);
				}
				if (HeadB.IsValid())
				{
					FTransform Hs = CS.GetComponentSpaceTransform(HeadB);
					Hs.SetRotation(FQuat(Axis, FMath::DegreesToRadians(GSneakLean * 0.8f * Cw)) * Hs.GetRotation());
					SetCS(CS, HeadB, Hs);
				}
				for (const FCompactPoseBoneIndex Ab : { UpL, UpR })
				{
					if (!Ab.IsValid()) continue;
					FTransform Ta = CS.GetComponentSpaceTransform(Ab);
					Ta.SetRotation(FQuat(Axis, FMath::DegreesToRadians(8.f * Cw)) * Ta.GetRotation());   // (+ = hanging arm swings forward)
					SetCS(CS, Ab, Ta);
				}
			}

			// legs: two-bone IK of the lowered chain onto the clip's own foot transform, in the clip's knee plane
			auto SolveLeg = [&](FCompactPoseBoneIndex ThB, FCompactPoseBoneIndex CaB, FCompactPoseBoneIndex FoB, const FTransform& Target, const FVector& Knee)
			{
				FTransform U = CS.GetComponentSpaceTransform(ThB);
				FTransform L = CS.GetComponentSpaceTransform(CaB);
				FTransform E = CS.GetComponentSpaceTransform(FoB);
				const double A = FVector::Dist(U.GetLocation(), L.GetLocation()), B = FVector::Dist(L.GetLocation(), E.GetLocation());
				FVector T = Target.GetLocation();
				const FVector D = T - U.GetLocation();
				const double Len = D.Size(), MaxLen = (A + B) * 0.998;
				if (Len > MaxLen) T = U.GetLocation() + D / Len * MaxLen;   // (only if the clip's foot is out of reach)
				const FVector Pole = (U.GetLocation() + T) * 0.5 + Knee * 60.0;
				AnimationCore::SolveTwoBoneIK(U, L, E, Pole, T, false, 1.0, 1.0);
				E.SetRotation(Target.GetRotation());   // the clip's foot roll / pitch (+ slope)
				TArray<FBoneTransform> Bt;
				Bt.Add(FBoneTransform(ThB, U));
				Bt.Add(FBoneTransform(CaB, L));
				Bt.Add(FBoneTransform(FoB, E));
				CS.LocalBlendCSBoneTransforms(Bt, 1.f);
			};
			SolveLeg(ThighLC, CalfLC, FootLC, FL, KneeL);
			SolveLeg(ThighRC, CalfRC, FootRC, FR, KneeR);
		}
	}

	// ---- Mantling / wall-climbing IK: body choreography + feet on the holds + hands planted
	//      (bMantling: vault / pull-up onto a ledge; otherwise free climbing on a wall)
	if ((In.bMantling || In.bClimbing) && In.ClimbWeight > 0.005f)
	{
		const bool bWallMode = !In.bMantling;
		const float CW = FMath::Clamp(In.ClimbWeight, 0.f, 1.f);
		const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();

		static const FName NPelvisM(TEXT("pelvis"));
		static const FName NThighRM(TEXT("thigh_r")), NThighLM(TEXT("thigh_l"));
		static const FName NCalfRM(TEXT("calf_r")), NCalfLM(TEXT("calf_l"));
		static const FName NFootRM(TEXT("foot_r")), NFootLM(TEXT("foot_l"));
		static const FName NBallRM(TEXT("ball_r")), NBallLM(TEXT("ball_l"));
		const FCompactPoseBoneIndex PelvisM = Bone(BC, NPelvisM);
		const FCompactPoseBoneIndex ThighRM = Bone(BC, NThighRM), ThighLM = Bone(BC, NThighLM);
		const FCompactPoseBoneIndex CalfRM = Bone(BC, NCalfRM), CalfLM = Bone(BC, NCalfLM);
		const FCompactPoseBoneIndex FootRM = Bone(BC, NFootRM), FootLM = Bone(BC, NFootLM);
		const FCompactPoseBoneIndex BallRM = Bone(BC, NBallRM), BallLM = Bone(BC, NBallLM);

		// 1a) Wall: hips in, chest slightly off the wall, weight shifting toward the reaching hand,
		//     head tilted up to look for the next hold (down when climbing down is the same pose)
		if (bWallMode)
		{
			const float Sh = FMath::Clamp(In.ClimbPhase, -1.f, 1.f);
			const float Mv = FMath::Clamp(In.ClimbMoveSpeed, 0.f, 1.f);
			if (PelvisM.IsValid())
			{
				FTransform PT = CS.GetComponentSpaceTransform(PelvisM);
				PT.AddToTranslation(RightCS * (double)(Sh * 4.f * CW) + FwdCS * (double)(8.f * CW));   // hips pressed in to the wall
				PT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(Sh * 6.f * CW)) * FQuat(UpCS, FMath::DegreesToRadians(-Sh * 4.f * CW)) * PT.GetRotation());
				SetCS(CS, PelvisM, PT, CW);
			}
			const FCompactPoseBoneIndex SpW[3] = { Bone(BC, NSpine3), Bone(BC, NSpine4), Bone(BC, NSpine5) };
			for (int32 S = 0; S < 3; ++S)
			{
				if (!SpW[S].IsValid()) continue;
				FTransform ST = CS.GetComponentSpaceTransform(SpW[S]);
				// counter-roll of the chest + a slight lean back so the arms have room to reach
				ST.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-Sh * 3.f * CW)) * FQuat(RightCS, FMath::DegreesToRadians((3.f - Mv * 1.f) * CW)) * ST.GetRotation());
				SetCS(CS, SpW[S], ST, CW);
			}
			if (HeadB.IsValid())
			{
				FTransform HT = CS.GetComponentSpaceTransform(HeadB);
				HT.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(Sh * 7.f * CW)) * FQuat(RightCS, FMath::DegreesToRadians(-7.f * CW)) * HT.GetRotation());
				SetCS(CS, HeadB, HT, CW);
			}
		}

		// 1) Body: hips and spine pitch forward over the ledge while pulling, then straighten up
		const float MP = FMath::Clamp(In.MantleProgress, 0.f, 1.f);
		const float Lean = (bWallMode ? 0.f : FMath::Sin(FMath::Clamp(MP / 0.85f, 0.f, 1.f) * UE_PI)) * (1.f - FMath::Clamp(In.ClimbClipWeight, 0.f, 1.f));
		if (!bWallMode && PelvisM.IsValid())
		{
			FTransform PT = CS.GetComponentSpaceTransform(PelvisM);
			PT.SetRotation(FQuat(RightCS, FMath::DegreesToRadians(8.f * Lean * CW)) * PT.GetRotation());
			SetCS(CS, PelvisM, PT, CW);
		}
		const FCompactPoseBoneIndex SpM[3] = { Bone(BC, NSpine3), Bone(BC, NSpine4), Bone(BC, NSpine5) };
		for (int32 S = 0; S < 3 && !bWallMode; ++S)
		{
			if (!SpM[S].IsValid()) continue;
			FTransform ST = CS.GetComponentSpaceTransform(SpM[S]);
			ST.SetRotation(FQuat(RightCS, FMath::DegreesToRadians(8.f * Lean * CW)) * ST.GetRotation());
			SetCS(CS, SpM[S], ST, CW);
		}
		if (!bWallMode && HeadB.IsValid())
		{
			FTransform HT = CS.GetComponentSpaceTransform(HeadB);
			HT.SetRotation(FQuat(RightCS, FMath::DegreesToRadians(-16.f * Lean * CW)) * HT.GetRotation());
			SetCS(CS, HeadB, HT, CW);
		}

		// 2) Feet: ankles are placed above the sole contact point; wall footholds tilt the sole toward the wall
		auto ApplyMantleFoot = [&](FCompactPoseBoneIndex ThighB, FCompactPoseBoneIndex CalfB, FCompactPoseBoneIndex FootB, FCompactPoseBoneIndex BallB,
			const FVector& WorldPos, const FVector& WorldNorm, float LimbW, bool bIsLeft)
		{
			const float W = FMath::Clamp(CW * LimbW, 0.f, 1.f);
			if (W <= 0.005f || !ThighB.IsValid() || !CalfB.IsValid() || !FootB.IsValid()) return;

			const FTransform IdleFootT = CS.GetComponentSpaceTransform(FootB);
			FVector NormCS = CompT.InverseTransformVectorNoScale(WorldNorm).GetSafeNormal();
			if (NormCS.IsNearlyZero()) NormCS = UpCS;
			const float Horizontal = (float)FMath::Clamp(FVector::DotProduct(NormCS, UpCS), 0.0, 1.0);

			FVector TargetCS = CompT.InverseTransformPosition(WorldPos) + NormCS * (bWallMode ? 11.0 : 9.0) + (bWallMode ? UpCS * 4.0 : FVector::ZeroVector);

			// never exceed the real leg length
			const FVector HipLoc = CS.GetComponentSpaceTransform(ThighB).GetLocation();
			const FVector LegDelta = TargetCS - HipLoc;
			const double LegDist = LegDelta.Size();
			if (LegDist > 84.0) TargetCS = HipLoc + (LegDelta / LegDist) * 84.0;

			FTransform FootTarget = IdleFootT;
			FootTarget.SetLocation(TargetCS);
			const FQuat SoleAlign = FQuat::FindBetweenNormals(UpCS, NormCS);
			const float AlignAlpha = bWallMode ? FMath::Lerp(0.12f, 0.9f, Horizontal) : FMath::Lerp(0.45f, 0.9f, Horizontal);   // on the toes, foot into the wall
			FootTarget.SetRotation(FQuat::Slerp(FQuat::Identity, SoleAlign, AlignAlpha) * IdleFootT.GetRotation());

			const FVector OutSide = bIsLeft ? -RightCS : RightCS;
			// a foot on the wall face (normal ~horizontal): the knee goes up and out, not forward into the wall
			const bool bFootOnWall = Horizontal < 0.5f;
			const FVector KneeDir = bFootOnWall ? (UpCS * 0.65 + OutSide * 0.6 - FwdCS * 0.15).GetSafeNormal()
				: (FwdCS * 0.8 + OutSide * 0.35 + UpCS * 0.25).GetSafeNormal();
			LegIK(CS, ThighB, CalfB, FootB, FootTarget, KneeDir, W);

			if (BallB.IsValid() && Horizontal < 0.5f)
			{
				FTransform BT = CS.GetComponentSpaceTransform(BallB);
				BT.SetRotation(FQuat(Axis, FMath::DegreesToRadians(-20.f * W)) * BT.GetRotation());
				SetCS(CS, BallB, BT, W);
			}
		};
		ApplyMantleFoot(ThighLM, CalfLM, FootLM, BallLM, In.ClimbFootPosL, In.ClimbFootNormL, In.ClimbFootWeightL, true);
		ApplyMantleFoot(ThighRM, CalfRM, FootRM, BallRM, In.ClimbFootPosR, In.ClimbFootNormR, In.ClimbFootWeightR, false);

		// Two-Bone Hand IK with 3D Anatomical Palm Alignment (pressing flat on ledge/roof)
		auto ApplyClimbHandIK = [&](FCompactPoseBoneIndex UpArm, FCompactPoseBoneIndex LowArm, FCompactPoseBoneIndex HandB, const FVector& WorldPos, const FVector& WorldNorm, float LimbW, bool bIsLeft)
		{
			float W = FMath::Clamp(CW * LimbW, 0.f, 1.f);
			if (W <= 0.005f || !UpArm.IsValid() || !LowArm.IsValid() || !HandB.IsValid()) return;

			const FCompactPoseBoneIndex Idx2 = Bone(BC, bIsLeft ? FName(TEXT("index_02_l"))  : FName(TEXT("index_02_r")));
			const FCompactPoseBoneIndex Mid3 = Bone(BC, bIsLeft ? FName(TEXT("middle_03_l")) : FName(TEXT("middle_03_r")));
			const FCompactPoseBoneIndex Pnk2 = Bone(BC, bIsLeft ? FName(TEXT("pinky_02_l"))  : FName(TEXT("pinky_02_r")));
			const FCompactPoseBoneIndex Twist1 = Bone(BC, bIsLeft ? FName(TEXT("lowerarm_twist_01_l")) : FName(TEXT("lowerarm_twist_01_r")));
			const FCompactPoseBoneIndex Twist2 = Bone(BC, bIsLeft ? FName(TEXT("lowerarm_twist_02_l")) : FName(TEXT("lowerarm_twist_02_r")));

			FVector HitCS = CompT.InverseTransformPosition(WorldPos);
			FVector NormCS = CompT.InverseTransformVectorNoScale(WorldNorm).GetSafeNormal();
			if (NormCS.IsNearlyZero()) NormCS = UpCS;

			const FVector InwardCS = bIsLeft ? RightCS : -RightCS;
			const bool bVerticalHold = FMath::Abs(FVector::DotProduct(NormCS, UpCS)) < 0.5;
			FVector F_hold = bVerticalHold ? (UpCS * 0.92 + InwardCS * 0.25).GetSafeNormal() : (FwdCS * 0.90 + InwardCS * 0.22).GetSafeNormal();
			F_hold = (F_hold - NormCS * FVector::DotProduct(F_hold, NormCS)).GetSafeNormal();
			if (F_hold.IsNearlyZero()) F_hold = FwdCS;

			const FVector F_target = (F_hold + NormCS * 0.10).GetSafeNormal();
			const FVector N_target = (-NormCS - F_target * FVector::DotProduct(-NormCS, F_target)).GetSafeNormal();

			HitCS += NormCS * 3.0 - F_hold * 4.5;

			// Clamp arm reach from shoulder (full arm length minus a small bend)
			const FVector ShoulderCS = CS.GetComponentSpaceTransform(UpArm).GetLocation();
			const FVector ReachDelta = HitCS - ShoulderCS;
			const double ReachDist = ReachDelta.Size();
			if (ReachDist > 52.0)
			{
				HitCS = ShoulderCS + (ReachDelta / ReachDist) * 52.0;
			}

			const FTransform HandT = CS.GetComponentSpaceTransform(HandB);
			const FVector WristLoc = HandT.GetLocation();
			const FVector F0 = Mid3.IsValid()
				? (CS.GetComponentSpaceTransform(Mid3).GetLocation() - WristLoc).GetSafeNormal()
				: -UpCS;
			const FVector SpanRaw = (Idx2.IsValid() && Pnk2.IsValid())
				? (CS.GetComponentSpaceTransform(Idx2).GetLocation() - CS.GetComponentSpaceTransform(Pnk2).GetLocation())
				: FwdCS;
			const FVector T0 = (SpanRaw - F0 * FVector::DotProduct(SpanRaw, F0)).GetSafeNormal();
			const FVector N_palm = ((bIsLeft ? -1.0 : 1.0) * FVector::CrossProduct(F0, T0)).GetSafeNormal();

			const FQuat Q1 = FQuat::FindBetweenNormals(F0, F_target);
			const FVector N_after = Q1.RotateVector(N_palm);
			const double SinTwist = FVector::DotProduct(FVector::CrossProduct(N_after, N_target), F_target);
			const double CosTwist = FVector::DotProduct(N_after, N_target);
			const FQuat Q2(F_target, (float)FMath::Atan2(SinTwist, CosTwist));
			const FQuat TargetRot = (Q2 * Q1) * HandT.GetRotation();

			FTransform U = CS.GetComponentSpaceTransform(UpArm);
			FTransform L = CS.GetComponentSpaceTransform(LowArm);
			FTransform E = HandT;
			const FVector OutSide = bIsLeft ? -RightCS : RightCS;
			const FVector ElbowDir = bVerticalHold ? (-UpCS * 0.9 - FwdCS * 0.3 + OutSide * 0.4).GetSafeNormal()   // elbows down, slightly out
				: (-UpCS * 0.65 - FwdCS * 0.45 + OutSide * 0.55).GetSafeNormal();
			const FVector Pole = L.GetLocation() + ElbowDir * 45.0;
			AnimationCore::SolveTwoBoneIK(U, L, E, Pole, HitCS, false, 1.0, 1.02);
			E.SetRotation(TargetRot);

			TArray<FBoneTransform> Bt;
			Bt.Add(FBoneTransform(UpArm, U));
			Bt.Add(FBoneTransform(LowArm, L));
			Bt.Add(FBoneTransform(HandB, E));
			CS.LocalBlendCSBoneTransforms(Bt, W);

			const FTransform NewL = CS.GetComponentSpaceTransform(LowArm);
			const FTransform NewE = CS.GetComponentSpaceTransform(HandB);
			const FVector ForearmAxis = (NewE.GetLocation() - NewL.GetLocation()).GetSafeNormal();
			if (!ForearmAxis.IsNearlyZero())
			{
				const FQuat RelRot = NewL.GetRotation().Inverse() * NewE.GetRotation();
				const FVector LocalAxis = NewL.GetRotation().Inverse().RotateVector(ForearmAxis).GetSafeNormal();
				FQuat SwingQ, TwistQ;
				RelRot.ToSwingTwist(LocalAxis, SwingQ, TwistQ);
				const FQuat WorldTwist = NewL.GetRotation() * TwistQ * NewL.GetRotation().Inverse();

				if (Twist1.IsValid())
				{
					FTransform Tw1 = CS.GetComponentSpaceTransform(Twist1);
					Tw1.SetRotation(FQuat::Slerp(FQuat::Identity, WorldTwist, 0.70f * W) * Tw1.GetRotation());
					SetCS(CS, Twist1, Tw1, W);
				}
				if (Twist2.IsValid())
				{
					FTransform Tw2 = CS.GetComponentSpaceTransform(Twist2);
					Tw2.SetRotation(FQuat::Slerp(FQuat::Identity, WorldTwist, 0.38f * W) * Tw2.GetRotation());
					SetCS(CS, Twist2, Tw2, W);
				}
			}
		};

		ApplyClimbHandIK(UpL, LowL, HandL, In.ClimbHandPosL, In.ClimbHandNormL, In.ClimbHandWeightL, true);
		ApplyClimbHandIK(UpR, LowR, HandR, In.ClimbHandPosR, In.ClimbHandNormR, In.ClimbHandWeightR, false);
	}

	// ---- Unarmed Contextual Environment Hand-Touch IK (Wall / Car / Tree)
	if (In.bUnarmed)
	{
		if (BinocularsWeight < 0.01f && In.ClimbWeight < 0.25f)
		{
			const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();
			auto ApplyHandSurfaceTouch = [&](FCompactPoseBoneIndex UpArm, FCompactPoseBoneIndex LowArm, FCompactPoseBoneIndex HandB, const FVector& WorldPos, const FVector& WorldNorm, float Weight, bool bIsLeft)
			{
				if (Weight <= 0.005f || !UpArm.IsValid() || !LowArm.IsValid() || !HandB.IsValid()) return;

				// Lookup distal finger joints (now already opened flat in Local Space via Reference Pose pre-pass)
				const FCompactPoseBoneIndex Idx2 = Bone(BC, bIsLeft ? FName(TEXT("index_02_l"))  : FName(TEXT("index_02_r")));
				const FCompactPoseBoneIndex Mid3 = Bone(BC, bIsLeft ? FName(TEXT("middle_03_l")) : FName(TEXT("middle_03_r")));
				const FCompactPoseBoneIndex Pnk2 = Bone(BC, bIsLeft ? FName(TEXT("pinky_02_l"))  : FName(TEXT("pinky_02_r")));
				const FCompactPoseBoneIndex Twist1 = Bone(BC, bIsLeft ? FName(TEXT("lowerarm_twist_01_l")) : FName(TEXT("lowerarm_twist_01_r")));
				const FCompactPoseBoneIndex Twist2 = Bone(BC, bIsLeft ? FName(TEXT("lowerarm_twist_02_l")) : FName(TEXT("lowerarm_twist_02_r")));

				FVector HitCS = CompT.InverseTransformPosition(WorldPos);
				FVector NormCS = CompT.InverseTransformVectorNoScale(WorldNorm).GetSafeNormal();
				if (NormCS.IsNearlyZero()) NormCS = bIsLeft ? RightCS : -RightCS;

				// Surface tangent along walking direction
				FVector WallTanFwd = (FwdCS - NormCS * FVector::DotProduct(FwdCS, NormCS)).GetSafeNormal();
				if (WallTanFwd.IsNearlyZero()) WallTanFwd = FwdCS;

				// Desired finger direction on the surface: fingers point forward along walking tangent + slightly up (natural relaxed glide)
				const FVector OutSide = bIsLeft ? -RightCS : RightCS;
				FVector F_wall = (WallTanFwd * 0.85 + UpCS * 0.30).GetSafeNormal();
				F_wall = (F_wall - NormCS * FVector::DotProduct(F_wall, NormCS)).GetSafeNormal();
				if (F_wall.IsNearlyZero()) F_wall = WallTanFwd;

				// Slight anatomical tilt so palm pad rests flush on the wall
				const FVector F_target = (F_wall + NormCS * 0.10).GetSafeNormal();
				const FVector N_target = (-NormCS - F_target * FVector::DotProduct(-NormCS, F_target)).GetSafeNormal();

				// Smooth gentle trailing glide while walking
				const float GlideWave = FMath::Sin(Phase * 2.0f * UE_PI) * MoveWeight;
				HitCS += WallTanFwd * (GlideWave * 2.5 + 4.0) + UpCS * (FMath::Cos(Phase * 2.0f * UE_PI) * MoveWeight * 0.8);

				// Position wrist 3.0 cm out along surface normal so palm rests flush on the wall
				HitCS += NormCS * 3.0 - F_wall * 4.5;

				// Check forward/backward reach relative to shoulder: NEVER reach behind the back!
				const FVector ShoulderCS = CS.GetComponentSpaceTransform(UpArm).GetLocation();
				const FVector ReachDelta = HitCS - ShoulderCS;
				const double FwdDelta = FVector::DotProduct(ReachDelta, FwdCS);

				// Fade out immediately if the wall point passes behind the shoulder line
				if (FwdDelta < 0.0)
				{
					Weight *= 1.0f - FMath::SmoothStep(0.0f, -6.0f, (float)FwdDelta);
					if (Weight <= 0.005f) return;
				}
				if (FwdDelta > 26.0)
				{
					Weight *= 1.0f - FMath::SmoothStep(26.0f, 34.0f, (float)FwdDelta);
					if (Weight <= 0.005f) return;
				}

				// Clamp comfortable arm reach from shoulder (38 cm max so elbow stays naturally bent down)
				const double ReachDist = ReachDelta.Size();
				if (ReachDist > 38.0)
				{
					HitCS = ShoulderCS + (ReachDelta / ReachDist) * 38.0;
				}
				Weight *= 1.f - FMath::SmoothStep(0.f, 1.f, (float)FMath::Clamp((ReachDist - 38.0) / 8.0, 0.0, 1.0));
				if (Weight <= 0.005f) return;

				// Extract exact 3D plane of the open hand from Wrist -> Middle Fingertip (F0) and Pinky -> Index (T0)
				const FTransform HandT = CS.GetComponentSpaceTransform(HandB);
				const FVector WristLoc = HandT.GetLocation();
				const FVector F0 = Mid3.IsValid()
					? (CS.GetComponentSpaceTransform(Mid3).GetLocation() - WristLoc).GetSafeNormal()
					: -UpCS;

				const FVector SpanRaw = (Idx2.IsValid() && Pnk2.IsValid())
					? (CS.GetComponentSpaceTransform(Idx2).GetLocation() - CS.GetComponentSpaceTransform(Pnk2).GetLocation())
					: FwdCS;
				const FVector T0 = (SpanRaw - F0 * FVector::DotProduct(SpanRaw, F0)).GetSafeNormal();

				// Deterministic anatomical inner-palm normal in UE left-handed coordinates (+Cross for Right Hand, -Cross for Left Hand)
				const FVector N_palm = ((bIsLeft ? -1.0 : 1.0) * FVector::CrossProduct(F0, T0)).GetSafeNormal();

				// Two-step exact 3D frame alignment:
				// Q1 swings F0 -> F_target; Q2 twists around F_target so N_palm -> N_target (flat into the wall)
				const FQuat Q1 = FQuat::FindBetweenNormals(F0, F_target);
				const FVector N_after = Q1.RotateVector(N_palm);
				const double SinTwist = FVector::DotProduct(FVector::CrossProduct(N_after, N_target), F_target);
				const double CosTwist = FVector::DotProduct(N_after, N_target);
				const FQuat Q2(F_target, (float)FMath::Atan2(SinTwist, CosTwist));
				const FQuat TargetRot = (Q2 * Q1) * HandT.GetRotation();

				// Solve Two-Bone Arm IK with downward-outward relaxed elbow pole (never hyperextends or points inward)
				FTransform U = CS.GetComponentSpaceTransform(UpArm);
				FTransform L = CS.GetComponentSpaceTransform(LowArm);
				FTransform E = HandT;
				const FVector NaturalElbowDir = (-UpCS * 0.82 + OutSide * 0.40 - FwdCS * 0.25).GetSafeNormal();
				const FVector Pole = L.GetLocation() + NaturalElbowDir * 40.0;
				AnimationCore::SolveTwoBoneIK(U, L, E, Pole, HitCS, false, 1.0, 1.0);
				E.SetRotation(TargetRot);

				TArray<FBoneTransform> Bt;
				Bt.Add(FBoneTransform(UpArm, U));
				Bt.Add(FBoneTransform(LowArm, L));
				Bt.Add(FBoneTransform(HandB, E));
				CS.LocalBlendCSBoneTransforms(Bt, Weight);

				// Smooth forearm twist bones (lowerarm_twist_01 / 02) so the wrist cuff never pinches or creases
				const FTransform NewL = CS.GetComponentSpaceTransform(LowArm);
				const FTransform NewE = CS.GetComponentSpaceTransform(HandB);
				const FVector ForearmAxis = (NewE.GetLocation() - NewL.GetLocation()).GetSafeNormal();
				if (!ForearmAxis.IsNearlyZero())
				{
					const FQuat RelRot = NewL.GetRotation().Inverse() * NewE.GetRotation();
					const FVector LocalAxis = NewL.GetRotation().Inverse().RotateVector(ForearmAxis).GetSafeNormal();
					FQuat SwingQ, TwistQ;
					RelRot.ToSwingTwist(LocalAxis, SwingQ, TwistQ);
					const FQuat WorldTwist = NewL.GetRotation() * TwistQ * NewL.GetRotation().Inverse();

					if (Twist1.IsValid())
					{
						FTransform Tw1 = CS.GetComponentSpaceTransform(Twist1);
						Tw1.SetRotation(FQuat::Slerp(FQuat::Identity, WorldTwist, 0.70f * Weight) * Tw1.GetRotation());
						SetCS(CS, Twist1, Tw1, Weight);
					}
					if (Twist2.IsValid())
					{
						FTransform Tw2 = CS.GetComponentSpaceTransform(Twist2);
						Tw2.SetRotation(FQuat::Slerp(FQuat::Identity, WorldTwist, 0.38f * Weight) * Tw2.GetRotation());
						SetCS(CS, Twist2, Tw2, Weight);
					}
				}
			};

			ApplyHandSurfaceTouch(UpL, LowL, HandL, In.TouchWorldPosL, In.TouchWorldNormL, In.TouchWeightL, true);
			ApplyHandSurfaceTouch(UpR, LowR, HandR, In.TouchWorldPosR, In.TouchWorldNormR, In.TouchWeightR, false);

			// Subtle upper-body & head contextual awareness lean toward touched surface
			const float NetTouchSide = In.TouchWeightR - In.TouchWeightL; // + = right, - = left
			if (FMath::Abs(NetTouchSide) > 0.01f)
			{
				const FCompactPoseBoneIndex Spine3B = Bone(BC, NSpine3);
				if (Spine3B.IsValid())
				{
					FTransform SpT = CS.GetComponentSpaceTransform(Spine3B);
					SpT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(NetTouchSide * 2.5f)) * SpT.GetRotation());
					SetCS(CS, Spine3B, SpT, 0.75f);
				}
				if (HeadB.IsValid())
				{
					FTransform Ht = CS.GetComponentSpaceTransform(HeadB);
					Ht.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(NetTouchSide * 6.5f)) * Ht.GetRotation());
					SetCS(CS, HeadB, Ht, 0.65f);
				}
			}
		}

		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
		return true;
	}

	// ---- Multi-Stage Secondary Motion Shooting Reaction + Aim Pitch across Pelvis, Spine, Clavicles, Neck & Head
	const FVector RightCS = FVector::CrossProduct(UpCS, FwdCS).GetSafeNormal();
	const bool bHasSecondaryRecoil = (In.Kick > 0.002f || FMath::Abs(RefHead) > 0.01f || FMath::Abs(ShoulderRecoil) > 0.003f || FMath::Abs(TorsoRecoil) > 0.003f || FMath::Abs(HeadWhip) > 0.003f || SustainedFire > 0.01f);
	const float TremorWave = 0.f;   // (a 19 Hz sine on the bones read as the head / weapon vibrating: removed)

	// 0) Apply clean Aim Pitch to Upper Spine FIRST and snapshot the true aimed HandR transform (BaseHandR)
	//    so that secondary torso/shoulder shockwaves move the body around the weapon WITHOUT swinging the barrel up-and-right!
	const float Pitch = FMath::Clamp(In.AimPitch, -70.f, 70.f) * (1.f - FMath::Clamp(In.ClimbWeight, 0.f, 1.f));
	const FCompactPoseBoneIndex Sp[3] = { Bone(BC, NSpine3), Bone(BC, NSpine4), Bone(BC, NSpine5) };
	if (FMath::Abs(Pitch) > 0.01f)
	{
		const FQuat AimQ(Axis, FMath::DegreesToRadians(Pitch / 3.f));
		for (int32 K = 0; K < 3; ++K)
		{
			if (!Sp[K].IsValid()) continue;
			FTransform T = CS.GetComponentSpaceTransform(Sp[K]);
			T.SetRotation(AimQ * T.GetRotation());
			SetCS(CS, Sp[K], T);
		}
	}
	// 0b) Grenade throw / cook body motion (springs from Update): the twist and lean are spread over the five
	//     spine bones so the chest coils as one curve; the head turns back against it to keep the eyes on target.
	if ((FMath::Abs(ThrowYaw) > 0.05f || FMath::Abs(ThrowPitch) > 0.05f) && In.ClimbWeight < 0.1f)
	{
		static const FName NSpine2T(TEXT("spine_02"));
		const FCompactPoseBoneIndex TwB[5] = { Spine1, Bone(BC, NSpine2T), Sp[0], Sp[1], Sp[2] };
		static const float TwYawW[5] = { 0.12f, 0.18f, 0.22f, 0.24f, 0.24f };
		static const float TwPitchW[5] = { 0.26f, 0.22f, 0.20f, 0.17f, 0.15f };
		for (int32 K = 0; K < 5; ++K)
		{
			if (!TwB[K].IsValid()) continue;
			FTransform T = CS.GetComponentSpaceTransform(TwB[K]);
			const FQuat Q = FQuat(UpCS, FMath::DegreesToRadians(ThrowYaw * TwYawW[K])) * FQuat(Axis, FMath::DegreesToRadians(ThrowPitch * TwPitchW[K]));
			T.SetRotation(Q * T.GetRotation());
			SetCS(CS, TwB[K], T);
		}
		if (HeadB.IsValid())
		{
			FTransform Ht = CS.GetComponentSpaceTransform(HeadB);
			Ht.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(-ThrowYaw * 0.6f)) * FQuat(Axis, FMath::DegreesToRadians(-ThrowPitch * 0.5f)) * Ht.GetRotation());
			SetCS(CS, HeadB, Ht);
		}
	}
	const FTransform BaseHandR = CS.GetComponentSpaceTransform(HandR);
	const FQuat BaseHeadQ = HeadB.IsValid() ? CS.GetComponentSpaceTransform(HeadB).GetRotation() : FQuat::Identity;

	// 1) Pelvis & Lower-Core Secondary Shock Absorption (delayed hip bracing, centered on firing line)
	if (bHasSecondaryRecoil && In.ClimbWeight < 0.1f)
	{
		static const FName NPelvisS(TEXT("pelvis")), NSpine2S(TEXT("spine_02"));
		const FCompactPoseBoneIndex PelvisS = Bone(BC, NPelvisS);
		const FCompactPoseBoneIndex Spine2S = Bone(BC, NSpine2S);

		if (PelvisS.IsValid() && FMath::Abs(PelvisBrace) > 0.002f)
		{
			FTransform PT = CS.GetComponentSpaceTransform(PelvisS);
			// Delayed backward/downward hip sink absorbing the shot impulse
			PT.AddToTranslation(-FwdCS * (double)(PelvisBrace * 0.90f) - UpCS * (double)(FMath::Abs(PelvisBrace) * 0.35f));
			const FQuat PelvisRot = FQuat(Axis, FMath::DegreesToRadians(-PelvisBrace * 0.85f));
			PT.SetRotation(PelvisRot * PT.GetRotation());
			SetCS(CS, PelvisS, PT);
		}

		// Lower spine (spine_01, spine_02): progressive forward combat brace during sustained fire + centered wave
		const FCompactPoseBoneIndex LowSp[2] = { Spine1, Spine2S };
		for (int32 L = 0; L < 2; ++L)
		{
			if (!LowSp[L].IsValid()) continue;
			// Sustained fire leans the lower torso slightly forward (-Axis) to counter muzzle climb
			const float BracePitch = -SustainedFire * 0.9f + TorsoRecoil * 0.45f;
			const float BraceYaw = RecoilSideSmooth * TorsoRecoil * 0.45f;
			if (FMath::Abs(BracePitch) > 0.01f || FMath::Abs(BraceYaw) > 0.01f)
			{
				FTransform LT = CS.GetComponentSpaceTransform(LowSp[L]);
				LT.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(BraceYaw)) * FQuat(Axis, FMath::DegreesToRadians(BracePitch)) * LT.GetRotation());
				SetCS(CS, LowSp[L], LT);
			}
		}
	}

	// 2) Upper Spine (spine_03, spine_04, spine_05): Delayed Recoil Wave Propagation (centered, no rightward drift)
	if (bHasSecondaryRecoil)
	{
		const float SpineWavePitch[3] = {
			TorsoRecoil * 0.55f - SustainedFire * 0.50f,
			TorsoRecoil * 0.45f + ShoulderRecoil * 0.40f,
			ShoulderRecoil * 0.65f + In.Kick * 0.45f + TremorWave * 0.22f
		};
		// Zero-mean lateral jitter so the upper chest vibrates naturally without twisting off to the right
		const float SpineWaveYaw[3] = {
			RecoilSideSmooth * TorsoRecoil * 0.4f,
			RecoilSideSmooth * (TorsoRecoil * 0.5f + ShoulderRecoil * 0.6f),
			RecoilSideSmooth * (ShoulderRecoil * 0.8f + In.Kick * 0.7f)
		};
		const float SpineWaveRoll[3] = {
			RecoilSideSmooth * TorsoRecoil * 0.35f,
			RecoilSideSmooth * ShoulderRecoil * 0.55f,
			RecoilSideSmooth * (ShoulderRecoil * 0.75f + In.Kick * 0.5f) + TremorWave * 0.2f
		};

		for (int32 K = 0; K < 3; ++K)
		{
			if (!Sp[K].IsValid()) continue;
			const float DegPitch = SpineWavePitch[K];
			const float DegYaw   = SpineWaveYaw[K];
			const float DegRoll  = SpineWaveRoll[K];
			if (FMath::Abs(DegPitch) < 0.01f && FMath::Abs(DegYaw) < 0.01f && FMath::Abs(DegRoll) < 0.01f) continue;

			FTransform T = CS.GetComponentSpaceTransform(Sp[K]);
			const FQuat WaveRot =
				FQuat(UpCS, FMath::DegreesToRadians(DegYaw)) *
				FQuat(FwdCS, FMath::DegreesToRadians(DegRoll)) *
				FQuat(Axis, FMath::DegreesToRadians(DegPitch));
			T.SetRotation(WaveRot * T.GetRotation());
			SetCS(CS, Sp[K], T);
		}
	}

	// 3) Clavicle & Right Shoulder Pocket Compression (buttstock slams straight back into clavicle_r)
	if (bHasSecondaryRecoil && BinocularsWeight < 0.2f)
	{
		static const FName NClavRS(TEXT("clavicle_r")), NClavLS(TEXT("clavicle_l"));
		const FCompactPoseBoneIndex ClavRS = Bone(BC, NClavRS);
		const FCompactPoseBoneIndex ClavLS = Bone(BC, NClavLS);

		if (ClavRS.IsValid())
		{
			FTransform CRT = CS.GetComponentSpaceTransform(ClavRS);
			const float ShComp = ShoulderRecoil * 0.85f + In.Kick * 0.55f;
			// Pure axial backward pocket compression + subtle shoulder shrug (no rightward yaw twist)
			CRT.AddToTranslation(-FwdCS * (double)(ShComp * (In.bIsPistol ? 0.65f : 1.45f)) + UpCS * (double)(ShComp * 0.32f));
			const FQuat ClavRot = FQuat(FwdCS, FMath::DegreesToRadians(ShComp * 1.1f));
			CRT.SetRotation(ClavRot * CRT.GetRotation());
			SetCS(CS, ClavRS, CRT);
		}
		if (ClavLS.IsValid() && In.bLeftHandIK)
		{
			FTransform CLT = CS.GetComponentSpaceTransform(ClavLS);
			const float LeftJolt = TorsoRecoil * 0.65f + In.Kick * 0.35f;
			CLT.AddToTranslation(-FwdCS * (double)(LeftJolt * 0.75f) + UpCS * (double)(LeftJolt * 0.25f));
			CLT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-LeftJolt * 1.0f)) * CLT.GetRotation());
			SetCS(CS, ClavLS, CLT);
		}
	}

	// 4) Neck & Head Inertial Lag + Secondary Whip & Tactical Cheek-Weld
	static const FName NNeck1(TEXT("neck_01"));
	const FCompactPoseBoneIndex NeckB = Bone(BC, NNeck1);
	if (NeckB.IsValid() && bHasSecondaryRecoil)
	{
		FTransform NT = CS.GetComponentSpaceTransform(NeckB);
		// Neck lags behind chest initially, then snaps with HeadWhip
		// only smooth, delayed terms here (the instant kick terms made the head jitter on every shot)
		const float NeckPitch = HeadWhip * 0.9f + RefHead * 2.3f;   // head pushed back (reference, ~2.3 deg per cm)
		const float NeckRoll  = 0.f;
		NT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(NeckRoll)) * FQuat(Axis, FMath::DegreesToRadians(NeckPitch)) * NT.GetRotation());
		SetCS(CS, NeckB, NT);
	}

	const float EasedAim = FMath::SmoothStep(0.f, 1.f, AimWeight);
	if (HeadB.IsValid() && (EasedAim > 0.005f || bHasSecondaryRecoil))
	{
		FTransform Ht = CS.GetComponentSpaceTransform(HeadB);
		const float AimRoll  = (!In.bIsPistol ? 3.0f * EasedAim : 0.f);
		const float AimPitch = (!In.bIsPistol ? -1.5f * EasedAim : 0.f);

		// Gaze stabilisation (like a real shooter's neck reflex): the eyes stay on the sights, so the head keeps the
		// orientation it had before the shot while the torso moves under it. It still moves back with the body
		// (position comes from the spine / neck), plus a small, slow nod - no per-frame kick terms, no tremor.
		const FQuat AimQ = FQuat(FwdCS, FMath::DegreesToRadians(AimRoll)) * FQuat(Axis, FMath::DegreesToRadians(AimPitch));
		FQuat HeadR = FQuat::Slerp(Ht.GetRotation(), AimQ * BaseHeadQ, 0.85f);
		HeadR = FQuat(Axis, FMath::DegreesToRadians(RefHead * 0.5f + HeadWhip * 0.4f)) * HeadR;
		HeadR.Normalize();
		Ht.SetRotation(HeadR);
		SetCS(CS, HeadB, Ht, FMath::Max(0.85f * EasedAim, bHasSecondaryRecoil ? 1.0f : 0.f));
	}

	// talking (the cinematic dialogue): the speaker's head and neck nod with the words, the head turns a little, the chest
	// breathes - two mixed sines so it never looks like a loop
	if (In.TalkT >= 0.f && In.DeathTime < 0.f)
	{
		const float Tt = In.TalkT;
		const float Wt = FMath::SmoothStep(0.f, 0.35f, Tt);
		const float Nod = (FMath::Sin(Tt * 5.3f) * 0.6f + FMath::Sin(Tt * 2.3f + 1.1f) * 0.4f) * 5.f * Wt;
		const float Turn = FMath::Sin(Tt * 1.4f + 0.5f) * 6.f * Wt;
		const float Breath = FMath::Sin(Tt * 2.9f) * 1.3f * Wt;
		static const FName NNeckT(TEXT("neck_01"));
		const FCompactPoseBoneIndex NeckT = Bone(BC, NNeckT), ChestT = Bone(BC, NSpine5);
		if (ChestT.IsValid())
		{
			FTransform T = CS.GetComponentSpaceTransform(ChestT);
			T.SetRotation((FQuat(Axis, FMath::DegreesToRadians(Breath)) * T.GetRotation()).GetNormalized());
			SetCS(CS, ChestT, T);
		}
		if (NeckT.IsValid())
		{
			FTransform T = CS.GetComponentSpaceTransform(NeckT);
			T.SetRotation((FQuat(Axis, FMath::DegreesToRadians(Nod * 0.4f)) * T.GetRotation()).GetNormalized());
			SetCS(CS, NeckT, T);
		}
		if (HeadB.IsValid())
		{
			FTransform T = CS.GetComponentSpaceTransform(HeadB);
			T.SetRotation((FQuat(UpCS, FMath::DegreesToRadians(Turn)) * FQuat(Axis, FMath::DegreesToRadians(Nod * 0.6f)) * T.GetRotation()).GetNormalized());
			SetCS(CS, HeadB, T);
		}
	}

	if (!bCalibrated) { FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose); return true; }

	// 5) Primary + Secondary Weapon Recoil & Dynamic Elbow Flare (Anchored to BaseHandR so weapon never drifts up-right)
	const bool bSway = FMath::Abs(SwayYaw) > 0.02f || FMath::Abs(SwayPitch) > 0.02f || FMath::Abs(SwayX) > 0.02f || FMath::Abs(SwayY) > 0.02f;
	if ((In.Kick > 0.001f || FMath::Abs(ShoulderRecoil) > 0.002f || FMath::Abs(TorsoRecoil) > 0.002f || bSway
		|| FMath::Abs(RefPitch) > 0.02f || FMath::Abs(RefLift) > 0.02f || RefBack > 0.02f || FMath::Abs(RefYaw) > 0.02f) && BinocularsWeight < 0.5f)
	{
		// Use BaseHandR (captured right after AimPitch, before secondary spine/clavicle motion)
		// so the rifle stays locked on the firing axis while the shoulders/chest compress around it!
		const FTransform H = BaseHandR;
		const FTransform W = GripRel * H;
		const FVector WX = W.GetUnitAxis(EAxis::X);
		const FVector Side = FVector::CrossProduct(WX, UpCS).GetSafeNormal();

		// Combine fast primary kick with underdamped secondary shoulder/torso rebound + 19 Hz full-auto vibration
		const float CombinedKick = FMath::Max(0.f, In.Kick * 0.85f + ShoulderRecoil * 0.25f - TorsoRecoil * 0.20f);
		const float PitchKick = In.bIsPistol ? 2.8f : 1.35f;
		const float BackKick  = In.bIsPistol ? 2.2f : 3.8f;
		const float UpKick    = In.bIsPistol ? 0.65f : 0.30f;
		// Centered zero-mean axial barrel torque roll & micro-yaw (no rightward bias)
		const float RollTorque = (In.Kick * 1.8f + ShoulderRecoil * 1.1f) * RecoilSideSmooth;
		const float YawJitter  = (In.KickSide * 0.5f + RecoilSideSmooth * 0.5f) * In.Kick * 0.55f;

		// One model for the weapon in the hands: the springs fitted to the reference clip (flip / undershoot, kick
		// back, rise / sag, sideways wander, bore-axis roll), a little shoulder compression for the rifle, and the
		// weapon's inertia (sway). The old per-frame terms (In.Kick, random side, tremor) are no longer added here:
		// they made the weapon and the head jitter.
		(void)CombinedKick; (void)PitchKick; (void)BackKick; (void)UpKick; (void)RollTorque; (void)YawJitter;
		const float Steady = 1.f - 0.15f * AimWeight;   // aiming down sights: a firmer grip
		const FQuat Q =
			FQuat(WX, FMath::DegreesToRadians(RefRoll * Steady)) *
			FQuat(UpCS, FMath::DegreesToRadians(RefYaw * Steady + SwayYaw)) *
			FQuat(Side, FMath::DegreesToRadians(RefPitch * Steady + SwayPitch));

		FTransform Target = H;
		Target.SetLocation(
			H.GetLocation()
			- WX * (RefBack + ShoulderRecoil * (In.bIsPistol ? 0.3f : 0.6f))
			+ UpCS * RefLift
			+ FwdCS * SwayX + RightCS * SwayY);
		Target.SetRotation(Q * H.GetRotation());

		// Dynamic right-arm IK with outward/upward secondary elbow jolt on each shot
		FTransform UR = CS.GetComponentSpaceTransform(UpR);
		FTransform LR = CS.GetComponentSpaceTransform(LowR);
		FTransform ER = CS.GetComponentSpaceTransform(HandR);
		FVector BendDirR = LR.GetLocation() - (UR.GetLocation() + ER.GetLocation()) * 0.5;
		if (BendDirR.SizeSquared() < 9.0) BendDirR = (-UpCS * 0.65 + RightCS * 0.35).GetSafeNormal() * 15.0;
		// Secondary elbow flare: elbow jolts outward (+RightCS) and slightly up (+UpCS) as weapon compresses into shoulder
		const float ElbowJolt = FMath::Max(0.f, RefBack * 0.2f + ShoulderRecoil * 0.6f);   // smooth (follows the kick-back)
		const FVector PoleR = LR.GetLocation() + (BendDirR.GetSafeNormal() + RightCS * (ElbowJolt * 0.30) + UpCS * (ElbowJolt * 0.16)).GetSafeNormal() * 36.0;
		AnimationCore::SolveTwoBoneIK(UR, LR, ER, PoleR, Target.GetLocation(), false, 1.0, 1.0);
		ER.SetRotation(Target.GetRotation());
		TArray<FBoneTransform> BtR;
		BtR.Add(FBoneTransform(UpR, UR));
		BtR.Add(FBoneTransform(LowR, LR));
		BtR.Add(FBoneTransform(HandR, ER));
		CS.LocalBlendCSBoneTransforms(BtR, 1.f);
	}

	// ---- grenade throw (left hand off-hand throw while the right hand keeps the rifle)
	//      The hand follows curved paths (quadratic Bezier) relative to the left shoulder, which itself is carried by
	//      the coiling torso above, so arm and body arrive at the whip together (shoulder leads, hand lags = secondary).
	const float ThrowT = In.ThrowTime;
	// the baked throw clip (spine + left arm) replaces the procedural hand path below; only the cook hold stays procedural
	const bool bThrowing = false;
	const bool bCookPose = !(ThrowT >= 0.f && ThrowT < 1.60f) && CookW > 0.01f;
	float ThrowWeight = 0.f;
	if (bThrowing)
	{
		ThrowWeight = FMath::Max(FMath::Clamp(ThrowT / 0.10f, 0.f, 1.f), ThrowStartCook);
		if (ThrowT > 0.80f) ThrowWeight = FMath::SmoothStep(0.f, 1.f, 1.f - (ThrowT - 0.80f) / 0.15f);
	}

	// ---- Kar98k "smart collision" with the shooter: the butt plate is SEATED in the shoulder pocket (between the
	//      collar bone and the deltoid) instead of going through the shoulder; the rifle keeps its aim direction and only
	//      slides along / across it, the hands follow by IK. The recoil drives the shoulder back (clavicle + chest give
	//      way) and the seated rifle goes back with it, instead of the stock punching into the body.
	if (In.bKarRifle && bCalibrated && !In.bUnarmed && In.DeathTime < 0.f && In.ClimbWeight < 0.1f && UpR.IsValid() && LowR.IsValid())
	{
		const float Ws = (1.f - FMath::Clamp(ReloadW, 0.f, 1.f)) * (1.f - FMath::Clamp(BinocularsWeight, 0.f, 1.f)) * (1.f - FMath::Clamp(EquipW, 0.f, 1.f));
		static const FName NClavRK(TEXT("clavicle_r")), NSpine5K(TEXT("spine_05"));
		const FCompactPoseBoneIndex ClavRK = Bone(BC, NClavRK), Spine5K = Bone(BC, NSpine5K);
		if (Ws > 0.01f && ClavRK.IsValid())
		{
			// 1) recoil into the shoulder: it gives way backwards (and the chest turns a little with it)
			const float BackCm = FMath::Clamp(RefBack + In.Kick * 2.5f, 0.f, 8.f) * Ws;
			if (BackCm > 0.01f)
			{
				if (Spine5K.IsValid())
				{
					FTransform St5 = CS.GetComponentSpaceTransform(Spine5K);
					St5.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(0.45f * BackCm)) * St5.GetRotation());
					SetCS(CS, Spine5K, St5);
				}
				FTransform CT = CS.GetComponentSpaceTransform(ClavRK);
				CT.SetRotation(FQuat(UpCS, FMath::DegreesToRadians(1.6f * BackCm)) * CT.GetRotation());
				SetCS(CS, ClavRK, CT);
			}
			// 2) the shoulder pocket: a third of the way from the shoulder joint to the collar bone, a little forward / down
			const FVector Cl = CS.GetComponentSpaceTransform(ClavRK).GetLocation();
			const FVector Sh = CS.GetComponentSpaceTransform(UpR).GetLocation();
			const FVector Pocket = FMath::Lerp(Sh, Cl, 0.35) + FwdCS * 3.0 - UpCS * 2.0;
			// 3) seat the butt there: fully along the barrel (never inside the body), partly across it
			const FTransform HRk = CS.GetComponentSpaceTransform(HandR);
			const FTransform Wk = GripRel * HRk;
			const FVector Xb = Wk.GetUnitAxis(EAxis::X);
			const FVector Butt = Wk.TransformPosition(FVector(-33.6, 0.0, 3.0));   // the butt plate's centre (rifle frame)
			const FVector Err = Pocket - Butt;
			const double Along = FVector::DotProduct(Err, Xb);
			FVector Corr = Xb * Along + (Err - Xb * Along) * 0.6;
			if (Corr.Size() > 22.0) Corr = Corr.GetSafeNormal() * 22.0;
			FTransform HTk = HRk;
			HTk.AddToTranslation(Corr * (double)Ws);
			ArmIK(CS, UpR, LowR, HandR, HTk, 1.f, false);
		}
	}

	// ---- left hand on the handguard / support grip (off while reload, weapon swap, binoculars, cooking or throwing)
	float LeftW = (In.bLeftHandIK && BinocularsWeight < 0.1f) ? (1.f - FMath::Max(In.bKarRifle ? 0.f : ReloadW, EquipW)) * (1.f - WeaponSwapBlend * 0.7f) : 0.f;
	if (bThrowing) LeftW *= 1.f - ThrowWeight;
	LeftW *= (1.f - ThrowClipW);
	if (bCookPose) LeftW *= 1.f - CookW;
	if (LeftW > 0.01f)
	{
		FTransform W = GripRel * CS.GetComponentSpaceTransform(HandR);
		// IK/FK: when crouching / strafing puts the forend out of the left arm's reach, the hand used to stop short and
		// leave the rifle. Now the right hand (which carries the rifle, FK) is brought back by the missing distance first,
		// so the two-bone IK of the left arm always closes on the forend.
		const FVector Shift = ReachShift(CS, UpL, LowL, HandL, (LeftRel * W).GetLocation());
		if (Shift.SizeSquared() > 0.04 && UpR.IsValid() && LowR.IsValid())
		{
			FTransform HRs = CS.GetComponentSpaceTransform(HandR);
			HRs.AddToTranslation(Shift);
			ArmIK(CS, UpR, LowR, HandR, HRs, LeftW, false);
			W = GripRel * CS.GetComponentSpaceTransform(HandR);
		}
		ArmIK(CS, UpL, LowL, HandL, LeftRel * W, LeftW, true);
	}
	if ((bThrowing || bCookPose) && UpL.IsValid() && LowL.IsValid() && HandL.IsValid())
	{
		const FVector ShLoc = CS.GetComponentSpaceTransform(UpL).GetLocation();
		const FTransform GripW = GripRel * CS.GetComponentSpaceTransform(HandR);
		const FVector Rest = (LeftRel * GripW).GetLocation();
		// key poses (F forward, U up, R right; the throwing hand is the left one)
		const FVector Chest  = ShLoc + FwdCS * 24.0 + RightCS * 13.0 - UpCS * 24.0;   // grenade held at the sternum
		const FVector Side   = ShLoc - FwdCS * 4.0 - RightCS * 24.0 - UpCS * 6.0;     // swing out wide on the way back
		const FVector Cocked = ShLoc - FwdCS * 26.0 + UpCS * 16.0 - RightCS * 10.0;   // behind the ear, elbow high
		const FVector Top    = ShLoc + FwdCS * 6.0 + UpCS * 34.0 - RightCS * 4.0;     // over the top
		const FVector Release = ShLoc + FwdCS * 52.0 + UpCS * 12.0 + RightCS * 2.0;
		const FVector Follow = ShLoc + FwdCS * 34.0 - UpCS * 32.0 + RightCS * 26.0;   // across the body, low
		auto Bez = [](const FVector& A, const FVector& C, const FVector& B, float K)
		{
			const float M = 1.f - K;
			return A * (M * M) + C * (2.f * M * K) + B * (K * K);
		};

		FVector HandTarget = Chest;
		float WristFlick = 0.f;
		float Wt = CookW;
		if (bThrowing)
		{
			Wt = ThrowWeight;
			if (ThrowT < 0.18f)
			{
				const FVector From = FMath::Lerp(Rest, Chest, ThrowStartCook);
				HandTarget = FMath::Lerp(From, Chest, FMath::SmoothStep(0.f, 1.f, ThrowT / 0.18f));
			}
			else if (ThrowT < 0.40f)
			{
				// wind-up: decelerates into the cocked pose (ease-out) - the coil
				const float K = (ThrowT - 0.18f) / 0.22f;
				HandTarget = Bez(Chest, Side, Cocked, FMath::Sin(K * HALF_PI));
				WristFlick = 18.f * K;                     // wrist cocks back
			}
			else if (ThrowT < 0.52f)
			{
				// whip: accelerates over the top (ease-in), hand lags the shoulder then snaps through
				const float K = (ThrowT - 0.40f) / 0.12f;
				HandTarget = Bez(Cocked, Top, Release, FMath::Pow(K, 1.6f));
				WristFlick = FMath::Lerp(18.f, -34.f, FMath::Clamp(K * 1.3f, 0.f, 1.f));   // wrist snap at release
			}
			else if (ThrowT < 0.75f)
			{
				// follow-through: the arm keeps travelling across and down the body, slowing (ease-out)
				const float K = (ThrowT - 0.52f) / 0.23f;
				HandTarget = FMath::Lerp(Release, Follow, 1.f - FMath::Square(1.f - K));
				WristFlick = FMath::Lerp(-34.f, -10.f, K);
			}
			else
			{
				const float K = (ThrowT - 0.75f) / 0.20f;
				HandTarget = FMath::Lerp(Follow, Rest, FMath::SmoothStep(0.f, 1.f, K));
				WristFlick = -10.f * (1.f - K);
			}
		}
		else
		{
			// cook hold: a slow breathing bob so the held grenade is not frozen in space
			HandTarget = Chest + UpCS * (double)(FMath::Sin(IdleT * 2.1f) * 0.6f);
		}

		FTransform TargetTransform = CS.GetComponentSpaceTransform(HandL);
		TargetTransform.SetLocation(HandTarget);
		TargetTransform.SetRotation(FQuat(Axis, FMath::DegreesToRadians(WristFlick)) * TargetTransform.GetRotation());
		ArmIK(CS, UpL, LowL, HandL, TargetTransform, Wt, true);
	}

	// Kar98k bolt cycle (clip 0.30 .. 1.50 s): the right hand leaves the grip for the bolt, so the LEFT hand carries the rifle.
	// The rifle frame is latched while the right hand is still on the grip and then held still; the left hand is put back on
	// its forend; GripRelOut (rifle relative to hand_r) is what the game thread attaches the rifle with.
	if (SniperW > 0.01f && bCalibrated)
	{
		const float St = In.SniperFireTime;
		// The rifle is latched RELATIVE TO THE CHEST (spine_05), not frozen in component space: before, after a standing
		// shot, crouching (hips -34 cm) or strafing moved the body away from a rifle hanging still in space, the left arm
		// could not reach it any more and the rifle floated off the left hand. Now it rides with the torso.
		static const FName NChestS(TEXT("spine_05"));
		const FCompactPoseBoneIndex ChestS = Bone(BC, NChestS);
		const FTransform ChestT = ChestS.IsValid() ? CS.GetComponentSpaceTransform(ChestS) : FTransform::Identity;
		if (St < 0.24f) SniperW0 = (GripRel * CS.GetComponentSpaceTransform(HandR)).GetRelativeTransform(ChestT);   // (chest-relative)
		const float Kb = FMath::SmoothStep(0.22f, 0.30f, St) * (1.f - FMath::SmoothStep(1.30f, 1.42f, St));
		if (Kb > 0.001f)
		{
			FTransform SniperWj = KarJolt() * (SniperW0 * ChestT);   // (with the bolt's jolts)
			// two-bone IK reach guard: pull the rifle toward the left shoulder if the forend is out of reach
			SniperWj.AddToTranslation(ReachShift(CS, UpL, LowL, HandL, (LeftRel * SniperWj).GetLocation()));
			ArmIK(CS, UpL, LowL, HandL, LeftRel * SniperWj, Kb, true);
			// FK: the rifle hangs from the left hand where it really ended up (no gap even if the IK fell short)
			{
				const FTransform FromL = WeaponFromLeftHand(CS, HandL, LeftRel);
				SniperWj.SetLocation(FMath::Lerp(SniperWj.GetLocation(), FromL.GetLocation(), (double)Kb));
				SniperWj.SetRotation(FQuat::Slerp(SniperWj.GetRotation(), FromL.GetRotation(), Kb).GetNormalized());
			}
			// right hand: from the grip to the bolt knob, with the bolt (same timeline as the bolt parts in the weapon
			// component: handle up 0.37-0.57 s, back 0.57-0.80, forward 0.80-1.02, down 1.02-1.15), back on the grip by 1.30
			const float Kh = FMath::SmoothStep(0.28f, 0.37f, St) * (1.f - FMath::SmoothStep(1.15f, 1.30f, St));
			if (Kh > 0.001f && UpR.IsValid() && LowR.IsValid())
			{
				const float Up = FMath::SmoothStep(0.37f, 0.57f, St) * (1.f - FMath::SmoothStep(1.02f, 1.15f, St));
				const float Back = -11.f * FMath::SmoothStep(0.57f, 0.80f, St) * (1.f - FMath::SmoothStep(0.80f, 1.02f, St));
				const FVector Knob = FVector(1.0 + Back, 0.0, 8.6) + FRotator(0.0, 0.0, -70.f * Up).RotateVector(FVector(-2.0, 5.4, -1.8));
				const FVector PalmT = SniperWj.TransformPosition(Knob + FVector(0.4, 2.2, 1.6));   // the palm over the knob, from outside
				const FTransform HR = CS.GetComponentSpaceTransform(HandR);
				const FVector PalmNow = Palm(CS, HandR, Bone(BC, NMidR));
				FTransform BoltHand = HR;
				BoltHand.SetLocation(PalmT - (PalmNow - HR.GetLocation()));
				BoltHand.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-25.f * Kh)) * HR.GetRotation());
				ArmIK(CS, UpR, LowR, HandR, BoltHand, Kh, false);
			}
			const FTransform Dyn = SniperWj.GetRelativeTransform(CS.GetComponentSpaceTransform(HandR));
			GripRelOut = GripRel;
			GripRelOut.BlendWith(Dyn, Kb);
			GripRelOut.NormalizeRotation();
			bBoltOut = true;
		}
	}

	// Kar98k reload (AAAKarReload): the rifle comes down off the shoulder into the LEFT hand - lowered, pulled in, canted
	// right side up, muzzle a little up - and rides on that hand (the rifle is placed from it, it is not frozen in
	// space); the right hand opens the bolt, loads the rounds one by one from the belt pouch (thumb presses each in, the
	// rifle jolts and settles), closes the bolt and goes back to the grip while the rifle goes back to the shoulder
	if (In.bKarRifle && ReloadW > 0.01f && In.ReloadTime >= 0.f && bCalibrated && In.DeathTime < 0.f && !In.bUnarmed && In.ClimbWeight < 0.3f)
	{
		const float T = In.ReloadTime;
		const int32 Nr = FMath::Clamp(In.KarReloadN, 0, 5);
		const float Len = AAAKarReload::Length(Nr), C0 = AAAKarReload::CloseStart(Nr);
		const float Kp = FMath::SmoothStep(0.f, 0.35f, T) * (1.f - FMath::SmoothStep(Len - 0.38f, Len, T));
		// the aimed rifle (where the right hand holds it now) and the loading position
		const FTransform Aim = GripRel * CS.GetComponentSpaceTransform(HandR);
		const FVector BarrelCS = Aim.GetUnitAxis(EAxis::X);
		FTransform Load(FQuat(BarrelCS, FMath::DegreesToRadians(26.f)) * FQuat(Axis, FMath::DegreesToRadians(8.f)) * Aim.GetRotation(),
			Aim.GetLocation() - UpCS * 14.0 - FwdCS * 7.0 - RightCS * 3.0);
		FTransform Wr;
		Wr.SetLocation(FMath::Lerp(Aim.GetLocation(), Load.GetLocation(), (double)Kp));
		Wr.SetRotation(FQuat::Slerp(Aim.GetRotation(), Load.GetRotation(), Kp).GetNormalized());
		// in the air the rifle floats up a little in the hands (the body drops away under it), muzzle up
		if (FallW > 0.01f)
		{
			Wr.AddToTranslation(UpCS * (double)(3.0f * FallW));
			Wr.SetRotation(FQuat(Axis, FMath::DegreesToRadians(4.f * FallW)) * Wr.GetRotation());
		}
		Wr = KarJolt() * Wr;   // the pushes / bolt strokes / footsteps / landings
		// left hand under the fore-stock carries it (reach guard, then FK: the rifle is placed from the real left hand)
		Wr.AddToTranslation(ReachShift(CS, UpL, LowL, HandL, (LeftRel * Wr).GetLocation()));
		ArmIK(CS, UpL, LowL, HandL, LeftRel * Wr, ReloadW, true);
		{
			const FTransform FromL = WeaponFromLeftHand(CS, HandL, LeftRel);
			Wr.SetLocation(FMath::Lerp(Wr.GetLocation(), FromL.GetLocation(), (double)ReloadW));
			Wr.SetRotation(FQuat::Slerp(Wr.GetRotation(), FromL.GetRotation(), ReloadW).GetNormalized());
		}
		// right hand path (palm), continuous from the grip and back to it
		float Up = 0.f, Bk = 0.f;
		AAAKarReload::Bolt(T, Nr, Up, Bk);
		auto KnobAt = [&](float U, float B)
		{
			const FVector K = FVector(1.0 - 11.0 * B, 0.0, 8.6) + FRotator(0.0, 0.0, -70.f * U).RotateVector(FVector(-2.0, 5.4, -1.8));
			return Wr.TransformPosition(K + FVector(0.4, 2.2, 1.6));
		};
		static const FName NPelvisK(TEXT("pelvis"));
		const FCompactPoseBoneIndex PelK = Bone(BC, NPelvisK);
		const FVector Pouch = (PelK.IsValid() ? CS.GetComponentSpaceTransform(PelK).GetLocation() : Aim.GetLocation() - UpCS * 40.0) + FwdCS * 16.0 + RightCS * 13.0 - UpCS * 2.0;
		const FVector Grip = Wr.GetLocation();
		const FVector Top = Wr.TransformPosition(FVector(5.0, 0.0, 15.0)), Press = Wr.TransformPosition(FVector(5.0, 0.4, 10.4));
		auto Ease = [](float A, float B, float X) { return FMath::SmoothStep(A, B, X); };
		FVector P = Grip;
		if (T < C0)
		{
			if (T < 0.18f) P = FMath::Lerp(Grip, KnobAt(0.f, 0.f), (double)Ease(0.06f, 0.18f, T));   // to the bolt knob
			else if (T < AAAKarReload::Open) P = KnobAt(Up, Bk);                                      // up and back with it
			else
			{
				const int32 K = FMath::Min((int32)((T - AAAKarReload::Open) / AAAKarReload::Per), FMath::Max(Nr - 1, 0));
				const float S = T - (AAAKarReload::Open + K * AAAKarReload::Per);
				const FVector From = K == 0 ? KnobAt(1.f, 1.f) : Press;
				if (S < 0.14f) P = FMath::Lerp(From, Pouch, (double)Ease(0.f, 0.14f, S));            // a round from the pouch
				else if (S < 0.30f) P = FMath::Lerp(Pouch, Top, (double)Ease(0.14f, 0.30f, S));       // up over the open action
				else if (S < 0.38f) P = FMath::Lerp(Top, Press, (double)FMath::Pow(FMath::Clamp((S - 0.30f) / 0.08f, 0.f, 1.f), 1.7f));   // thumb presses it in
				else P = Press;
			}
		}
		else
		{
			const float S = T - C0;
			const FVector From = Nr > 0 ? Press : KnobAt(1.f, 1.f);
			if (S < 0.08f) P = FMath::Lerp(From, KnobAt(1.f, 1.f), (double)Ease(0.f, 0.08f, S));
			else if (S < 0.30f) P = KnobAt(Up, Bk);                                                   // forward and down
			else P = FMath::Lerp(KnobAt(0.f, 0.f), Grip, (double)Ease(0.30f, 0.62f, S));            // back on the grip
		}
		const FTransform HR = CS.GetComponentSpaceTransform(HandR);
		const FVector PalmNow = Palm(CS, HandR, Bone(BC, NMidR));
		const float Away = FMath::Clamp((float)FVector::Dist(P, Grip) / 8.f, 0.f, 1.f);
		FTransform HandT = HR;
		HandT.SetLocation(P - (PalmNow - HR.GetLocation()));
		HandT.SetRotation(FQuat(FwdCS, FMath::DegreesToRadians(-30.f * Away)) * HR.GetRotation());   // palm down over the action
		ArmIK(CS, UpR, LowR, HandR, HandT, ReloadW);
		// the rifle is attached through hand_r on the game thread: give it the left-hand-carried frame
		const FTransform Dyn = Wr.GetRelativeTransform(CS.GetComponentSpaceTransform(HandR));
		GripRelOut = GripRel;
		GripRelOut.BlendWith(Dyn, ReloadW);
		GripRelOut.NormalizeRotation();
		bBoltOut = true;
	}
	// any other gun: the left hand is pulled onto the magazine (IK) while it reaches for it
	else if (MagIKW > 0.01f && bCalibrated && In.DeathTime < 0.f && !In.bUnarmed)
	{
		const FTransform Wr = GripRel * CS.GetComponentSpaceTransform(HandR);
		const FVector MagPos = Wr.TransformPosition(In.MagRelLoc);
		const FTransform HL = CS.GetComponentSpaceTransform(HandL);
		const FVector PalmOff = Palm(CS, HandL, Bone(BC, NMidL)) - HL.GetLocation();
		FTransform T = HL;
		T.SetLocation(MagPos - PalmOff);
		ArmIK(CS, UpL, LowL, HandL, T, MagIKW * 0.85f, true);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	return true;
}
