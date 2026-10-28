// AAA City Weapons - native rifle animation (no Anim Blueprint needed).
//
// While the rifle is out, the character's mesh runs this anim instance instead of ABP_Unarmed:
//  * locomotion from the project's rifle set (Characters/Mannequins/Anims/Rifle): idle ADS, 8-way walk and jog,
//    blended by speed and direction (strafing), the fall loop in the air, the reload on the upper body
//  * aim pitch: the spine bends to the camera pitch
//  * recoil: the right hand is kicked back / up on a spring and the arm follows with two-bone IK
//  * left hand IK: the left hand stays on the handguard (where the idle pose puts it) through every blend
//  * calibration: from the idle pose it measures where the rifle sits in the right hand (palm -> left palm),
//    the weapon component attaches the rifle model there (GripRel) and builds it to fit the hands
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AAARifleAnimInstance.generated.h"

/** Kar98k reload, shared by the weapon component (events, bolt parts) and the anim (hands, rifle):
 *  the rifle comes down into the left hand, the bolt opens, the right hand loads the rounds one by one from the
 *  belt pouch (N = rounds missing, 1..5), the bolt closes and the rifle goes back to the shoulder. Seconds. */
namespace AAAKarReload
{
	constexpr float Open = 0.40f;    // rifle down + bolt up and back
	constexpr float Per = 0.42f;     // one round: pouch, over the action, pressed in
	constexpr float Close = 0.70f;   // bolt forward + down, rifle back up
	inline float Length(int32 N) { return Open + (float)FMath::Max(N, 0) * Per + Close; }
	inline float PushTime(int32 K) { return Open + (float)K * Per + 0.34f; }   // the round goes in
	inline float CloseStart(int32 N) { return Open + (float)FMath::Max(N, 0) * Per; }
	/** bolt handle up (0..1) and bolt back (0..1) at time T */
	inline void Bolt(float T, int32 N, float& Up, float& Back)
	{
		const float C0 = CloseStart(N);
		Up = FMath::SmoothStep(0.18f, 0.26f, T) * (1.f - FMath::SmoothStep(C0 + 0.18f, C0 + 0.28f, T));
		Back = FMath::SmoothStep(0.26f, 0.36f, T) * (1.f - FMath::SmoothStep(C0 + 0.08f, C0 + 0.18f, T));
	}
}

class UAnimSequence;
class UAAARifleAnimInstance;

/** values the weapon component sets every frame (game thread) */
struct FAAARifleInputs
{
	FVector2D LocalVel = FVector2D::ZeroVector;   // cm/s along the character's forward / right
	float AimPitch = 0.f;                         // degrees, + = up
	float Kick = 0.f;                             // recoil spring 0..~1.5
	float KickSide = 0.f;                         // -1..1
	float ReloadTime = -1.f;                      // seconds into the reload, < 0 = not reloading
	float EquipTime = -1.f;                       // seconds into drawing the rifle, < 0 = done
	float HitTime = -1.f;                         // seconds since a hit (flinch), < 0 = none
	int32 HitIndex = 0;                           // which flinch clip
	float ThrowTime = -1.f;                       // seconds into grenade throw (baked clip, 1.60 s, release at 0.80 s), < 0 = none
	float SniperFireTime = -1.f;                  // seconds into the Kar98k fire + bolt-cycle clip (2.00 s), < 0 = none
	bool bKarRifle = false;                       // Kar98k in the hands: the left hand carries the rifle while the right hand works the bolt / clip
	int32 ReloadStage = 0;                        // 0 hand going to the magazine, 1 magazine in the hand, 2 magazine back in
	int32 KarReloadN = 0;                         // Kar98k: rounds this reload loads one by one (AAAKarReload)
	FVector MagRelLoc = FVector::ZeroVector;      // magazine well in rifle space (left-hand IK target when reloading), zero = unknown
	float CookWeight = 0.f;                       // 1 = holding a cooked grenade at the chest (G held)
	float CrouchWeight = 0.f;                     // 0 standing .. 1 full crouch (procedural: hips down, knees bent)
	float LeanAmount = 0.f;                       // -1 lean left .. +1 lean right (peeking round a corner)
	bool bFalling = false;
	bool bJumpUp = false;                         // falling because of a jump (going up)
	float VelZ = 0.f;                             // cm/s, + = up (jump rise -> apex -> fall)
	float HitDir = 0.f;                           // where the last hit came from: degrees from facing (0 front, +90 right, 180 back)
	float HitStrength = 0.4f;                     // 0 graze .. 1 heavy (picks the flinch clip)
	float DeathTime = -1.f;                       // seconds since death (< 0 = alive): the death clip plays, then ragdoll
	int32 DeathIndex = 0;                         // UAAARifleAnimInstance::PickDeath
	float TalkT = -1.f;                           // cinematic dialogue: s since his line started (< 0 = silent): the head nods, the chest breathes
	float ShahadaT = -1.f;                        // the player's last moments (s since the fall ended, < 0 = none): right hand raised, index finger to the sky
	bool bAiming = false;
	bool bLeftHandIK = true;
	bool bIsPistol = false;
	bool bUnarmed = false;
	bool bBinoculars = false;
	float BinocularsWeight = 0.f;

	// Unarmed contextual hand-touch IK (wall / car / tree)
	float TouchWeightL = 0.f;
	float TouchWeightR = 0.f;
	FVector TouchWorldPosL = FVector::ZeroVector;
	FVector TouchWorldNormL = FVector(0.0, 1.0, 0.0);
	FVector TouchWorldPosR = FVector::ZeroVector;
	FVector TouchWorldNormR = FVector(0.0, -1.0, 0.0);

	// Procedural 4-Limb IK Climbing & Ledge/Car Mantling System
	bool bClimbing = false;
	bool bMantling = false;       // true = mantling onto car/ledge/roof; false = vertical wall/tree free-climb
	float ClimbWeight = 0.f;      // 0..1 overall IK blend weight
	float MantleProgress = 0.f;   // 0..1 progress through mantle animation
	float ClimbPhase = 0.f;       // 0..1 cyclic phase for vertical wall/tree climbing
	float ClimbMoveSpeed = 0.f;   // 0..1 movement intensity while climbing
	FVector ClimbSurfaceNorm = FVector(-1.0, 0.0, 0.0);
	FVector ClimbHandPosL = FVector::ZeroVector;
	FVector ClimbHandPosR = FVector::ZeroVector;
	FVector ClimbHandNormL = FVector(0.0, 0.0, 1.0);
	FVector ClimbHandNormR = FVector(0.0, 0.0, 1.0);
	FVector ClimbFootPosL = FVector::ZeroVector;
	FVector ClimbFootPosR = FVector::ZeroVector;
	FVector ClimbFootNormL = FVector(0.0, 0.0, 1.0);
	FVector ClimbFootNormR = FVector(0.0, 0.0, 1.0);
	float ClimbHandWeightL = 0.f;
	float ClimbHandWeightR = 0.f;
	float ClimbFootWeightL = 0.f;
	float ClimbFootWeightR = 0.f;
	// baked Mixamo clip (Braced Hang To Crouch) retargeted at runtime onto the body: < 0 = off
	float ClimbClipTime = -1.f;
	float ClimbClipWeight = 0.f;

	// Adaptive Ground / Slope / Vehicle-Roof Foot & Pelvis IK
	float GroundFootWeight = 0.f;
	float GroundOffsetL = 0.f;
	float GroundOffsetR = 0.f;
	float GroundPelvisOffsetZ = 0.f;
	FVector GroundNormL = FVector(0.0, 0.0, 1.0);
	FVector GroundNormR = FVector(0.0, 0.0, 1.0);
};

/** bone map of one baked clip onto the mannequin (rebuilt when the bone set changes) */
struct FBakedMap
{
	int32 Num = -1;
	TArray<int32> BoneOf;      // compact index -> clip bone (-1 = not driven)
	TArray<FQuat> RefRot;      // per clip bone: reference rotation (component space)
	TArray<FVector> RefDir;    // per clip bone: reference bone direction (component space)
};

/** view of a baked clip header (namespace AAAClipXxx) */
struct FBakedClipView
{
	int32 NumFrames = 0;
	float FPS = 30.f;
	int32 NumBones = 0;
	const TCHAR* const* Bone = nullptr;
	const TCHAR* const* DirChild = nullptr;
	const float (*BindDir)[3] = nullptr;
	const float* Delta = nullptr;   // [NumFrames][NumBones][4]
};

struct FAAARifleProxy : public FAnimInstanceProxy
{
	FAAARifleProxy() {}
	FAAARifleProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	// assets (copied from the instance)
	const UAnimSequence* Idle = nullptr;
	const UAnimSequence* Walk[8] = {};
	const UAnimSequence* Jog[8] = {};
	const UAnimSequence* Fall = nullptr;
	const UAnimSequence* Reload = nullptr;
	const UAnimSequence* Equip = nullptr;
	const UAnimSequence* JumpStart = nullptr;
	const UAnimSequence* Land = nullptr;
	const UAnimSequence* Hit[4] = {};
	const UAnimSequence* AirRise = nullptr;   // jump going up (loop)
	const UAnimSequence* AirApex = nullptr;   // top of the jump
	const UAnimSequence* LandAdd = nullptr;   // additive landing recovery (on top of the running legs)
	FAAARifleInputs In;
	bool bActivePistol = false;
	bool bActiveUnarmed = false;

	// timing measured from the clips themselves (once): the speed the planted foot slides at = the speed the clip
	// is animated for (no foot sliding at any speed), and where in the cycle the left foot is in front (phase sync
	// between the 8 directions, so blending never swaps the feet)
	bool bAnalyzed = false;
	float WalkSpd[8] = { 170.f, 170.f, 170.f, 170.f, 170.f, 170.f, 170.f, 170.f };
	float JogSpd[8] = { 380.f, 380.f, 380.f, 380.f, 380.f, 380.f, 380.f, 380.f };
	float WalkOff[8] = {}, JogOff[8] = {};
	void Analyze(FPoseContext& Output);

	// state (anim thread)
	float Phase = 0.f, IdleT = 0.f, FallT = 0.f, FallW = 0.f, ReloadW = 0.f, EquipW = 0.f;
	float StartT = -1.f, LandT = -1.f, MoveSpd = 0.f;
	bool bWasFalling = false;
	int32 DirK0 = 0, DirK1 = 1; float DirF = 0.f, JogW = 0.f;
	FVector2D SmoothVel = FVector2D::ZeroVector;

	// Enhanced AAA animation smoothing & transitions
	float SmoothSpeed = 0.f;
	float SmoothAngle = 0.f;
	float MoveWeight = 0.f;
	float AimWeight = 0.f;
	float WeaponSwapBlend = 0.f;
	float BinocularsWeight = 0.f;

	// Turn In Place foot movement
	float LastTurnYaw = 0.f;
	bool bTurnYawInit = false;
	float TurnPhase = 0.f;
	float TurnWeight = 0.f;
	float SmoothTurnDir = 0.f;
	// feet planted in the world while standing (turn in place / idle / crouch): held until too far off, then a step
	FTransform PlantW[2];
	FTransform StepFromW[2];
	float StepT[2] = { -1.f, -1.f };
	float PlantWeight = 0.f;
	float SettleT = 0.f;
	float LastDt = 0.016f;
	bool bPlantInit = false;

	// Multi-stage Secondary Motion Shooting Reaction (delayed coupled springs for Shoulder -> Torso -> Head -> Pelvis wave propagation)
	float PrevKick = 0.f;
	float ShoulderRecoil = 0.f, ShoulderRecoilV = 0.f;
	float TorsoRecoil = 0.f, TorsoRecoilV = 0.f;
	float HeadWhip = 0.f, HeadWhipV = 0.f;
	float PelvisBrace = 0.f, PelvisBraceV = 0.f;
	float RecoilSideSmooth = 0.f;
	float SustainedFire = 0.f;
	float FireTremorPhase = 0.f;

	// Reference-matched shot reaction (measured frame by frame from a two-handed handgun reference clip, 30 fps):
	//  muzzle flip +10 deg peaking at ~80 ms, back through zero at ~200 ms, -4 deg undershoot, settled ~0.6 s
	//  hands kicked back ~4 cm instantly, recovering over ~0.45 s (exponential)
	//  hands rise ~2.8 cm peaking at ~0.15 s, then sag ~2 cm below at ~0.7 s, settled ~1.2 s (slow arm spring)
	//  head moves back ~2.5 cm, delayed, peaking at ~0.3-0.4 s, recovered at ~0.9 s
	float RefPitch = 0.f, RefPitchV = 0.f;   // deg
	float RefLift = 0.f, RefLiftV = 0.f;     // cm
	float RefBack = 0.f, RefBackT = 0.f;     // cm
	float RefHead = 0.f, RefHeadV = 0.f;     // cm
	float RefYaw = 0.f, RefYawV = 0.f;       // deg: sideways muzzle wander per shot
	float RefRoll = 0.f, RefRollV = 0.f;     // deg: bore-axis torque roll per shot
	// weapon inertia (sway): the weapon lags behind turning / looking up-down / speeding up, then catches up
	float SwayYaw = 0.f, SwayYawV = 0.f, SwayPitch = 0.f, SwayPitchV = 0.f;   // deg
	float SwayX = 0.f, SwayXV = 0.f, SwayY = 0.f, SwayYV = 0.f;               // cm (forward / right)
	float PrevYaw = 0.f, PrevAimPitch = 0.f;
	FVector2D PrevVel = FVector2D::ZeroVector;
	bool bSwayInit = false;
	// air states (0 take-off, 1 rising, 2 apex, 3 falling) with a short crossfade from the previous one
	int32 AirState = 3, AirPrev = -1;
	float AirT = 0.f, AirPrevT = 0.f, AirBlend = 0.f;
	// weapon / stance change: crossfade from the last pose (no snap between the unarmed, pistol and rifle sets)
	TArray<FTransform> LastPose, SwapFrom;
	// runtime retarget of the baked climbing clip: compact bone index per clip bone + the mannequin reference pose
	int32 ClipMapNum = -1;
	TArray<int32> ClipBoneOfCompact;   // compact index -> clip bone (-1 = not driven)
	TArray<FQuat> ClipRefRot;          // per clip bone: reference rotation (component space)
	TArray<FVector> ClipRefDir;        // per clip bone: reference bone direction (component space)
	void ApplyClimbClip(FPoseContext& Output, const FVector& FwdCS, const FVector& UpCS);
	// generic baked clip player (crouch walk legs / throw / Kar98k fire) with a bone mask:
	// 1 = legs (thigh..ball), 2 = spine + left arm + left fingers, 3 = upper body (spine_01 and everything above)
	FBakedMap MapCrouch, MapThrow, MapSniper;
	void ApplyBaked(FPoseContext& Output, FBakedMap& Map, const FBakedClipView& Clip, float Time, float W, int32 MaskKind, const FVector& FwdCS, const FVector& UpCS);
	float CrouchPhase = 0.f;                   // crouch-walk cycle, advanced by distance travelled (no foot sliding)
	float CrouchWarp = 0.f;                    // orientation warping: hips / legs turned toward the travel direction (deg)
	bool bCrouchBack = false;                  // moving backwards: the crouch walk plays in reverse
	float CrouchClipW = 0.f, ThrowClipW = 0.f, SniperW = 0.f;   // weights of the baked clips this frame
	float ClipGroundZ = 0.f;                   // lowest foot height (along up) before the crouch clip is applied
	float ClipDropCm = 0.f;                    // hip drop the crouch-walk clip was made for (cm, this mannequin's scale)
	FTransform GripRelOut = FTransform::Identity;   // rifle relative to hand_r for the game thread (differs from GripRel during the bolt cycle)
	bool bBoltOut = false;
	FTransform KarW0 = FTransform::Identity;      // rifle frame (component space) kept still while the right hand leaves the grip (reload / bolt)
	FTransform SniperW0 = FTransform::Identity;
	float MagIKW = 0.f;                           // smoothed left-hand-to-magazine IK weight
	// Kar98k handling secondary motion: the rifle dips / rolls / pitches on each bolt stroke and each round pressed in,
	// then settles (under-damped springs, rifle-local cm / deg)
	float KarDip = 0.f, KarDipV = 0.f, KarRoll = 0.f, KarRollV = 0.f, KarPitch = 0.f, KarPitchV = 0.f;
	float PrevKarShotT = -1.f, PrevKarReloadT = -1.f;
	float PrevKarStepPh = 0.f;            // footsteps / landings shake the rifle too (reload: only the left hand holds it)
	bool bKarStepL = false, bKarWasAir = false;
	FTransform KarJolt() const;
	// grenade throw secondary motion: torso yaw / pitch springs chase keyframed targets and overshoot
	float ThrowYaw = 0.f, ThrowYawV = 0.f;     // deg, + = turn right
	float ThrowPitch = 0.f, ThrowPitchV = 0.f; // deg, + = lean back
	float CookW = 0.f;                         // smoothed cook-hold pose weight
	float CrouchW = 0.f, CrouchV = 0.f;        // crouch depth (spring: drops quickly, a little settle at the bottom)
	float LeanW = 0.f;                         // smoothed corner lean
	float ThrowStartCook = 0.f;                // cook weight at the moment of release (throw starts from the chest)
	float PrevThrowT = -1.f;
	float RefSince = 9.f;                    // seconds since the last shot (after the first swing everything is damped hard: no ringing)

	// calibration result (anim thread writes once, the game thread reads after evaluation)
	bool bCalibrated = false;
	FTransform GripRel = FTransform::Identity;   // rifle frame relative to the hand_r bone
	FTransform LeftRel = FTransform::Identity;   // hand_l bone relative to the rifle frame
	float LeftDist = 30.f;                       // cm from the grip to the left palm along the barrel
};

UCLASS(Transient, NotBlueprintable)
class AAAWEAPONS_API UAAARifleAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	FAAARifleInputs Inputs;

	/** calibration (valid when bCalibrated) - read by the weapon component */
	bool bCalibrated = false;
	FTransform GripRel = FTransform::Identity;
	float LeftDist = 30.f;
	/** all the rifle animations were found and fit this skeleton */
	bool bAssetsOk = false;

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Walk;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Jog;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Fall;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Reload;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Equip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> JumpStart;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Land;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> HitReacts;

	float GetReloadLength() const;
	float GetEquipLength() const;

	/** death clip for a hit from HitDir (degrees from facing, see FAAARifleInputs) - INDEX_NONE when none is available */
	static int32 PickDeath(float HitDir);
	/** length of that death clip (s), 0 when none */
	static float GetDeathLength(int32 Index);
	/** angle (degrees, 0 front, +90 right) of a point seen from an actor */
	static float DirFrom(const AActor* Self, const FVector& From);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativePostEvaluateAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	friend struct FAAARifleProxy;
};
