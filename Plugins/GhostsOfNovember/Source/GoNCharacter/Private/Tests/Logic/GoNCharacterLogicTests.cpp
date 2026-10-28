// Pure-logic tests for GoNCharacter. Run by Tools/LogicTests and "GoN.Character.Logic".
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNAnimLogic.h"
#include "Logic/GoNMovementLogic.h"

#include <cmath>

namespace GoNCharacterLogicTestsDetail
{
	using namespace GoN::Character;

	FTraverseProbe Obstacle(double Height, double Depth, bool bLanding)
	{
		FTraverseProbe Probe;
		Probe.bObstacle = true;
		Probe.ObstacleDistance = 20.0;
		Probe.bTopFound = true;
		Probe.TopHeight = Height;
		Probe.Depth = Depth;
		Probe.bLandingFound = bLanding;
		Probe.bLandingClear = bLanding;
		Probe.bRoomStandingOnTop = true;
		Probe.bRoomCrouchedOnTop = true;
		Probe.bHeadroomClear = true;
		return Probe;
	}

	bool Near(double A, double B, double Tolerance = 1e-6)
	{
		return std::fabs(A - B) <= Tolerance;
	}

	GON_LOGIC_TEST("Character.Movement.DefaultMetricsAreConsistent", Character_Movement_Defaults)
	{
		for (const std::string& Issue : ValidateMetrics(FMovementMetrics()))
		{
			Ctx.Fail(__FILE__, __LINE__, Issue);
		}
		FMovementMetrics Bad;
		Bad.SprintSpeed = 100.0;
		Bad.ProneHalfHeight = 20.0;
		GON_EXPECT(ValidateMetrics(Bad).size() >= 2);
	}

	GON_LOGIC_TEST("Character.Movement.SpeedsPerStanceAndCarry", Character_Movement_Speeds)
	{
		const FMovementMetrics M;
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Stand, EGait::Walk, false), M.WalkSpeed);
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Stand, EGait::Jog, false), M.JogSpeed);
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Stand, EGait::Sprint, false), M.SprintSpeed);
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Crouch, EGait::Sprint, false), M.CrouchSpeed);
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Prone, EGait::Jog, false), M.ProneSpeed);
		// Carrying: slower, and sprint falls back to the carry jog.
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Stand, EGait::Sprint, true), M.CarryJogSpeed);
		GON_EXPECT_EQ(MaxSpeed(M, EStance::Stand, EGait::Walk, true), M.CarryWalkSpeed);
		GON_EXPECT(ResolveGait(EGait::Sprint, EStance::Stand, true) == EGait::Jog);
		GON_EXPECT(ResolveGait(EGait::Sprint, EStance::Crouch, false) == EGait::Walk);
		GON_EXPECT_EQ(HalfHeightFor(M, EStance::Prone), M.CapsuleRadius);
	}

	GON_LOGIC_TEST("Character.Traverse.GymVaultHeights", Character_Traverse_Vaults)
	{
		const FMovementMetrics M;
		const FTraverseDecision Low = DecideTraverse(Obstacle(60.0, 40.0, true), M);
		const FTraverseDecision Mid = DecideTraverse(Obstacle(90.0, 40.0, true), M);
		const FTraverseDecision High = DecideTraverse(Obstacle(120.0, 40.0, true), M);
		GON_EXPECT(Low.Action == ETraverseAction::Vault && Low.VaultKind == EVaultKind::Low);
		GON_EXPECT(Mid.Action == ETraverseAction::Vault && Mid.VaultKind == EVaultKind::Mid);
		GON_EXPECT(High.Action == ETraverseAction::Vault && High.VaultKind == EVaultKind::High);
		GON_EXPECT(Low.Duration < High.Duration);
	}

	GON_LOGIC_TEST("Character.Traverse.GymLedgesClimbAndRefuse", Character_Traverse_Ledges)
	{
		const FMovementMetrics M;
		GON_EXPECT(DecideTraverse(Obstacle(150.0, 200.0, false), M).Action == ETraverseAction::Mantle);
		GON_EXPECT(DecideTraverse(Obstacle(200.0, 200.0, false), M).Action == ETraverseAction::Mantle);
		const FTraverseDecision TooHigh = DecideTraverse(Obstacle(250.0, 200.0, false), M);
		GON_EXPECT(TooHigh.Action == ETraverseAction::Refused);
		GON_EXPECT(TooHigh.Reason.find("too high") != std::string::npos);
	}

	GON_LOGIC_TEST("Character.Traverse.Rules", Character_Traverse_Rules)
	{
		const FMovementMetrics M;
		GON_EXPECT(DecideTraverse(FTraverseProbe(), M).Action == ETraverseAction::None);
		GON_EXPECT(DecideTraverse(Obstacle(20.0, 30.0, true), M).Action == ETraverseAction::None); // a stair step

		FTraverseProbe Carrying = Obstacle(90.0, 40.0, true);
		Carrying.bCarrying = true;
		GON_EXPECT(DecideTraverse(Carrying, M).Action == ETraverseAction::Refused);
		FTraverseProbe Prone = Obstacle(60.0, 40.0, true);
		Prone.Stance = EStance::Prone;
		GON_EXPECT(DecideTraverse(Prone, M).Action == ETraverseAction::Refused);

		// Thick 100 cm block: climbed onto, not vaulted.
		GON_EXPECT(DecideTraverse(Obstacle(100.0, 150.0, false), M).Action == ETraverseAction::Mantle);
		// Thin wall without a landing and too narrow to stand on: refused.
		GON_EXPECT(DecideTraverse(Obstacle(90.0, 30.0, false), M).Action == ETraverseAction::Refused);
		// Low ceiling over the ledge: climb ends crouched; no room at all: refused.
		FTraverseProbe LowCeiling = Obstacle(150.0, 200.0, false);
		LowCeiling.bRoomStandingOnTop = false;
		const FTraverseDecision Crouched = DecideTraverse(LowCeiling, M);
		GON_EXPECT(Crouched.Action == ETraverseAction::Mantle && Crouched.EndStance == EStance::Crouch);
		LowCeiling.bRoomCrouchedOnTop = false;
		GON_EXPECT(DecideTraverse(LowCeiling, M).Action == ETraverseAction::Refused);
		FTraverseProbe Blocked = Obstacle(90.0, 40.0, true);
		Blocked.bHeadroomClear = false;
		GON_EXPECT(DecideTraverse(Blocked, M).Action == ETraverseAction::Refused);
	}

	GON_LOGIC_TEST("Character.Traverse.PathsClearTheObstacle", Character_Traverse_Paths)
	{
		const FMovementMetrics M;
		const FTraverseProbe VaultProbe = Obstacle(120.0, 40.0, true);
		const FTraverseDecision Vault = DecideTraverse(VaultProbe, M);
		const std::vector<FPathKey> VaultPath = BuildTraversePath(Vault, VaultProbe, M);
		GON_REQUIRE(VaultPath.size() == 3);
		const double Face = VaultProbe.ObstacleDistance + M.CapsuleRadius;
		// Capsule bottom (centre - half height, relative to the start feet) clears the top at the apex.
		GON_EXPECT(VaultPath[1].Up >= Vault.Height + M.VaultClearance - 1e-6);
		GON_EXPECT(VaultPath[1].Forward > Face && VaultPath[1].Forward < Face + VaultProbe.Depth);
		// Lands beyond the obstacle with the capsule clear of its far face.
		GON_EXPECT(VaultPath[2].Forward - M.CapsuleRadius >= Face + VaultProbe.Depth);
		GON_EXPECT(Near(VaultPath[2].Up, 0.0));

		const FTraverseProbe LedgeProbe = Obstacle(200.0, 200.0, false);
		const FTraverseDecision Climb = DecideTraverse(LedgeProbe, M);
		const std::vector<FPathKey> ClimbPath = BuildTraversePath(Climb, LedgeProbe, M);
		GON_REQUIRE(ClimbPath.size() == 3);
		// Rises against the face before moving over the top.
		GON_EXPECT(ClimbPath[1].Forward + M.CapsuleRadius <= Face + 1e-6);
		GON_EXPECT(ClimbPath[1].Up >= Climb.Height);
		GON_EXPECT(Near(ClimbPath[2].Up, Climb.Height));
		GON_EXPECT(ClimbPath[2].Forward - M.CapsuleRadius > Face);

		GON_EXPECT(Near(EvaluateTraversePath(ClimbPath, 0.0).Up, 0.0));
		GON_EXPECT(Near(EvaluateTraversePath(ClimbPath, 1.0).Forward, ClimbPath[2].Forward));
		GON_EXPECT(Near(EvaluateTraversePath(ClimbPath, 0.65).Up, ClimbPath[1].Up));
		// Rises without dipping until it clears the top (then settles 2 cm onto it).
		double Last = -1.0;
		bool bMonotonic = true;
		for (int Step = 0; Step <= 13; ++Step)
		{
			const double Up = EvaluateTraversePath(ClimbPath, Step / 20.0).Up;
			bMonotonic = bMonotonic && Up >= Last - 1e-9;
			Last = Up;
		}
		GON_EXPECT(bMonotonic);
	}

	GON_LOGIC_TEST("Character.Anim.GaitBlendAndPlayRate", Character_Anim_Gait)
	{
		FDirectionValues Walk;
		FDirectionValues Jog;
		FDirectionValues Lengths;
		Walk.fill(150.0);
		Jog.fill(380.0);
		Lengths.fill(1.0);
		const FGaitRateParams Params;

		const FGaitBlend AtWalk = ComputeGaitBlend(150.0, 0.0, 0.0, Walk, Jog, Lengths, Lengths, Params);
		GON_EXPECT(AtWalk.Dir0 == 0 && Near(AtWalk.DirAlpha, 0.0));
		GON_EXPECT(Near(AtWalk.JogWeight, 0.0));
		GON_EXPECT(Near(AtWalk.PlayRate, 1.0));

		const FGaitBlend AtJog = ComputeGaitBlend(380.0, 90.0, 0.0, Walk, Jog, Lengths, Lengths, Params);
		GON_EXPECT(AtJog.Dir0 == 2); // right
		GON_EXPECT(Near(AtJog.JogWeight, 1.0));
		GON_EXPECT(Near(AtJog.PlayRate, 1.0));

		// Above the clip speed the cadence rises gently and is capped.
		const FGaitBlend Fast = ComputeGaitBlend(1000.0, 0.0, 0.0, Walk, Jog, Lengths, Lengths, Params);
		GON_EXPECT(Near(Fast.PlayRate, Params.MaxRate));
		// Between two directions.
		const FGaitBlend Diagonal = ComputeGaitBlend(150.0, 22.5, 0.0, Walk, Jog, Lengths, Lengths, Params);
		GON_EXPECT(Diagonal.Dir0 == 0 && Diagonal.Dir1 == 1 && Near(Diagonal.DirAlpha, 0.5));
		// Crouched at jog speed: walk set only, cadence matches the speed exactly.
		const FGaitBlend Crouched = ComputeGaitBlend(300.0, 0.0, 1.0, Walk, Jog, Lengths, Lengths, Params);
		GON_EXPECT(Near(Crouched.JogWeight, 0.0));
		GON_EXPECT(Near(Crouched.PlayRate, 2.0));

		GON_EXPECT(Near(AdvancePhase(0.9, 0.2, 1.0, 1.0), 0.1, 1e-9));
		GON_EXPECT(Near(DirectionAngle(0.0, 100.0), 90.0));
		GON_EXPECT(Near(DirectionAngle(0.0, -100.0), 270.0));
		GON_EXPECT_EQ(std::string(LocomotionDirectionSuffix(6)), std::string("Left"));
	}

	GON_LOGIC_TEST("Character.Anim.MeasureClipSpeedFromPlantedFoot", Character_Anim_Measure)
	{
		// Left foot planted (lower) sliding back at 150 cm/s for the first half, then the right.
		std::vector<FFootSample> Samples;
		const double Length = 1.0;
		const int Count = 32;
		for (int Index = 0; Index < Count; ++Index)
		{
			const double T = Length * Index / Count;
			FFootSample Sample;
			const bool bLeftDown = Index < Count / 2;
			Sample.LX = -150.0 * T;
			Sample.LZ = bLeftDown ? 0.0 : 10.0;
			Sample.RX = -150.0 * T;
			Sample.RZ = bLeftDown ? 10.0 : 0.0;
			Samples.push_back(Sample);
		}
		GON_EXPECT(Near(MeasureClipSpeed(Samples, Length), 150.0, 1e-6));
		GON_EXPECT(Near(MeasureClipSpeed({}, Length), 40.0));
	}

	GON_LOGIC_TEST("Character.Anim.CrouchSpringSettles", Character_Anim_CrouchSpring)
	{
		double Weight = 0.0;
		double Velocity = 0.0;
		double Peak = 0.0;
		for (int Frame = 0; Frame < 120; ++Frame)
		{
			StepCrouchSpring(1.0, 1.0 / 60.0, Weight, Velocity);
			Peak = std::max(Peak, Weight);
		}
		GON_EXPECT(Near(Weight, 1.0, 0.01));
		GON_EXPECT(Peak > 1.0 && Peak <= 1.08); // slightly under-damped: sinks a touch past the pose
	}

	GON_LOGIC_TEST("Character.Anim.TurnInPlaceSteps", Character_Anim_Turn)
	{
		FTurnInPlace Turn;
		Turn.Update(0.0, 1.0 / 60.0, true);
		for (int Frame = 1; Frame <= 30; ++Frame)
		{
			Turn.Update(Frame * 3.0, 1.0 / 60.0, true); // 90 degrees in half a second
		}
		GON_EXPECT(Turn.Weight > 0.9);
		GON_EXPECT(Turn.Direction > 0.9);
		GON_EXPECT(Turn.Phase > 0.0);
		for (int Frame = 0; Frame < 120; ++Frame)
		{
			Turn.Update(90.0, 1.0 / 60.0, true);
		}
		GON_EXPECT(Turn.Weight < 0.01);
		GON_EXPECT(Near(Turn.Phase, 0.5, 1e-6) || Near(Turn.Phase, 0.0, 1e-6)); // ended on a touchdown
		// Wrap-around: 179 -> -179 is a 2 degree turn, not 358.
		FTurnInPlace Wrap;
		Wrap.Update(179.0, 1.0 / 60.0, true);
		GON_EXPECT(Near(Wrap.Update(-179.0, 1.0 / 60.0, true), 2.0, 1e-9));
	}

	GON_LOGIC_TEST("Character.Anim.PlantedFeetStepOneAtATime", Character_Anim_Feet)
	{
		FFootPlanter Planter;
		FFootPlanter::FTarget Targets[2];
		double HomeX[2] = {0.0, 0.0};
		double HomeY[2] = {-12.0, 12.0};
		double HomeYaw[2] = {0.0, 0.0};
		Planter.Update(HomeX, HomeY, HomeYaw, 1.0 / 60.0, Targets);
		// Small sway: the feet stay planted.
		HomeX[0] = 3.0;
		Planter.Update(HomeX, HomeY, HomeYaw, 1.0 / 60.0, Targets);
		GON_EXPECT(!Targets[0].bStepping && Near(Targets[0].X, 0.0));
		// The body turns 40 degrees: one foot steps, then the other.
		HomeYaw[0] = HomeYaw[1] = 40.0;
		int MaxStepping = 0;
		int StepsStarted = 0;
		bool bWasStepping[2] = {false, false};
		double MaxLift = 0.0;
		for (int Frame = 0; Frame < 120; ++Frame)
		{
			Planter.Update(HomeX, HomeY, HomeYaw, 1.0 / 60.0, Targets);
			MaxStepping = std::max(MaxStepping, (Targets[0].bStepping ? 1 : 0) + (Targets[1].bStepping ? 1 : 0));
			for (int Foot = 0; Foot < 2; ++Foot)
			{
				StepsStarted += (Targets[Foot].bStepping && !bWasStepping[Foot]) ? 1 : 0;
				bWasStepping[Foot] = Targets[Foot].bStepping;
				MaxLift = std::max(MaxLift, Targets[Foot].Lift);
			}
		}
		GON_EXPECT_EQ(MaxStepping, 1);
		GON_EXPECT(StepsStarted >= 2);
		GON_EXPECT(MaxLift > 0.0 && MaxLift <= Planter.Params.StepLift);
		GON_EXPECT(Near(Targets[0].Yaw, 40.0) && Near(Targets[1].Yaw, 40.0));
	}

	GON_LOGIC_TEST("Character.Anim.GroundIKOnStairsAndRamps", Character_Anim_GroundIK)
	{
		const FGroundIKParams Params;
		const FGroundIKTarget Flat = ComputeGroundIK(true, 0.0, true, 1.0, Params);
		GON_EXPECT(Near(Flat.Weight, 0.0) && Near(Flat.Pelvis, 0.0));
		// Stairs: the capsule stands on the upper step; the lower foot reaches down 20 cm.
		const FGroundIKTarget Stairs = ComputeGroundIK(true, 0.0, true, -20.0, Params);
		GON_EXPECT(Near(Stairs.Weight, 1.0));
		GON_EXPECT(Near(Stairs.Pelvis, -11.0));
		GON_EXPECT(Near(Stairs.OffsetL, 11.0) && Near(Stairs.OffsetR, -9.0));
		GON_EXPECT(Near(Stairs.Pelvis + Stairs.OffsetR, -20.0)); // foot ends on the lower step
		// A drop deeper than a step is ignored; a missing hit too.
		GON_EXPECT(Near(ComputeGroundIK(true, 0.0, true, -40.0, Params).Weight, 0.0));
		GON_EXPECT(Near(ComputeGroundIK(false, 0.0, true, -10.0, Params).Weight, 0.0));
	}

	GON_LOGIC_TEST("Character.Anim.ReachGuardAndLookAt", Character_Anim_Guard)
	{
		GON_EXPECT(Near(IKReachAlpha(50.0, 60.0), 1.0));
		GON_EXPECT(Near(IKReachAlpha(60.0 * 0.985 + 7.5, 60.0), 0.5, 1e-9));
		GON_EXPECT(Near(IKReachAlpha(200.0, 60.0), 0.0));

		const FLookAtLimits Limits;
		const FLookAt Ahead = ComputeLookAt(30.0, 10.0, Limits);
		GON_EXPECT(Near(Ahead.Yaw, 30.0) && Near(Ahead.Pitch, 10.0) && Near(Ahead.Weight, 1.0));
		const FLookAt Side = ComputeLookAt(90.0, -80.0, Limits);
		GON_EXPECT(Near(Side.Yaw, Limits.MaxYaw) && Near(Side.Pitch, -Limits.MaxPitchDown));
		GON_EXPECT(Near(ComputeLookAt(180.0, 0.0, Limits).Weight, 0.0));
		GON_EXPECT(Near(ComputeLookAt(-170.0, 0.0, Limits).Weight, 0.0));
	}

	GON_LOGIC_TEST("Character.Camera.RigPerStanceShoulderAndCover", Character_Camera_Rig)
	{
		const FCameraProfile Profile;
		const FCameraRig Stand = ComputeCameraRig(Profile, EStance::Stand, false, false, 1.0);
		GON_EXPECT(Near(Stand.ArmLength, Profile.ArmLength) && Near(Stand.SideOffset, Profile.ShoulderOffset));
		GON_EXPECT(Near(Stand.PivotHeight, Profile.PivotHeightStand));
		const FCameraRig Swapped = ComputeCameraRig(Profile, EStance::Stand, false, false, -1.0);
		GON_EXPECT(Near(Swapped.SideOffset, -Profile.ShoulderOffset));
		const FCameraRig Crouch = ComputeCameraRig(Profile, EStance::Crouch, false, true, 1.0);
		GON_EXPECT(Near(Crouch.PivotHeight, Profile.PivotHeightCrouch));
		GON_EXPECT(Near(Crouch.Fov, Profile.Fov)); // no sprint FOV while crouched
		const FCameraRig Cover = ComputeCameraRig(Profile, EStance::Crouch, true, false, 1.0);
		GON_EXPECT(Near(Cover.SideOffset, Profile.CoverShoulderOffset) && Near(Cover.ArmLength, Profile.CoverArmLength));
		const FCameraRig Sprint = ComputeCameraRig(Profile, EStance::Stand, false, true, 1.0);
		GON_EXPECT(Near(Sprint.ArmLength, Profile.SprintArmLength) && Near(Sprint.Fov, Profile.SprintFov));
		GON_EXPECT(Near(ComputeCameraRig(Profile, EStance::Prone, false, false, 0.0).PivotHeight, Profile.PivotHeightProne));
		GON_EXPECT(Near(InterpTo(0.0, 10.0, 0.05, 10.0), 5.0));
	}
} // namespace GoNCharacterLogicTestsDetail

#endif // GON_WITH_LOGIC_TESTS
