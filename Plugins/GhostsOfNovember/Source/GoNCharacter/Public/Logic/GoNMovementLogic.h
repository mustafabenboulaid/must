// Ghosts of November — character movement rules (pure C++, no Unreal headers).
// Speeds per stance/gait, the vault / climb decision from trace results, and the traversal
// path. AGoNCharacter feeds traces in and executes the result; tests cover the rules.
// Units: cm, seconds, degrees. Heights are relative to the character's feet.
#pragma once

#include <string>
#include <vector>

namespace GoN::Character
{
	enum class EStance
	{
		Stand,
		Crouch,
		Prone
	};

	enum class EGait
	{
		Walk,
		Jog,
		Sprint
	};

	const char* StanceName(EStance Stance);
	const char* GaitName(EGait Gait);

	// Defaults match the M0 gym metrics (Logic/GoNGymLayout.h) and UE's character defaults.
	struct FMovementMetrics
	{
		// Capsule. UE clamps a capsule half height to at least its radius, so prone is >= radius.
		double CapsuleRadius = 42.0;
		double StandHalfHeight = 96.0;
		double CrouchHalfHeight = 60.0;
		double ProneHalfHeight = 42.0;

		// Speeds (cm/s).
		double WalkSpeed = 170.0;
		double JogSpeed = 360.0;
		double SprintSpeed = 600.0;
		double CrouchSpeed = 170.0;
		double ProneSpeed = 60.0;
		double CarryWalkSpeed = 120.0; // carrying a wounded comrade: slower, no sprint
		double CarryJogSpeed = 190.0;

		// Ground.
		double MaxStepHeight = 45.0;
		double WalkableSlopeDeg = 44.765;

		// Traversal. Obstacles <= MaxStepHeight are walked by the movement component.
		double ReachDistance = 90.0;    // look for an obstacle this far ahead of the capsule surface
		double MaxVaultHeight = 130.0;  // thin obstacles up to this height are vaulted over
		double MaxVaultDepth = 80.0;    // "thin": thickness along the movement direction
		double MinMantleDepth = 50.0;   // a top shallower than this has nowhere to stand
		double MaxClimbHeight = 220.0;  // higher is refused (gym: climb 150/200, refuse 250)
		double LowVaultMax = 75.0;      // vault kind for the animation: low <= 75 < mid <= 105 < high
		double MidVaultMax = 105.0;
		double VaultClearance = 12.0;   // capsule bottom above the obstacle top at the apex
		double VaultBaseSeconds = 0.35;
		double VaultSecondsPerMetre = 0.30;
		double ClimbBaseSeconds = 0.45;
		double ClimbSecondsPerMetre = 0.45;
	};

	// Movement component speed for a stance/gait. Carrying caps the gait at jog.
	double MaxSpeed(const FMovementMetrics& Metrics, EStance Stance, EGait Gait, bool bCarrying);
	EGait ResolveGait(EGait Requested, EStance Stance, bool bCarrying);
	double HalfHeightFor(const FMovementMetrics& Metrics, EStance Stance);

	// Consistency of the metrics themselves (empty = fine).
	std::vector<std::string> ValidateMetrics(const FMovementMetrics& Metrics);

	// ---- Vault / climb ------------------------------------------------------------------

	// What the forward/down traces found (filled by AGoNCharacter, or by tests).
	struct FTraverseProbe
	{
		bool bObstacle = false;          // forward trace hit something within ReachDistance
		double ObstacleDistance = 0.0;   // from the capsule surface to the obstacle face
		bool bTopFound = false;          // downward trace found the obstacle's top
		double TopHeight = 0.0;          // top surface above the feet
		double Depth = 1.0e6;            // obstacle thickness (large = solid / unknown)
		bool bLandingFound = false;      // ground found behind a thin obstacle
		double LandingHeight = 0.0;      // that ground, relative to the feet (negative = lower)
		bool bLandingClear = false;      // a standing capsule fits at the landing spot
		bool bRoomStandingOnTop = false; // a standing capsule fits on the top
		bool bRoomCrouchedOnTop = false; // a crouched capsule fits on the top
		bool bHeadroomClear = false;     // the path up/over the obstacle is free

		EStance Stance = EStance::Stand;
		bool bCarrying = false;
		bool bFalling = false;
	};

	enum class ETraverseAction
	{
		None,    // nothing to do (no obstacle, or a step the movement component walks)
		Vault,   // over a thin obstacle
		Mantle,  // climb onto the top
		Refused  // an obstacle that cannot be crossed (too high, no room, carrying...)
	};

	enum class EVaultKind
	{
		Low,
		Mid,
		High
	};

	const char* TraverseActionName(ETraverseAction Action);
	const char* VaultKindName(EVaultKind Kind);

	struct FTraverseDecision
	{
		ETraverseAction Action = ETraverseAction::None;
		EVaultKind VaultKind = EVaultKind::Low;
		double Height = 0.0;
		EStance EndStance = EStance::Stand;
		double Duration = 0.0;
		std::string Reason;

		std::string ToString() const;
	};

	FTraverseDecision DecideTraverse(const FTraverseProbe& Probe, const FMovementMetrics& Metrics);

	// Capsule-centre path relative to the start (Forward along the move direction, Up),
	// Time normalised 0..1. Evaluated with EvaluateTraversePath.
	struct FPathKey
	{
		double Time = 0.0;
		double Forward = 0.0;
		double Up = 0.0;
	};

	std::vector<FPathKey> BuildTraversePath(const FTraverseDecision& Decision, const FTraverseProbe& Probe, const FMovementMetrics& Metrics);

	// Smooth (ease in/out per segment) interpolation; Alpha outside 0..1 is clamped.
	FPathKey EvaluateTraversePath(const std::vector<FPathKey>& Path, double Alpha);
} // namespace GoN::Character
