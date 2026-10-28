// Ghosts of November — compares the character's movement metrics with the gym's reference
// metrics (pure C++). Backs `gon.char.metrics` and the test "Blockout.MetricsCheck.*".
// Lives in GoNBlockout because GoNBlockout depends on GoNCharacter, not the other way round.
#pragma once

#include "Logic/GoNGymLayout.h"
#include "Logic/GoNMovementLogic.h"

#include <string>
#include <vector>

namespace GoN::Blockout
{
	enum class EMetricResult
	{
		Ok,
		Note, // works, but worth knowing while tuning (e.g. crouched capsule taller than low cover)
		Fail
	};

	struct FMetricCheck
	{
		EMetricResult Result = EMetricResult::Ok;
		std::string Text;
	};

	// The probe a gym obstacle produces when walked into head-on (used to predict the decision).
	GoN::Character::FTraverseProbe GymObstacleProbe(double Height, double Depth, bool bLandingBehind);

	std::vector<FMetricCheck> CompareMovementWithGym(const GoN::Character::FMovementMetrics& Movement, const FGymMetrics& Gym);

	int CountResults(const std::vector<FMetricCheck>& Checks, EMetricResult Result);
} // namespace GoN::Blockout
