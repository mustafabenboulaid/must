// Pure-logic tests for the blockout kit. Run by Tools/LogicTests and "GoN.Blockout.Logic".
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNBlockoutKit.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace GoNBlockoutKitTestsDetail
{
	using namespace GoN::Blockout;

	int CountRole(const FKitShape& Shape, EKitRole Role)
	{
		return static_cast<int>(std::count_if(Shape.Boxes.begin(), Shape.Boxes.end(), [Role](const FKitBox& Box) { return Box.Role == Role; }));
	}

	double MaxTop(const FKitShape& Shape, EKitRole Role)
	{
		double Result = -std::numeric_limits<double>::infinity();
		for (const FKitBox& Box : Shape.Boxes)
		{
			if (Box.Role == Role)
			{
				Result = std::max(Result, Box.Top());
			}
		}
		return Result;
	}

	bool Near(double A, double B, double Tolerance = 1e-6)
	{
		return std::fabs(A - B) <= Tolerance;
	}

	// Footprint test for unrotated boxes (walls and doorways of the house/alley).
	bool InsideFootprint(const FKitBox& Box, const FVec3& Point)
	{
		return std::fabs(Point.X - Box.Center.X) < Box.Extent.X && std::fabs(Point.Y - Box.Center.Y) < Box.Extent.Y;
	}

	bool HasIssueContaining(const FKitShape& Shape, const std::string& Text)
	{
		return std::any_of(Shape.Issues.begin(), Shape.Issues.end(), [&Text](const std::string& Issue) { return Issue.find(Text) != std::string::npos; });
	}

	struct FNamedShape
	{
		const char* Name;
		FKitShape Shape;
	};

	std::vector<FNamedShape> AllDefaultShapes()
	{
		FDouarHouseParams TwoRooms;
		TwoRooms.Rooms = 2;
		FCoverParams High;
		High.Height = ECoverHeight::High;
		return {
			{"house1", BuildDouarHouse(FDouarHouseParams())},
			{"house2", BuildDouarHouse(TwoRooms)},
			{"alley", BuildAlley(FAlleyParams())},
			{"bridge", BuildRoofBridge(FRoofBridgeParams())},
			{"wall", BuildWall(FWallParams())},
			{"stairs", BuildStairs(FStairsParams())},
			{"rock", BuildRock(FRockParams())},
			{"slope", BuildSlope(FSlopeParams())},
			{"coverLow", BuildCover(FCoverParams())},
			{"coverHigh", BuildCover(High)},
		};
	}

	// Every point: unit horizontal normal and a standing capsule that touches no box.
	void ExpectPointsFree(GoN::LogicTest::FContext& Ctx, const char* Name, const FKitShape& Shape)
	{
		const FCoverRules Rules;
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			++Ctx.Checks;
			const double NormalLength = std::sqrt(Point.Normal.X * Point.Normal.X + Point.Normal.Y * Point.Normal.Y);
			if (!Near(NormalLength, 1.0, 1e-9) || Point.Normal.Z != 0.0 || !std::isfinite(Point.Position.Z))
			{
				Ctx.Fail(__FILE__, __LINE__, std::string(Name) + ": bad cover normal or position");
			}
			const FVec3 Centre{Point.Position.X, Point.Position.Y, Point.Position.Z + Rules.CapsuleHalfHeight};
			for (const FKitBox& Box : Shape.Boxes)
			{
				if (Box.Top() > Point.Position.Z + 2.0 && CapsuleOverlapsKitBox(Centre, Rules.CapsuleRadius, Rules.CapsuleHalfHeight, Box))
				{
					Ctx.Fail(__FILE__, __LINE__, std::string(Name) + ": cover point capsule overlaps a " + KitRoleName(Box.Role) + " box");
				}
			}
		}
	}

	GON_LOGIC_TEST("Blockout.Kit.WallHighCoverPoints", Blockout_Kit_WallHigh)
	{
		const FKitShape Shape = BuildWall(FWallParams()); // 400 x 250 x 30
		GON_EXPECT(Shape.Issues.empty());
		GON_EXPECT_EQ(Shape.CoverPoints.size(), size_t(8));
		int PlusY = 0;
		int MinusY = 0;
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			GON_EXPECT(Point.Height == ECoverHeight::High);
			GON_EXPECT(Near(Point.Normal.X, 0.0)); // nothing on the 30 cm ends
			GON_EXPECT(Near(Point.Position.Z, 0.0));
			GON_EXPECT(Near(Point.CoverTop, 250.0));
			GON_EXPECT(std::fabs(Point.Position.X) <= 200.0 - 30.0 + 1e-6);
			GON_EXPECT(Near(std::fabs(Point.Position.Y), 15.0 + 42.0 + 5.0));
			PlusY += Point.Normal.Y > 0.5 ? 1 : 0;
			MinusY += Point.Normal.Y < -0.5 ? 1 : 0;
		}
		GON_EXPECT_EQ(PlusY, 4);
		GON_EXPECT_EQ(MinusY, 4);
		GON_EXPECT(Shape.Label.find("high cover") != std::string::npos);
	}

	GON_LOGIC_TEST("Blockout.Kit.LowCoverIsLow", Blockout_Kit_LowCover)
	{
		const FKitShape Shape = BuildCover(FCoverParams()); // low, 250 x 50 x 100
		GON_EXPECT(Shape.Issues.empty());
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::LowCover), 1);
		GON_EXPECT_EQ(Shape.CoverPoints.size(), size_t(4)); // 2 per long side, none on the 50 cm ends
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			GON_EXPECT(Point.Height == ECoverHeight::Low);
		}

		FCoverParams High;
		High.Height = ECoverHeight::High;
		const FKitShape HighShape = BuildCover(High);
		GON_EXPECT_EQ(CountRole(HighShape, EKitRole::HighCover), 1);
		GON_REQUIRE(!HighShape.CoverPoints.empty());
		for (const FCoverPoint& Point : HighShape.CoverPoints)
		{
			GON_EXPECT(Point.Height == ECoverHeight::High);
		}

		FCoverParams Mismatch;
		Mismatch.HeightLow = 150.0; // between low and high
		const FKitShape MismatchShape = BuildCover(Mismatch);
		GON_EXPECT(MismatchShape.CoverPoints.empty());
		GON_EXPECT_EQ(MismatchShape.Issues.size(), size_t(1));
	}

	GON_LOGIC_TEST("Blockout.Kit.ShortWallHasNoCover", Blockout_Kit_ShortWall)
	{
		FWallParams Params;
		Params.Height = 50.0;
		const FKitShape Shape = BuildWall(Params);
		GON_EXPECT(Shape.CoverPoints.empty());
		GON_EXPECT(Shape.Label.find("too low") != std::string::npos);
	}

	GON_LOGIC_TEST("Blockout.Kit.SlopesHaveNoCover", Blockout_Kit_SlopeNoCover)
	{
		for (const double Angle : {20.0, 35.0, 50.0})
		{
			FSlopeParams Params;
			Params.AngleDeg = Angle;
			GON_EXPECT(BuildSlope(Params).CoverPoints.empty());
		}
		// Even a pitched box flagged as a cover source produces nothing.
		FKitBox Pitched;
		Pitched.Role = EKitRole::Wall;
		Pitched.Center = FVec3{0.0, 0.0, 100.0};
		Pitched.Extent = FVec3{200.0, 15.0, 100.0};
		Pitched.PitchDeg = 10.0;
		Pitched.bCoverSource = true;
		GON_EXPECT(GenerateCoverPoints({Pitched}, FCoverRules()).empty());
	}

	GON_LOGIC_TEST("Blockout.Kit.CoverPointsNeverOverlapBoxes", Blockout_Kit_PointsFree)
	{
		for (const FNamedShape& Named : AllDefaultShapes())
		{
			ExpectPointsFree(Ctx, Named.Name, Named.Shape);
		}
		for (uint32_t Seed = 0; Seed < 40; ++Seed)
		{
			FRockParams Params;
			Params.Seed = Seed;
			Params.Chunks = 1 + static_cast<int>(Seed % 8);
			ExpectPointsFree(Ctx, "rock", BuildRock(Params));
		}
		FAlleyParams Flat;
		Flat.StepRise = 0.0;
		ExpectPointsFree(Ctx, "flat alley", BuildAlley(Flat));
	}

	GON_LOGIC_TEST("Blockout.Kit.HouseOneRoom", Blockout_Kit_HouseOne)
	{
		const FDouarHouseParams Params;
		const FKitShape Shape = BuildDouarHouse(Params);
		GON_EXPECT(Shape.Issues.empty());
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Roof), 1);
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Floor), 1);
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Wall), 6); // front x2 + lintel, back, 2 sides
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Parapet), 4);
		GON_EXPECT(CountRole(Shape, EKitRole::Stairs) > 0);
		GON_EXPECT_EQ(Shape.Doorways.size(), size_t(1));
		const double RoofTop = Params.WallHeight + Params.RoofThickness;
		GON_EXPECT(Near(MaxTop(Shape, EKitRole::Roof), RoofTop));

		const double StairTop = MaxTop(Shape, EKitRole::Stairs);
		GON_EXPECT(StairTop <= RoofTop + 1e-6 && StairTop >= RoofTop - Params.StairRise);
		for (const FKitBox& Box : Shape.Boxes)
		{
			if (Box.Role == EKitRole::Stairs)
			{
				GON_EXPECT(Box.Extent.Z * 2.0 > 0.0);
			}
		}
		// Every stair step rises by at most StairRise from the previous one.
		std::vector<double> StepTops;
		for (const FKitBox& Box : Shape.Boxes)
		{
			if (Box.Role == EKitRole::Stairs)
			{
				StepTops.push_back(Box.Top());
			}
		}
		std::sort(StepTops.begin(), StepTops.end());
		double Previous = 0.0;
		for (const double StepTop : StepTops)
		{
			GON_EXPECT(StepTop - Previous <= Params.StairRise + 1e-6);
			Previous = StepTop;
		}

		GON_EXPECT(Shape.CoverPoints.size() >= 8);
		bool bInside = false;
		bool bOutside = false;
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			for (const FKitBox& Doorway : Shape.Doorways)
			{
				GON_EXPECT(!InsideFootprint(Doorway, Point.Position));
			}
			for (const FKitBox& Box : Shape.Boxes)
			{
				if (Box.Role == EKitRole::Wall)
				{
					GON_EXPECT(!InsideFootprint(Box, Point.Position));
				}
			}
			const bool bInRoom = std::fabs(Point.Position.X) < Params.RoomSizeX * 0.5 && std::fabs(Point.Position.Y) < Params.RoomSizeY * 0.5;
			bInside = bInside || bInRoom;
			bOutside = bOutside || !bInRoom;
			// Inside points stand on the floor slab, outside points on the ground.
			GON_EXPECT(Near(Point.Position.Z, bInRoom ? Params.FloorThickness : 0.0));
		}
		GON_EXPECT(bInside);
		GON_EXPECT(bOutside);
	}

	GON_LOGIC_TEST("Blockout.Kit.HouseTwoRooms", Blockout_Kit_HouseTwo)
	{
		FDouarHouseParams Params;
		Params.Rooms = 2;
		const FKitShape Shape = BuildDouarHouse(Params);
		GON_EXPECT(Shape.Issues.empty());
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Floor), 2);
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Roof), 1);
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Wall), 9); // + divider x2 + lintel
		GON_REQUIRE(Shape.Doorways.size() == 2);
		// Interior door in the dividing wall, between the two rooms.
		const double OuterX = 2.0 * Params.RoomSizeX + 3.0 * Params.WallThickness;
		const double DividerX = -OuterX * 0.5 + Params.WallThickness + Params.RoomSizeX + Params.WallThickness * 0.5;
		GON_EXPECT(Near(Shape.Doorways[1].Center.X, DividerX));
		GON_EXPECT(Near(Shape.Doorways[0].Center.X, -OuterX * 0.5 + Params.WallThickness * 0.5));
		GON_EXPECT(Near(MaxTop(Shape, EKitRole::Roof), Params.WallHeight + Params.RoofThickness));
		const double StairTop = MaxTop(Shape, EKitRole::Stairs);
		GON_EXPECT(StairTop <= Params.WallHeight + Params.RoofThickness + 1e-6);
		GON_EXPECT(StairTop >= Params.WallHeight + Params.RoofThickness - Params.StairRise);
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			for (const FKitBox& Doorway : Shape.Doorways)
			{
				GON_EXPECT(!InsideFootprint(Doorway, Point.Position));
			}
			for (const FKitBox& Box : Shape.Boxes)
			{
				if (Box.Role == EKitRole::Wall)
				{
					GON_EXPECT(!InsideFootprint(Box, Point.Position));
				}
			}
		}
		// Both rooms get interior cover along the divider.
		const auto CountNear = [&Shape, DividerX](double Side)
		{
			return std::count_if(Shape.CoverPoints.begin(), Shape.CoverPoints.end(), [DividerX, Side](const FCoverPoint& Point)
			{
				return std::fabs(Point.Normal.X - Side) < 1e-6 && std::fabs(Point.Position.X - DividerX) < 100.0;
			});
		};
		GON_EXPECT(CountNear(1.0) > 0);
		GON_EXPECT(CountNear(-1.0) > 0);

		FDouarHouseParams NoStairs;
		NoStairs.bExteriorStairs = false;
		NoStairs.ParapetHeight = 0.0;
		const FKitShape Plain = BuildDouarHouse(NoStairs);
		GON_EXPECT_EQ(CountRole(Plain, EKitRole::Stairs), 0);
		GON_EXPECT_EQ(CountRole(Plain, EKitRole::Parapet), 0);
	}

	GON_LOGIC_TEST("Blockout.Kit.HouseValidator", Blockout_Kit_HouseBad)
	{
		FDouarHouseParams Params;
		Params.DoorWidth = 80.0;    // narrower than 2 * 42 + 10
		Params.DoorHeight = 400.0;  // above the 300 cm wall
		Params.StairRise = 50.0;    // above the 45 cm max step
		const FKitShape Shape = BuildDouarHouse(Params);
		GON_EXPECT_EQ(Shape.Issues.size(), size_t(3));
		GON_EXPECT(HasIssueContaining(Shape, "narrower"));
		GON_EXPECT(HasIssueContaining(Shape, "higher than the wall"));
		GON_EXPECT(HasIssueContaining(Shape, "max step"));

		FDouarHouseParams Rooms;
		Rooms.Rooms = 5;
		const FKitShape Clamped = BuildDouarHouse(Rooms);
		GON_EXPECT(HasIssueContaining(Clamped, "rooms"));
		GON_EXPECT_EQ(CountRole(Clamped, EKitRole::Floor), 2);
	}

	GON_LOGIC_TEST("Blockout.Kit.AlleyTerraces", Blockout_Kit_Alley)
	{
		const FAlleyParams Params;
		const FKitShape Shape = BuildAlley(Params);
		GON_EXPECT(Shape.Issues.empty());
		std::vector<FKitBox> Floors;
		for (const FKitBox& Box : Shape.Boxes)
		{
			if (Box.Role == EKitRole::Floor)
			{
				Floors.push_back(Box);
			}
		}
		std::sort(Floors.begin(), Floors.end(), [](const FKitBox& A, const FKitBox& B) { return A.Center.X < B.Center.X; });
		GON_EXPECT_EQ(Floors.size(), size_t(8)); // 1200 / 150
		for (size_t Index = 0; Index < Floors.size(); ++Index)
		{
			GON_EXPECT(Near(Floors[Index].Bottom(), 0.0));
			GON_EXPECT(Near(Floors[Index].Extent.X * 2.0, Params.StepRun));
			if (Index > 0)
			{
				GON_EXPECT(Near(Floors[Index].Top() - Floors[Index - 1].Top(), Params.StepRise));
				GON_EXPECT(Near(Floors[Index].Center.X - Floors[Index - 1].Center.X, Params.StepRun));
			}
		}
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Roof), 2); // sabats at 300 and 900
		GON_EXPECT(!Shape.Doorways.empty());
		GON_EXPECT(!Shape.CoverPoints.empty());
		for (const FCoverPoint& Point : Shape.CoverPoints)
		{
			GON_EXPECT(Point.Height == ECoverHeight::High);
			for (const FKitBox& Doorway : Shape.Doorways)
			{
				GON_EXPECT(!InsideFootprint(Doorway, Point.Position));
			}
			// Inside the alley a point stands on its terrace; outside, on the ground.
			if (std::fabs(Point.Position.Y) < Params.Width * 0.5)
			{
				const int Terrace = static_cast<int>(std::floor(Point.Position.X / Params.StepRun));
				GON_EXPECT(Near(Point.Position.Z, Params.FloorThickness + Params.StepRise * Terrace));
			}
			else
			{
				GON_EXPECT(Near(Point.Position.Z, 0.0));
			}
		}
		// Walls stand on the terraces (never floating, never sunk).
		for (const FKitBox& Box : Shape.Boxes)
		{
			if (Box.Role == EKitRole::Wall && Box.bCoverSource)
			{
				const int Terrace = static_cast<int>(std::floor(Box.Center.X / Params.StepRun));
				GON_EXPECT(Near(Box.Bottom(), Params.FloorThickness + Params.StepRise * Terrace));
			}
		}

		FAlleyParams Flat;
		Flat.StepRise = 0.0;
		Flat.DoorSpacing = 0.0;
		Flat.SabatSpacing = 0.0;
		const FKitShape FlatShape = BuildAlley(Flat);
		GON_EXPECT_EQ(CountRole(FlatShape, EKitRole::Floor), 1);
		GON_EXPECT_EQ(CountRole(FlatShape, EKitRole::Wall), 2);
		GON_EXPECT_EQ(CountRole(FlatShape, EKitRole::Roof), 0);
		GON_EXPECT(FlatShape.Doorways.empty());
	}

	GON_LOGIC_TEST("Blockout.Kit.AlleyValidator", Blockout_Kit_AlleyBad)
	{
		FAlleyParams Narrow;
		Narrow.Width = 100.0;
		const FKitShape NarrowShape = BuildAlley(Narrow);
		GON_EXPECT_EQ(NarrowShape.Issues.size(), size_t(1));
		GON_EXPECT(HasIssueContaining(NarrowShape, "narrower"));

		FAlleyParams Steep;
		Steep.StepRise = 60.0;
		GON_EXPECT(HasIssueContaining(BuildAlley(Steep), "max step"));
	}

	GON_LOGIC_TEST("Blockout.Kit.RockIsDeterministic", Blockout_Kit_Rock)
	{
		const FRockParams Params;
		const FKitShape First = BuildRock(Params);
		const FKitShape Second = BuildRock(Params);
		FRockParams Other = Params;
		Other.Seed = 2;
		const FKitShape Third = BuildRock(Other);
		GON_REQUIRE(First.Boxes.size() == static_cast<size_t>(Params.Chunks));
		GON_REQUIRE(Second.Boxes.size() == First.Boxes.size());
		GON_REQUIRE(Third.Boxes.size() == First.Boxes.size());
		bool bAllSame = true;
		bool bAnyDifferent = false;
		for (size_t Index = 0; Index < First.Boxes.size(); ++Index)
		{
			const FKitBox& A = First.Boxes[Index];
			const FKitBox& B = Second.Boxes[Index];
			const FKitBox& C = Third.Boxes[Index];
			bAllSame = bAllSame && A.Center.X == B.Center.X && A.Center.Y == B.Center.Y && A.Extent.X == B.Extent.X && A.Extent.Y == B.Extent.Y && A.Extent.Z == B.Extent.Z && A.YawDeg == B.YawDeg;
			bAnyDifferent = bAnyDifferent || A.Center.X != C.Center.X || A.Extent.X != C.Extent.X || A.YawDeg != C.YawDeg;
		}
		GON_EXPECT(bAllSame);
		GON_EXPECT(bAnyDifferent);
		GON_EXPECT_EQ(First.CoverPoints.size(), Second.CoverPoints.size());

		for (uint32_t Seed = 0; Seed < 50; ++Seed)
		{
			FRockParams Random;
			Random.Seed = Seed;
			Random.Chunks = 8;
			for (const FKitBox& Box : BuildRock(Random).Boxes)
			{
				const FVec3 Bounds = Box.BoundsExtent();
				GON_EXPECT(Box.Role == EKitRole::Rock && Box.bCoverSource);
				GON_EXPECT(std::fabs(Box.Center.X) + Bounds.X <= Random.SizeX * 0.5 + 1e-6);
				GON_EXPECT(std::fabs(Box.Center.Y) + Bounds.Y <= Random.SizeY * 0.5 + 1e-6);
				GON_EXPECT(Box.Top() <= Random.SizeZ + 1e-6);
				GON_EXPECT(Near(Box.Bottom(), 0.0));
			}
		}

		FRockParams TooMany;
		TooMany.Chunks = 20;
		const FKitShape Clamped = BuildRock(TooMany);
		GON_EXPECT_EQ(Clamped.Boxes.size(), size_t(8));
		GON_EXPECT_EQ(Clamped.Issues.size(), size_t(1));
	}

	GON_LOGIC_TEST("Blockout.Kit.StairsValidator", Blockout_Kit_Stairs)
	{
		const FStairsParams Params;
		const FKitShape Shape = BuildStairs(Params);
		GON_EXPECT(Shape.Issues.empty());
		GON_EXPECT_EQ(CountRole(Shape, EKitRole::Stairs), Params.StepCount + 1); // + landing
		GON_EXPECT(Near(MaxTop(Shape, EKitRole::Stairs), Params.StepRise * Params.StepCount));
		GON_EXPECT(Shape.CoverPoints.empty());
		GON_EXPECT(Shape.Label.find("walkable") != std::string::npos);

		FStairsParams Steep;
		Steep.StepRise = 60.0;
		const FKitShape SteepShape = BuildStairs(Steep);
		GON_EXPECT_EQ(SteepShape.Issues.size(), size_t(1));
		GON_EXPECT(SteepShape.Label.find("too high") != std::string::npos);

		FStairsParams NoSteps;
		NoSteps.StepCount = 0;
		NoSteps.LandingDepth = 0.0;
		const FKitShape NoStepsShape = BuildStairs(NoSteps);
		GON_EXPECT_EQ(NoStepsShape.Issues.size(), size_t(1));
		GON_EXPECT_EQ(CountRole(NoStepsShape, EKitRole::Stairs), 1);
	}

	GON_LOGIC_TEST("Blockout.Kit.SlopeValidator", Blockout_Kit_Slope)
	{
		FSlopeParams Params; // 30 deg
		const FKitShape Walkable = BuildSlope(Params);
		GON_REQUIRE(Walkable.Boxes.size() == 1);
		GON_EXPECT(Walkable.Boxes[0].Role == EKitRole::Slope);
		GON_EXPECT(Walkable.Label.find("walkable") != std::string::npos);
		// Low end at the origin: the top surface passes through (0, 0, 0) and rises L * sin(angle).
		const double Radians = Params.AngleDeg * 3.14159265358979323846 / 180.0;
		const FKitBox& Box = Walkable.Boxes[0];
		GON_EXPECT(Box.Bottom() <= 0.0 && Box.Bottom() >= -Params.Thickness - 1e-6);
		GON_EXPECT(Near(Box.Top(), std::sin(Radians) * Params.Length, 1e-6) || Box.Top() >= std::sin(Radians) * Params.Length);
		GON_EXPECT(Box.Top() <= std::sin(Radians) * Params.Length + Params.Thickness);
		GON_EXPECT(Box.Center.X - Box.BoundsExtent().X <= 1e-6);

		Params.AngleDeg = 50.0;
		const FKitShape Steep = BuildSlope(Params);
		GON_EXPECT(Steep.Boxes[0].Role == EKitRole::SlopeTooSteep);
		GON_EXPECT(Steep.Label.find("too steep") != std::string::npos);
		GON_EXPECT(Steep.Issues.empty());
	}

	GON_LOGIC_TEST("Blockout.Kit.AllBuildersAreSane", Blockout_Kit_Sane)
	{
		for (const FNamedShape& Named : AllDefaultShapes())
		{
			const FKitShape& Shape = Named.Shape;
			if (!Shape.Issues.empty())
			{
				Ctx.Fail(__FILE__, __LINE__, std::string(Named.Name) + ": default params report: " + Shape.Issues[0]);
			}
			GON_EXPECT(!Shape.Boxes.empty());
			GON_EXPECT(!Shape.Label.empty());
			const bool bBridge = std::string(Named.Name) == "bridge";
			for (const FKitBox& Box : Shape.Boxes)
			{
				++Ctx.Checks;
				const bool bFinite = std::isfinite(Box.Center.X) && std::isfinite(Box.Center.Y) && std::isfinite(Box.Center.Z)
					&& std::isfinite(Box.Extent.X) && std::isfinite(Box.Extent.Y) && std::isfinite(Box.Extent.Z);
				if (!bFinite || Box.Extent.X <= 0.0 || Box.Extent.Y <= 0.0 || Box.Extent.Z <= 0.0)
				{
					Ctx.Fail(__FILE__, __LINE__, std::string(Named.Name) + ": degenerate " + KitRoleName(Box.Role) + " box");
				}
				if (bBridge)
				{
					GON_EXPECT(Box.Bottom() >= -FRoofBridgeParams().Thickness - 1.0);
				}
				else if (Box.Bottom() < -FSlopeParams().Thickness - 1.0)
				{
					Ctx.Fail(__FILE__, __LINE__, std::string(Named.Name) + ": box below the ground");
				}
			}
			if (bBridge)
			{
				GON_EXPECT(Near(Shape.Boxes[0].Top(), 0.0));
			}
		}
	}

	GON_LOGIC_TEST("Blockout.Kit.BadInputIsClamped", Blockout_Kit_BadInput)
	{
		const double NotANumber = std::numeric_limits<double>::quiet_NaN();
		FDouarHouseParams House;
		House.RoomSizeX = NotANumber;
		House.WallThickness = -5.0;
		FAlleyParams Alley;
		Alley.Length = 1e9;
		Alley.StepRun = 0.0;
		FRoofBridgeParams Bridge;
		Bridge.Width = 0.0;
		FWallParams Wall;
		Wall.Length = -1.0;
		FStairsParams Stairs;
		Stairs.StepCount = -3;
		FRockParams Rock;
		Rock.SizeZ = NotANumber;
		FSlopeParams Slope;
		Slope.AngleDeg = 120.0;
		FCoverParams Cover;
		Cover.Thickness = 0.0;
		FCoverRules Rules;
		Rules.Spacing = 0.0; // would loop forever if not clamped
		const std::vector<FKitShape> Shapes = {
			BuildDouarHouse(House, Rules), BuildAlley(Alley, Rules), BuildRoofBridge(Bridge, Rules), BuildWall(Wall, Rules),
			BuildStairs(Stairs, Rules), BuildRock(Rock, Rules), BuildSlope(Slope, Rules), BuildCover(Cover, Rules),
		};
		for (const FKitShape& Shape : Shapes)
		{
			GON_EXPECT(Shape.Issues.size() >= 2); // the param and the rules
			for (const FKitBox& Box : Shape.Boxes)
			{
				GON_EXPECT(std::isfinite(Box.Center.Z) && Box.Extent.X > 0.0 && Box.Extent.Y > 0.0 && Box.Extent.Z > 0.0);
			}
		}
	}

	GON_LOGIC_TEST("Blockout.Kit.CapsuleOverlapIsYawAware", Blockout_Kit_Capsule)
	{
		FKitBox Box;
		Box.Center = FVec3{0.0, 0.0, 100.0};
		Box.Extent = FVec3{200.0, 10.0, 100.0};
		Box.YawDeg = 45.0;
		// Inside the world bounds but far from the rotated slab.
		GON_EXPECT(!CapsuleOverlapsKitBox(FVec3{100.0, -100.0, 96.0}, 42.0, 96.0, Box));
		GON_EXPECT(CapsuleOverlapsKitBox(FVec3{100.0, 100.0, 96.0}, 42.0, 96.0, Box));
		// Touching the top is not overlapping.
		GON_EXPECT(!CapsuleOverlapsKitBox(FVec3{100.0, 100.0, 296.0}, 42.0, 96.0, Box));
		Box.YawDeg = 0.0;
		GON_EXPECT(!CapsuleOverlapsKitBox(FVec3{0.0, 52.0, 96.0}, 42.0, 96.0, Box));
		GON_EXPECT(CapsuleOverlapsKitBox(FVec3{0.0, 51.0, 96.0}, 42.0, 96.0, Box));
		// Corner: outside by (30, 30) is 42.4 cm away.
		GON_EXPECT(!CapsuleOverlapsKitBox(FVec3{230.0, 40.0, 96.0}, 42.0, 96.0, Box));
		GON_EXPECT(CapsuleOverlapsKitBox(FVec3{229.0, 39.0, 96.0}, 42.0, 96.0, Box));
	}
} // namespace GoNBlockoutKitTestsDetail

#endif // GON_WITH_LOGIC_TESTS
