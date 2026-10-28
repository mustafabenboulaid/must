#include "Logic/GoNMovementLogic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace GoN::Character
{
	namespace MovementDetail
	{
		std::string Cm(double Value)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.0f cm", Value);
			return Buffer;
		}

		double SmoothStep01(double X)
		{
			const double T = std::clamp(X, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		FTraverseDecision Refuse(double Height, const std::string& Reason)
		{
			FTraverseDecision Decision;
			Decision.Action = ETraverseAction::Refused;
			Decision.Height = Height;
			Decision.Reason = Reason;
			return Decision;
		}
	} // namespace MovementDetail

	const char* StanceName(EStance Stance)
	{
		switch (Stance)
		{
		case EStance::Stand: return "stand";
		case EStance::Crouch: return "crouch";
		case EStance::Prone: return "prone";
		}
		return "?";
	}

	const char* GaitName(EGait Gait)
	{
		switch (Gait)
		{
		case EGait::Walk: return "walk";
		case EGait::Jog: return "jog";
		case EGait::Sprint: return "sprint";
		}
		return "?";
	}

	const char* TraverseActionName(ETraverseAction Action)
	{
		switch (Action)
		{
		case ETraverseAction::None: return "none";
		case ETraverseAction::Vault: return "vault";
		case ETraverseAction::Mantle: return "climb";
		case ETraverseAction::Refused: return "refused";
		}
		return "?";
	}

	const char* VaultKindName(EVaultKind Kind)
	{
		switch (Kind)
		{
		case EVaultKind::Low: return "low";
		case EVaultKind::Mid: return "mid";
		case EVaultKind::High: return "high";
		}
		return "?";
	}

	EGait ResolveGait(EGait Requested, EStance Stance, bool bCarrying)
	{
		if (Stance != EStance::Stand)
		{
			return EGait::Walk; // crouch and prone have one speed
		}
		if (bCarrying && Requested == EGait::Sprint)
		{
			return EGait::Jog;
		}
		return Requested;
	}

	double MaxSpeed(const FMovementMetrics& Metrics, EStance Stance, EGait Gait, bool bCarrying)
	{
		switch (Stance)
		{
		case EStance::Prone:
			return Metrics.ProneSpeed;
		case EStance::Crouch:
			return bCarrying ? std::min(Metrics.CrouchSpeed, Metrics.CarryWalkSpeed) : Metrics.CrouchSpeed;
		case EStance::Stand:
			break;
		}
		switch (ResolveGait(Gait, Stance, bCarrying))
		{
		case EGait::Walk: return bCarrying ? Metrics.CarryWalkSpeed : Metrics.WalkSpeed;
		case EGait::Jog: return bCarrying ? Metrics.CarryJogSpeed : Metrics.JogSpeed;
		case EGait::Sprint: return Metrics.SprintSpeed;
		}
		return Metrics.WalkSpeed;
	}

	double HalfHeightFor(const FMovementMetrics& Metrics, EStance Stance)
	{
		switch (Stance)
		{
		case EStance::Stand: return Metrics.StandHalfHeight;
		case EStance::Crouch: return std::max(Metrics.CrouchHalfHeight, Metrics.CapsuleRadius);
		case EStance::Prone: return std::max(Metrics.ProneHalfHeight, Metrics.CapsuleRadius);
		}
		return Metrics.StandHalfHeight;
	}

	std::vector<std::string> ValidateMetrics(const FMovementMetrics& Metrics)
	{
		using MovementDetail::Cm;
		std::vector<std::string> Issues;
		if (!(Metrics.WalkSpeed < Metrics.JogSpeed && Metrics.JogSpeed < Metrics.SprintSpeed))
		{
			Issues.push_back("speeds must rise walk < jog < sprint");
		}
		if (Metrics.CarryJogSpeed > Metrics.JogSpeed || Metrics.CarryWalkSpeed > Metrics.WalkSpeed)
		{
			Issues.push_back("carrying must be slower than walking/jogging free");
		}
		if (Metrics.ProneSpeed > Metrics.CrouchSpeed)
		{
			Issues.push_back("prone must not be faster than crouch");
		}
		if (!(Metrics.ProneHalfHeight <= Metrics.CrouchHalfHeight && Metrics.CrouchHalfHeight < Metrics.StandHalfHeight))
		{
			Issues.push_back("capsule half heights must shrink stand > crouch >= prone");
		}
		if (Metrics.ProneHalfHeight < Metrics.CapsuleRadius)
		{
			Issues.push_back("prone half height " + Cm(Metrics.ProneHalfHeight) + " is below the radius; UE clamps it to " + Cm(Metrics.CapsuleRadius));
		}
		if (!(Metrics.MaxStepHeight < Metrics.MaxVaultHeight && Metrics.MaxVaultHeight <= Metrics.MaxClimbHeight))
		{
			Issues.push_back("heights must rise step < vault <= climb");
		}
		if (!(Metrics.LowVaultMax < Metrics.MidVaultMax && Metrics.MidVaultMax <= Metrics.MaxVaultHeight))
		{
			Issues.push_back("vault kinds must rise low < mid <= max vault height");
		}
		if (Metrics.MinMantleDepth > Metrics.CapsuleRadius * 2.0)
		{
			Issues.push_back("min mantle depth is wider than the capsule; ledges the capsule fits on are refused");
		}
		return Issues;
	}

	std::string FTraverseDecision::ToString() const
	{
		std::string Out = TraverseActionName(Action);
		if (Action == ETraverseAction::Vault)
		{
			Out += std::string(" (") + VaultKindName(VaultKind) + ")";
		}
		if (Action != ETraverseAction::None || Height > 0.0)
		{
			Out += " " + MovementDetail::Cm(Height);
		}
		if (Action == ETraverseAction::Mantle && EndStance == EStance::Crouch)
		{
			Out += " (ends crouched)";
		}
		if (!Reason.empty())
		{
			Out += ": " + Reason;
		}
		return Out;
	}

	FTraverseDecision DecideTraverse(const FTraverseProbe& Probe, const FMovementMetrics& Metrics)
	{
		using MovementDetail::Cm;
		using MovementDetail::Refuse;
		FTraverseDecision Decision;
		if (!Probe.bObstacle || !Probe.bTopFound)
		{
			Decision.Reason = Probe.bObstacle ? "obstacle top not found (taller than the trace)" : "no obstacle ahead";
			return Decision;
		}
		const double Height = Probe.TopHeight;
		Decision.Height = Height;
		if (Height <= Metrics.MaxStepHeight)
		{
			Decision.Reason = "step: walked by the movement component";
			return Decision;
		}
		if (Probe.bFalling)
		{
			return Refuse(Height, "in the air");
		}
		if (Probe.Stance == EStance::Prone)
		{
			return Refuse(Height, "prone");
		}
		if (Probe.bCarrying)
		{
			return Refuse(Height, "carrying a wounded comrade");
		}
		if (Height > Metrics.MaxClimbHeight)
		{
			return Refuse(Height, "too high (max climb " + Cm(Metrics.MaxClimbHeight) + ")");
		}
		if (!Probe.bHeadroomClear)
		{
			return Refuse(Height, "no headroom above the obstacle");
		}

		const bool bThin = Probe.Depth <= Metrics.MaxVaultDepth;
		if (bThin && Height <= Metrics.MaxVaultHeight && Probe.bLandingFound && Probe.bLandingClear)
		{
			Decision.Action = ETraverseAction::Vault;
			Decision.VaultKind = Height <= Metrics.LowVaultMax ? EVaultKind::Low : (Height <= Metrics.MidVaultMax ? EVaultKind::Mid : EVaultKind::High);
			Decision.Duration = Metrics.VaultBaseSeconds + Metrics.VaultSecondsPerMetre * Height / 100.0;
			return Decision;
		}
		if (Probe.Depth < Metrics.MinMantleDepth)
		{
			return Refuse(Height, "top too narrow to stand on and nowhere to land");
		}
		if (!Probe.bRoomCrouchedOnTop)
		{
			return Refuse(Height, "no room on top");
		}
		Decision.Action = ETraverseAction::Mantle;
		Decision.EndStance = Probe.bRoomStandingOnTop ? EStance::Stand : EStance::Crouch;
		Decision.Duration = Metrics.ClimbBaseSeconds + Metrics.ClimbSecondsPerMetre * Height / 100.0;
		return Decision;
	}

	std::vector<FPathKey> BuildTraversePath(const FTraverseDecision& Decision, const FTraverseProbe& Probe, const FMovementMetrics& Metrics)
	{
		std::vector<FPathKey> Path;
		const double HalfHeight = HalfHeightFor(Metrics, Probe.Stance);
		const double Face = Probe.ObstacleDistance + Metrics.CapsuleRadius; // centre -> obstacle face
		if (Decision.Action == ETraverseAction::Vault)
		{
			// Up and over the middle of the obstacle (capsule bottom VaultClearance above the top),
			// then down to the landing. Up is relative to the start capsule centre, same stance.
			const double Land = Face + Probe.Depth + Metrics.CapsuleRadius + 10.0;
			Path.push_back(FPathKey{0.0, 0.0, 0.0});
			Path.push_back(FPathKey{0.40, Face + Probe.Depth * 0.5, Decision.Height + Metrics.VaultClearance});
			Path.push_back(FPathKey{1.0, Land, Probe.LandingHeight});
			return Path;
		}
		if (Decision.Action == ETraverseAction::Mantle)
		{
			// Rise against the face until the capsule clears the top, then step forward onto it.
			const double EndHalfHeight = HalfHeightFor(Metrics, Decision.EndStance);
			const double ClearUp = Decision.Height + 2.0;
			const double OnTop = Face + Metrics.CapsuleRadius + 5.0;
			Path.push_back(FPathKey{0.0, 0.0, 0.0});
			Path.push_back(FPathKey{0.65, std::max(0.0, Face - Metrics.CapsuleRadius), ClearUp});
			Path.push_back(FPathKey{1.0, OnTop, Decision.Height + EndHalfHeight - HalfHeight});
			return Path;
		}
		Path.push_back(FPathKey{0.0, 0.0, 0.0});
		return Path;
	}

	FPathKey EvaluateTraversePath(const std::vector<FPathKey>& Path, double Alpha)
	{
		if (Path.empty())
		{
			return FPathKey{};
		}
		const double T = std::clamp(Alpha, 0.0, 1.0);
		if (T <= Path.front().Time || Path.size() == 1)
		{
			return FPathKey{T, Path.front().Forward, Path.front().Up};
		}
		for (size_t Index = 1; Index < Path.size(); ++Index)
		{
			const FPathKey& A = Path[Index - 1];
			const FPathKey& B = Path[Index];
			if (T <= B.Time)
			{
				const double Span = std::max(B.Time - A.Time, 1e-6);
				const double K = MovementDetail::SmoothStep01((T - A.Time) / Span);
				return FPathKey{T, A.Forward + (B.Forward - A.Forward) * K, A.Up + (B.Up - A.Up) * K};
			}
		}
		return FPathKey{T, Path.back().Forward, Path.back().Up};
	}
} // namespace GoN::Character
