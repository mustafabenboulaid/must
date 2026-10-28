// Pure-logic tests for the movement-vs-gym comparison behind `gon.char.metrics`.
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNMovementGymCheck.h"

namespace GoNMovementGymCheckTestsDetail
{
	using namespace GoN::Blockout;

	GON_LOGIC_TEST("Blockout.MetricsCheck.DefaultsMatchTheGym", Blockout_MetricsCheck_Defaults)
	{
		const std::vector<FMetricCheck> Checks = CompareMovementWithGym(GoN::Character::FMovementMetrics(), FGymMetrics());
		for (const FMetricCheck& Check : Checks)
		{
			if (Check.Result == EMetricResult::Fail)
			{
				Ctx.Fail(__FILE__, __LINE__, Check.Text);
			}
		}
		GON_EXPECT(Checks.size() > 15);
	}

	GON_LOGIC_TEST("Blockout.MetricsCheck.DetectsMismatches", Blockout_MetricsCheck_Mismatch)
	{
		GoN::Character::FMovementMetrics Movement;
		Movement.MaxVaultHeight = 100.0; // the 120 cm gym vault is no longer vaultable
		Movement.MidVaultMax = 95.0;
		Movement.MaxClimbHeight = 260.0; // the 250 cm ledge becomes climbable
		Movement.CapsuleRadius = 34.0;
		const std::vector<FMetricCheck> Checks = CompareMovementWithGym(Movement, FGymMetrics());
		// Capsule radius and the 120 cm vault fail; with climb 260 the 250 ledge is expected to be
		// climbed, so only the missing "refuse" reference is reported, as a note.
		GON_EXPECT_EQ(CountResults(Checks, EMetricResult::Fail), 2);
		GON_EXPECT(CountResults(Checks, EMetricResult::Note) >= 1);
	}
} // namespace GoNMovementGymCheckTestsDetail

#endif // GON_WITH_LOGIC_TESTS
