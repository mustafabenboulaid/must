#include "Logic/GoNAnimLogic.h"

#include <algorithm>
#include <cmath>

namespace GoN::Character
{
	namespace AnimDetail
	{
		constexpr double Pi = 3.14159265358979323846;

		double SmoothStep01(double X)
		{
			const double T = std::clamp(X, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		double Lerp(double A, double B, double T)
		{
			return A + (B - A) * T;
		}

		double Frac(double X)
		{
			return X - std::floor(X);
		}

		double NormalizeAxis(double Degrees)
		{
			double Out = std::fmod(Degrees + 180.0, 360.0);
			if (Out < 0.0)
			{
				Out += 360.0;
			}
			return Out - 180.0;
		}
	} // namespace AnimDetail

	const char* LocomotionDirectionSuffix(int Index)
	{
		static const char* const Suffixes[LocomotionDirections] = {"Fwd", "Fwd_Right", "Right", "Bwd_Right", "Bwd", "Bwd_Left", "Left", "Fwd_Left"};
		return (Index >= 0 && Index < LocomotionDirections) ? Suffixes[Index] : "Fwd";
	}

	double DirectionAngle(double LocalVelX, double LocalVelY)
	{
		double Angle = std::atan2(LocalVelY, LocalVelX) * 180.0 / AnimDetail::Pi;
		if (Angle < 0.0)
		{
			Angle += 360.0;
		}
		return Angle >= 360.0 ? 0.0 : Angle;
	}

	FGaitBlend ComputeGaitBlend(double Speed, double AngleDeg, double CrouchWeight, const FDirectionValues& WalkSpeeds, const FDirectionValues& JogSpeeds, const FDirectionValues& WalkLengths, const FDirectionValues& JogLengths, const FGaitRateParams& Params)
	{
		using namespace AnimDetail;
		FGaitBlend Blend;
		double Angle = std::fmod(AngleDeg, 360.0);
		if (Angle < 0.0)
		{
			Angle += 360.0;
		}
		Blend.Dir0 = static_cast<int>(std::floor(Angle / 45.0)) % LocomotionDirections;
		Blend.Dir1 = (Blend.Dir0 + 1) % LocomotionDirections;
		Blend.DirAlpha = SmoothStep01(Frac(Angle / 45.0));

		const double Vw = Lerp(WalkSpeeds[Blend.Dir0], WalkSpeeds[Blend.Dir1], Blend.DirAlpha);
		const double Vj = Lerp(JogSpeeds[Blend.Dir0], JogSpeeds[Blend.Dir1], Blend.DirAlpha);
		Blend.JogWeight = SmoothStep01((Speed - Vw) / std::max(Vj - Vw, 35.0));
		// Crouched: only the walk set ("the same walk, only lower"), cadence matched exactly.
		const double Crouch = std::clamp(CrouchWeight, 0.0, 1.0);
		Blend.JogWeight *= 1.0 - Crouch;
		Blend.ClipSpeed = Lerp(Vw, Vj, Blend.JogWeight);

		const double Lw = Lerp(WalkLengths[Blend.Dir0], WalkLengths[Blend.Dir1], Blend.DirAlpha);
		const double Lj = Lerp(JogLengths[Blend.Dir0], JogLengths[Blend.Dir1], Blend.DirAlpha);
		Blend.ClipLength = std::max(Lerp(Lw, Lj, Blend.JogWeight), 0.05);

		const double RawRate = Speed / std::max(Blend.ClipSpeed, 1.0);
		double Rate = RawRate <= 1.0 ? std::max(RawRate, 0.0) : std::min(1.0 + (RawRate - 1.0) * Params.GainAboveClip, Params.MaxRate);
		if (Crouch > 0.01)
		{
			Rate = Lerp(Rate, std::clamp(RawRate, 0.0, Params.CrouchMaxRate), Crouch);
		}
		Blend.PlayRate = Rate;
		return Blend;
	}

	double AdvancePhase(double Phase, double Dt, double PlayRate, double ClipLength)
	{
		return AnimDetail::Frac(Phase + Dt * PlayRate / std::max(ClipLength, 0.05));
	}

	double MeasureClipSpeed(const std::vector<FFootSample>& Samples, double ClipLength)
	{
		const size_t Count = Samples.size();
		if (Count < 2 || ClipLength <= 0.0)
		{
			return 40.0;
		}
		const double Step = ClipLength / static_cast<double>(Count);
		std::vector<double> Speeds;
		Speeds.reserve(Count);
		for (size_t Index = 0; Index < Count; ++Index)
		{
			const FFootSample& A = Samples[Index];
			const FFootSample& B = Samples[(Index + 1) % Count];
			const bool bLeftPlanted = A.LZ < A.RZ; // the lower foot is on the ground
			const double Dx = bLeftPlanted ? B.LX - A.LX : B.RX - A.RX;
			const double Dy = bLeftPlanted ? B.LY - A.LY : B.RY - A.RY;
			Speeds.push_back(std::sqrt(Dx * Dx + Dy * Dy) / Step);
		}
		std::sort(Speeds.begin(), Speeds.end());
		return std::clamp(Speeds[Count / 2], 40.0, 900.0);
	}

	void StepCrouchSpring(double Target, double Dt, double& Weight, double& Velocity)
	{
		const double Goal = std::clamp(Target, 0.0, 1.0);
		const double SubDt = std::clamp(Dt, 0.0, 0.1) * 0.5;
		for (int Sub = 0; Sub < 2; ++Sub)
		{
			Velocity += ((Goal - Weight) * 150.0 - Velocity * 19.0) * SubDt;
			Weight += Velocity * SubDt;
		}
		Weight = std::clamp(Weight, 0.0, 1.08);
	}

	double FTurnInPlace::Update(double YawDeg, double Dt, bool bStanding)
	{
		using namespace AnimDetail;
		const double Yaw = NormalizeAxis(YawDeg);
		if (!bInitialised)
		{
			LastYaw = Yaw;
			bInitialised = true;
		}
		const double Delta = NormalizeAxis(Yaw - LastYaw);
		LastYaw = Yaw;
		const double Rate = Dt > 1e-4 ? Delta / Dt : 0.0;
		const bool bTurning = bStanding && (std::fabs(Delta) > 0.04 || std::fabs(Rate) > 8.0);
		if (bTurning)
		{
			Direction = InterpTo(Direction, Delta >= 0.0 ? 1.0 : -1.0, Dt, 16.0);
			const double Advance = std::max(std::fabs(Delta) / DegreesPerCycle, Dt * 0.85);
			Phase = Frac(Phase + Advance);
			Weight = InterpTo(Weight, 1.0, Dt, 16.0);
		}
		else
		{
			// Finish the half-step to the next touchdown so the lifted foot plants cleanly.
			if (Weight > 0.01 && bStanding)
			{
				const double Touchdown = Phase < 0.5 ? 0.5 : 1.0;
				const double Remaining = Touchdown - Phase;
				if (Remaining > 0.01 && Remaining < 0.48)
				{
					Phase = std::min(Touchdown, Phase + Dt * 2.2);
					if (Phase >= 1.0)
					{
						Phase = 0.0;
					}
				}
			}
			Weight = InterpTo(Weight, 0.0, Dt, bStanding ? 9.0 : 20.0);
		}
		return Delta;
	}

	void FFootPlanter::Update(const double HomeX[2], const double HomeY[2], const double HomeYaw[2], double Dt, FTarget OutTargets[2])
	{
		using namespace AnimDetail;
		if (!bInitialised)
		{
			for (int Foot = 0; Foot < 2; ++Foot)
			{
				Feet[Foot] = FFoot{HomeX[Foot], HomeY[Foot], HomeYaw[Foot], HomeX[Foot], HomeY[Foot], HomeYaw[Foot], -1.0};
			}
			bInitialised = true;
			SettleTime = 0.0;
		}
		else
		{
			const auto Score = [this, HomeX, HomeY, HomeYaw](int Foot)
			{
				const double Dx = Feet[Foot].X - HomeX[Foot];
				const double Dy = Feet[Foot].Y - HomeY[Foot];
				const double Distance = std::sqrt(Dx * Dx + Dy * Dy);
				const double Angle = std::fabs(NormalizeAxis(Feet[Foot].Yaw - HomeYaw[Foot]));
				return std::max(Distance / Params.StepDistance, Angle / Params.StepAngle);
			};
			SettleTime += Dt;
			if (Feet[0].StepT < 0.0 && Feet[1].StepT < 0.0)
			{
				// One foot at a time: the one furthest from where the pose wants it.
				const double Score0 = Score(0);
				const double Score1 = Score(1);
				const double Need = SettleTime > Params.SettleAfter ? Params.SettleScore : 1.0;
				const int Foot = Score0 >= Score1 ? 0 : 1;
				if (std::max(Score0, Score1) > Need)
				{
					Feet[Foot].FromX = Feet[Foot].X;
					Feet[Foot].FromY = Feet[Foot].Y;
					Feet[Foot].FromYaw = Feet[Foot].Yaw;
					Feet[Foot].StepT = 0.0;
				}
			}
			for (int Foot = 0; Foot < 2; ++Foot)
			{
				if (Feet[Foot].StepT < 0.0)
				{
					continue;
				}
				Feet[Foot].StepT = std::min(Feet[Foot].StepT + Dt / Params.StepSeconds, 1.0);
				if (Feet[Foot].StepT >= 1.0)
				{
					Feet[Foot].X = HomeX[Foot];
					Feet[Foot].Y = HomeY[Foot];
					Feet[Foot].Yaw = HomeYaw[Foot];
					Feet[Foot].StepT = -1.0;
					SettleTime = 0.0;
				}
			}
		}
		for (int Foot = 0; Foot < 2; ++Foot)
		{
			const FFoot& State = Feet[Foot];
			FTarget& Target = OutTargets[Foot];
			Target.bStepping = State.StepT >= 0.0;
			if (!Target.bStepping)
			{
				Target = FTarget{State.X, State.Y, State.Yaw, 0.0, false};
				continue;
			}
			const double K = SmoothStep01(State.StepT);
			const double Dx = HomeX[Foot] - State.FromX;
			const double Dy = HomeY[Foot] - State.FromY;
			const double Span = std::sqrt(Dx * Dx + Dy * Dy);
			Target.X = State.FromX + Dx * K;
			Target.Y = State.FromY + Dy * K;
			Target.Yaw = State.FromYaw + NormalizeAxis(HomeYaw[Foot] - State.FromYaw) * K;
			Target.Lift = std::clamp(Span / 10.0, 0.4, 1.0) * Params.StepLift * std::sin(State.StepT * Pi);
		}
	}

	FGroundIKTarget ComputeGroundIK(bool bHitL, double HitL, bool bHitR, double HitR, const FGroundIKParams& Params)
	{
		FGroundIKTarget Target;
		const double Difference = std::fabs(HitL - HitR);
		if (!bHitL || !bHitR || Difference <= Params.MinDifference || Difference >= Params.MaxDifference)
		{
			return Target; // flat ground (or not a step): zero offsets, weight 0
		}
		// Relative to the higher foot, which supports the capsule: no knee bends on flat ground.
		const double High = std::max(HitL, HitR);
		const double RawL = std::clamp(HitL - High, -Params.MaxDrop, 0.0);
		const double RawR = std::clamp(HitR - High, -Params.MaxDrop, 0.0);
		Target.Pelvis = std::min(RawL, RawR) * Params.PelvisShare;
		Target.OffsetL = RawL - Target.Pelvis;
		Target.OffsetR = RawR - Target.Pelvis;
		Target.Weight = 1.0;
		return Target;
	}

	double IKReachAlpha(double TargetDistance, double ChainLength, double FadeDistance, double Slack)
	{
		const double Reach = ChainLength * Slack;
		if (TargetDistance <= Reach)
		{
			return 1.0;
		}
		if (FadeDistance <= 0.0)
		{
			return 0.0;
		}
		return std::clamp(1.0 - (TargetDistance - Reach) / FadeDistance, 0.0, 1.0);
	}

	FLookAt ComputeLookAt(double TargetYawDeg, double TargetPitchDeg, const FLookAtLimits& Limits)
	{
		FLookAt Out;
		const double Yaw = AnimDetail::NormalizeAxis(TargetYawDeg);
		const double AbsYaw = std::fabs(Yaw);
		Out.Weight = 1.0 - AnimDetail::SmoothStep01((AbsYaw - Limits.FadeStartYaw) / std::max(Limits.FadeEndYaw - Limits.FadeStartYaw, 1.0));
		Out.Yaw = std::clamp(Yaw, -Limits.MaxYaw, Limits.MaxYaw);
		Out.Pitch = std::clamp(TargetPitchDeg, -Limits.MaxPitchDown, Limits.MaxPitchUp);
		return Out;
	}

	FCameraRig ComputeCameraRig(const FCameraProfile& Profile, EStance Stance, bool bInCover, bool bSprinting, double ShoulderSide)
	{
		FCameraRig Rig;
		const double Side = std::clamp(ShoulderSide, -1.0, 1.0);
		Rig.SideOffset = (bInCover ? Profile.CoverShoulderOffset : Profile.ShoulderOffset) * Side;
		switch (Stance)
		{
		case EStance::Stand: Rig.PivotHeight = Profile.PivotHeightStand; break;
		case EStance::Crouch: Rig.PivotHeight = Profile.PivotHeightCrouch; break;
		case EStance::Prone: Rig.PivotHeight = Profile.PivotHeightProne; break;
		}
		const bool bSprint = bSprinting && Stance == EStance::Stand && !bInCover;
		if (Stance == EStance::Prone)
		{
			Rig.ArmLength = Profile.ProneArmLength;
		}
		else if (bInCover)
		{
			Rig.ArmLength = Profile.CoverArmLength;
		}
		else
		{
			Rig.ArmLength = bSprint ? Profile.SprintArmLength : Profile.ArmLength;
		}
		Rig.Fov = bSprint ? Profile.SprintFov : Profile.Fov;
		return Rig;
	}

	double InterpTo(double Current, double Target, double Dt, double Speed)
	{
		if (Speed <= 0.0)
		{
			return Target;
		}
		const double Distance = Target - Current;
		if (Distance * Distance < 1e-8)
		{
			return Target;
		}
		return Current + Distance * std::clamp(Dt * Speed, 0.0, 1.0);
	}
} // namespace GoN::Character
