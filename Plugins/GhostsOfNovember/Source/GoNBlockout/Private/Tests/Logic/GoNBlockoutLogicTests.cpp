// Pure-logic tests for the gym layout. Run by Tools/LogicTests and "GoN.Blockout.Logic".
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNGymLayout.h"

#include <algorithm>
#include <cmath>

namespace GoNBlockoutLogicTestsDetail
{
	using namespace GoN::Blockout;

	int CountPieces(const FGymLayout& Layout, EGymPiece Piece)
	{
		return static_cast<int>(std::count_if(Layout.Boxes.begin(), Layout.Boxes.end(), [Piece](const FGymBox& Box) { return Box.Piece == Piece; }));
	}

	GON_LOGIC_TEST("Blockout.Gym.DefaultLayoutIsConsistent", Blockout_Gym_Default)
	{
		const FGymMetrics Metrics;
		const FGymLayout Layout = BuildGymLayout(Metrics);
		for (const std::string& Issue : ValidateGymLayout(Layout, Metrics))
		{
			Ctx.Fail(__FILE__, __LINE__, Issue);
		}
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Floor), 1);
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Wall), 4);
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Vault), static_cast<int>(Metrics.VaultHeights.size()));
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Ledge), static_cast<int>(Metrics.LedgeHeights.size()));
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Ramp), static_cast<int>(Metrics.RampAnglesDeg.size()));
		GON_EXPECT_EQ(CountPieces(Layout, EGymPiece::Stairs), Metrics.StepCount + 1);
		GON_EXPECT_EQ(Layout.Labels.size(), Metrics.DoorWidths.size());
	}

	GON_LOGIC_TEST("Blockout.Gym.HeightsMatchMetrics", Blockout_Gym_Heights)
	{
		const FGymMetrics Metrics;
		const FGymLayout Layout = BuildGymLayout(Metrics);
		std::vector<double> Vaults;
		std::vector<double> Ledges;
		double TallestStep = 0.0;
		for (const FGymBox& Box : Layout.Boxes)
		{
			if (Box.Piece == EGymPiece::Vault) { Vaults.push_back(Box.Top()); }
			if (Box.Piece == EGymPiece::Ledge) { Ledges.push_back(Box.Top()); }
			if (Box.Piece == EGymPiece::LowCover) { GON_EXPECT(std::fabs(Box.Top() - Metrics.LowCoverHeight) < 1e-6); }
			if (Box.Piece == EGymPiece::HighCover) { GON_EXPECT(std::fabs(Box.Top() - Metrics.HighCoverHeight) < 1e-6); }
			if (Box.Piece == EGymPiece::Stairs) { TallestStep = std::max(TallestStep, Box.Top()); }
			if (Box.Piece != EGymPiece::Floor && Box.Piece != EGymPiece::Ramp) { GON_EXPECT(std::fabs(Box.Center.Z - Box.Extent.Z) < 1e-6); } // sits on the floor
		}
		GON_EXPECT(Vaults == Metrics.VaultHeights);
		GON_EXPECT(Ledges == Metrics.LedgeHeights);
		GON_EXPECT(std::fabs(TallestStep - Metrics.StepRise * Metrics.StepCount) < 1e-6);
	}

	GON_LOGIC_TEST("Blockout.Gym.RampsStartAtFloor", Blockout_Gym_Ramps)
	{
		const FGymMetrics Metrics;
		const FGymLayout Layout = BuildGymLayout(Metrics);
		for (const FGymBox& Ramp : Layout.Boxes)
		{
			if (Ramp.Piece != EGymPiece::Ramp)
			{
				continue;
			}
			// Bottom of the bounds dips slightly below the floor (thickness), top reaches L*sin(angle).
			const double Radians = Ramp.PitchDeg * 3.14159265358979323846 / 180.0;
			const double ExpectedTop = std::sin(Radians) * Ramp.Extent.X * 2.0;
			const double BoundsTop = Ramp.Center.Z + Ramp.BoundsExtent().Z;
			GON_EXPECT(BoundsTop >= ExpectedTop - 1e-6 && BoundsTop <= ExpectedTop + Ramp.Extent.Z * 2.0 + 1e-6);
			GON_EXPECT(Ramp.Center.Z - Ramp.BoundsExtent().Z <= 0.0);
		}
	}

	GON_LOGIC_TEST("Blockout.Gym.ValidatorCatchesBadMetrics", Blockout_Gym_BadMetrics)
	{
		FGymMetrics Metrics;
		Metrics.StepRise = 60.0;          // above MaxStepHeight
		Metrics.DoorWidths = {60.0};      // narrower than the capsule
		Metrics.RampAnglesDeg = {20.0};   // no too-steep reference
		const std::vector<std::string> Issues = ValidateGymLayout(BuildGymLayout(Metrics), Metrics);
		GON_EXPECT_EQ(Issues.size(), size_t(3));
	}

	GON_LOGIC_TEST("Blockout.Gym.CapsuleOverlap", Blockout_Gym_Capsule)
	{
		FGymBox Box;
		Box.Center = FVec3{0.0, 0.0, 50.0};
		Box.Extent = FVec3{50.0, 50.0, 50.0};
		GON_EXPECT(CapsuleOverlapsBox(FVec3{80.0, 0.0, 96.0}, 42.0, 96.0, Box));
		GON_EXPECT(!CapsuleOverlapsBox(FVec3{93.0, 0.0, 96.0}, 42.0, 96.0, Box));
		GON_EXPECT(!CapsuleOverlapsBox(FVec3{0.0, 0.0, 200.0}, 42.0, 96.0, Box));
	}
} // namespace GoNBlockoutLogicTestsDetail

#endif // GON_WITH_LOGIC_TESTS
