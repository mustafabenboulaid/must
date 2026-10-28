// Ghosts of November — animation and camera maths (pure C++), ported from the prototype's
// native proxy (Reference/AAAWeapons/.../AAARifleAnimInstance.cpp) so it can be unit-tested.
// UGoNAnimInstance's proxy evaluates poses; every decision about weights, rates, phases,
// steps and offsets is made here.
#pragma once

#include "Logic/GoNMovementLogic.h"

#include <array>
#include <vector>

namespace GoN::Character
{
	// ---- Locomotion: idle <-> 8-way walk / jog on one synced gait phase -------------------

	constexpr int LocomotionDirections = 8;
	using FDirectionValues = std::array<double, LocomotionDirections>;

	// Clip suffix for a direction, prototype order: angle 0 = forward, +45 per step clockwise
	// (UE local velocity: atan2(Y, X), +Y = right). "Fwd", "Fwd_Right", "Right", ...
	const char* LocomotionDirectionSuffix(int Index);

	struct FGaitRateParams
	{
		double GainAboveClip = 0.45; // above the clip's speed, lengthen the stride rather than spin the legs
		double MaxRate = 1.22;       // walk/jog cap (prototype)
		double CrouchMaxRate = 2.4;  // crouched: cadence follows speed exactly (no sliding)
	};

	struct FGaitBlend
	{
		int Dir0 = 0;
		int Dir1 = 1;
		double DirAlpha = 0.0;  // smoothstepped blend Dir0 -> Dir1
		double JogWeight = 0.0; // 0 walk set, 1 jog set
		double ClipSpeed = 0.0; // speed the blended clips naturally move at
		double ClipLength = 1.0;
		double PlayRate = 0.0;
	};

	// Speed in cm/s, AngleDeg in [0, 360). Clip speeds are measured from the clips' planted foot.
	FGaitBlend ComputeGaitBlend(double Speed, double AngleDeg, double CrouchWeight, const FDirectionValues& WalkSpeeds, const FDirectionValues& JogSpeeds, const FDirectionValues& WalkLengths, const FDirectionValues& JogLengths, const FGaitRateParams& Params);

	// Advances a normalised phase [0, 1) by Dt at PlayRate for a clip of ClipLength seconds.
	double AdvancePhase(double Phase, double Dt, double PlayRate, double ClipLength);

	// Velocity -> direction angle in [0, 360), 0 = forward, 90 = right.
	double DirectionAngle(double LocalVelX, double LocalVelY);

	// The planted-foot speed of a clip from sampled foot positions (component space, Up = Z):
	// per sample the lower foot is on the ground; its horizontal speed is measured; median.
	// Clamped to [40, 900] like the prototype.
	struct FFootSample
	{
		double LX = 0.0, LY = 0.0, LZ = 0.0;
		double RX = 0.0, RY = 0.0, RZ = 0.0;
	};
	double MeasureClipSpeed(const std::vector<FFootSample>& Samples, double ClipLength);

	// ---- Crouch weight: stiff, slightly under-damped spring (prototype) ------------------------

	void StepCrouchSpring(double Target, double Dt, double& Weight, double& Velocity);

	// ---- Turn in place: procedural foot cycle driven by the capsule's yaw (prototype) ---------

	struct FTurnInPlace
	{
		double Phase = 0.0;  // 0..1, touchdowns at 0.5 and 1.0
		double Weight = 0.0; // 0..1
		double Direction = 0.0;
		double LastYaw = 0.0;
		bool bInitialised = false;
		double DegreesPerCycle = 64.0; // 32 degrees per single step

		// Returns the yaw delta this frame (degrees). bStanding: idle and on the ground.
		double Update(double YawDeg, double Dt, bool bStanding);
	};

	// ---- Planted feet: feet stay on the ground and step when the pose wants them elsewhere -----

	struct FFootPlantParams
	{
		double StepDistance = 8.0;   // cm off the pose's foot -> step
		double StepAngle = 25.0;     // degrees
		double SettleAfter = 0.35;   // after this pause a smaller error triggers a settle step
		double SettleScore = 0.30;
		double StepSeconds = 0.24;
		double StepLift = 5.0;       // cm at full span
	};

	struct FFootPlanter
	{
		struct FFoot
		{
			double X = 0.0, Y = 0.0, Yaw = 0.0;     // planted (world)
			double FromX = 0.0, FromY = 0.0, FromYaw = 0.0;
			double StepT = -1.0;                    // < 0 = planted, else 0..1
		};

		struct FTarget
		{
			double X = 0.0, Y = 0.0, Yaw = 0.0, Lift = 0.0;
			bool bStepping = false;
		};

		FFoot Feet[2];
		double SettleTime = 0.0;
		bool bInitialised = false;
		FFootPlantParams Params;

		void Reset() { bInitialised = false; }

		// Home = where the current pose puts each foot (world X/Y/yaw). 0 = left, 1 = right.
		void Update(const double HomeX[2], const double HomeY[2], const double HomeYaw[2], double Dt, FTarget OutTargets[2]);
	};

	// ---- Ground IK on steps and slopes (prototype: the capsule stands on the higher foot) ------

	struct FGroundIKParams
	{
		double MinDifference = 2.2;  // below: flat ground, no IK
		double MaxDifference = 32.0; // above: not a step the feet can span
		double MaxDrop = 26.0;
		double PelvisShare = 0.55;
	};

	struct FGroundIKTarget
	{
		double OffsetL = 0.0; // foot targets relative to the pose (negative = lower)
		double OffsetR = 0.0;
		double Pelvis = 0.0;
		double Weight = 0.0;  // target weight; the anim instance interpolates
	};

	// Hit heights are relative to the capsule bottom (the feet).
	FGroundIKTarget ComputeGroundIK(bool bHitL, double HitL, bool bHitR, double HitR, const FGroundIKParams& Params);

	// ---- IK/FK guard ------------------------------------------------------------------------

	// 1 while the two-bone chain can reach the target, fading to 0 (pure FK) over FadeDistance
	// beyond its reach: a hand never snaps to a target it cannot reach.
	double IKReachAlpha(double TargetDistance, double ChainLength, double FadeDistance = 15.0, double Slack = 0.985);

	// ---- Head look-at ---------------------------------------------------------------------------

	struct FLookAtLimits
	{
		double MaxYaw = 70.0;
		double MaxPitchUp = 35.0;
		double MaxPitchDown = 40.0;
		double FadeStartYaw = 100.0; // behind the shoulder the head returns forward
		double FadeEndYaw = 135.0;
	};

	struct FLookAt
	{
		double Yaw = 0.0;
		double Pitch = 0.0;
		double Weight = 0.0;
	};

	// Target direction relative to the body (yaw: + right, pitch: + up), degrees.
	FLookAt ComputeLookAt(double TargetYawDeg, double TargetPitchDeg, const FLookAtLimits& Limits);

	// ---- Third-person camera rig ----------------------------------------------------------------

	struct FCameraProfile
	{
		double ArmLength = 300.0;
		double SprintArmLength = 340.0;
		double CoverArmLength = 240.0;
		double ProneArmLength = 230.0;
		double ShoulderOffset = 55.0;      // to the side (+ right shoulder)
		double CoverShoulderOffset = 80.0; // wider in cover: see past the edge
		double PivotHeightStand = 150.0;   // above the feet
		double PivotHeightCrouch = 105.0;
		double PivotHeightProne = 55.0;
		double Fov = 80.0;
		double SprintFov = 86.0;
		double InterpSpeed = 8.0;
		double ShoulderSwapSpeed = 10.0;
	};

	struct FCameraRig
	{
		double ArmLength = 0.0;
		double SideOffset = 0.0;
		double PivotHeight = 0.0; // above the feet
		double Fov = 0.0;
	};

	// ShoulderSide: smoothed -1 (left) .. +1 (right).
	FCameraRig ComputeCameraRig(const FCameraProfile& Profile, EStance Stance, bool bInCover, bool bSprinting, double ShoulderSide);

	// Frame-rate independent exponential approach (UE FInterpTo semantics).
	double InterpTo(double Current, double Target, double Dt, double Speed);
} // namespace GoN::Character
