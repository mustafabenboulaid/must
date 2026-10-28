#include "Logic/GoNMovementGymCheck.h"

#include <cstdio>

namespace GoN::Blockout
{
	namespace MetricsCheckDetail
	{
		std::string Format(const char* Pattern, double A, double B = 0.0)
		{
			char Buffer[160];
			std::snprintf(Buffer, sizeof(Buffer), Pattern, A, B);
			return Buffer;
		}

		void Add(std::vector<FMetricCheck>& Out, bool bOk, const std::string& Text, EMetricResult WhenNotOk = EMetricResult::Fail)
		{
			Out.push_back(FMetricCheck{bOk ? EMetricResult::Ok : WhenNotOk, Text});
		}
	} // namespace MetricsCheckDetail

	GoN::Character::FTraverseProbe GymObstacleProbe(double Height, double Depth, bool bLandingBehind)
	{
		GoN::Character::FTraverseProbe Probe;
		Probe.bObstacle = true;
		Probe.ObstacleDistance = 20.0;
		Probe.bTopFound = true;
		Probe.TopHeight = Height;
		Probe.Depth = Depth;
		Probe.bLandingFound = bLandingBehind;
		Probe.LandingHeight = 0.0;
		Probe.bLandingClear = bLandingBehind;
		Probe.bRoomStandingOnTop = true;
		Probe.bRoomCrouchedOnTop = true;
		Probe.bHeadroomClear = true;
		return Probe;
	}

	std::vector<FMetricCheck> CompareMovementWithGym(const GoN::Character::FMovementMetrics& Movement, const FGymMetrics& Gym)
	{
		using namespace GoN::Character;
		using MetricsCheckDetail::Add;
		using MetricsCheckDetail::Format;
		std::vector<FMetricCheck> Out;

		for (const std::string& Issue : ValidateMetrics(Movement))
		{
			Add(Out, false, "movement metrics: " + Issue);
		}

		Add(Out, Movement.CapsuleRadius == Gym.CapsuleRadius && Movement.StandHalfHeight == Gym.CapsuleHalfHeight,
			Format("capsule radius %.0f / half height %.0f", Movement.CapsuleRadius, Movement.StandHalfHeight) + Format(" vs gym %.0f / %.0f", Gym.CapsuleRadius, Gym.CapsuleHalfHeight));
		Add(Out, Movement.MaxStepHeight == Gym.MaxStepHeight, Format("max step height %.0f vs gym %.0f cm", Movement.MaxStepHeight, Gym.MaxStepHeight));
		Add(Out, Movement.WalkableSlopeDeg == Gym.WalkableSlopeDeg, Format("walkable slope %.3f vs gym %.3f deg", Movement.WalkableSlopeDeg, Gym.WalkableSlopeDeg));
		Add(Out, Gym.StepRise <= Movement.MaxStepHeight, Format("gym stairs rise %.0f cm <= max step %.0f cm: walked", Gym.StepRise, Movement.MaxStepHeight));

		// The gym vault blocks are 40 cm thick with open floor behind them.
		for (const double Height : Gym.VaultHeights)
		{
			const FTraverseDecision Decision = DecideTraverse(GymObstacleProbe(Height, 40.0, true), Movement);
			Add(Out, Decision.Action == ETraverseAction::Vault, Format("gym vault %.0f cm -> ", Height) + Decision.ToString());
		}
		// The gym ledges are 200 cm deep blocks: climbed up to MaxClimbHeight, refused above.
		bool bHasRefusedLedge = false;
		for (const double Height : Gym.LedgeHeights)
		{
			const FTraverseDecision Decision = DecideTraverse(GymObstacleProbe(Height, 200.0, false), Movement);
			const ETraverseAction Expected = Height <= Movement.MaxClimbHeight ? ETraverseAction::Mantle : ETraverseAction::Refused;
			bHasRefusedLedge = bHasRefusedLedge || Expected == ETraverseAction::Refused;
			Add(Out, Decision.Action == Expected, Format("gym ledge %.0f cm -> ", Height) + Decision.ToString());
		}
		Add(Out, bHasRefusedLedge, Format("gym has a ledge above max climb %.0f cm (the 'refuse' reference)", Movement.MaxClimbHeight), EMetricResult::Note);

		for (const double Angle : Gym.RampAnglesDeg)
		{
			const bool bWalkable = Angle <= Movement.WalkableSlopeDeg;
			Out.push_back(FMetricCheck{EMetricResult::Ok, Format("gym ramp %.0f deg: ", Angle) + (bWalkable ? "walkable" : "too steep, the character slides off")});
		}
		for (const double Width : Gym.DoorWidths)
		{
			Add(Out, Width >= Movement.CapsuleRadius * 2.0 + 4.0, Format("gym door %.0f cm vs capsule %.0f cm wide", Width, Movement.CapsuleRadius * 2.0));
		}

		const double StandHeight = Movement.StandHalfHeight * 2.0;
		const double CrouchHeight = HalfHeightFor(Movement, EStance::Crouch) * 2.0;
		const double ProneHeight = HalfHeightFor(Movement, EStance::Prone) * 2.0;
		Add(Out, StandHeight <= Gym.HighCoverHeight, Format("standing capsule %.0f cm hidden by high cover %.0f cm", StandHeight, Gym.HighCoverHeight));
		Add(Out, CrouchHeight <= Gym.LowCoverHeight, Format("crouched capsule %.0f cm vs low cover %.0f cm (taller: seen over it until the M3 cover pose)", CrouchHeight, Gym.LowCoverHeight), EMetricResult::Note);
		Out.push_back(FMetricCheck{EMetricResult::Ok, Format("prone capsule %.0f cm tall (UE clamps the half height to the radius %.0f)", ProneHeight, Movement.CapsuleRadius)});
		return Out;
	}

	int CountResults(const std::vector<FMetricCheck>& Checks, EMetricResult Result)
	{
		int Count = 0;
		for (const FMetricCheck& Check : Checks)
		{
			Count += Check.Result == Result ? 1 : 0;
		}
		return Count;
	}
} // namespace GoN::Blockout
