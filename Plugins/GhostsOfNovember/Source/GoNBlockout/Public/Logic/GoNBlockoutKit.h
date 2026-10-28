// Ghosts of November — geometry of the placeable blockout kit (pure C++).
// Every piece is a list of boxes (built as instanced cubes by AGoNBlockoutPiece) plus the
// cover points derived from them, so metrics and cover rules are testable without Unreal.
// Units: centimetres, Unreal axes (X forward, Y right, Z up). The piece origin is on the
// ground it stands on (Z = 0). Rotations are Unreal's: positive pitch lifts +X toward +Z,
// positive yaw turns +X toward +Y.
#pragma once

#include "Logic/GoNGymLayout.h" // FVec3

#include <cstdint>
#include <string>
#include <vector>

namespace GoN::Blockout
{
	enum class EKitRole
	{
		Floor,
		Wall,
		Roof,
		Parapet,
		LowCover,
		HighCover,
		Stairs,
		Slope,
		SlopeTooSteep,
		Rock,
		Bridge
	};
	constexpr int KitRoleCount = static_cast<int>(EKitRole::Bridge) + 1;

	const char* KitRoleName(EKitRole Role);

	struct FKitBox
	{
		EKitRole Role = EKitRole::Floor;
		FVec3 Center;
		FVec3 Extent; // half size, before rotation
		double PitchDeg = 0.0;
		double YawDeg = 0.0;
		bool bCoverSource = false; // its vertical faces generate cover points

		// World-space axis-aligned half size (accounts for pitch/yaw).
		FVec3 BoundsExtent() const;
		double Bottom() const;
		double Top() const;
	};

	enum class ECoverHeight
	{
		Low,
		High
	};

	struct FCoverPoint
	{
		FVec3 Position; // capsule foot position on the floor
		FVec3 Normal;   // unit, horizontal, from the cover face toward the character
		ECoverHeight Height = ECoverHeight::Low;
		double CoverTop = 0.0; // Z of the top of the cover
	};

	struct FCoverRules
	{
		double CapsuleRadius = 42.0;
		double CapsuleHalfHeight = 96.0;
		double Spacing = 100.0;     // between points along a face
		double EdgeMargin = 30.0;   // first/last point distance from a face end
		double Clearance = 5.0;     // gap between the capsule and the cover face
		double LowMinHeight = 80.0;
		double LowMaxHeight = 140.0;
		double HighMinHeight = 160.0;
		double MinFaceLength = 60.0;
		double MaxStepHeight = 45.0; // floors this much above the cover base still count as "the floor"
	};

	struct FKitShape
	{
		std::vector<FKitBox> Boxes;
		std::vector<FCoverPoint> CoverPoints;
		std::vector<std::string> Issues; // readable; parameters are clamped to safe values
		std::string Label;               // metrics shown above the piece
		std::vector<FKitBox> Doorways;   // empty door volumes (not built): for tests and AI
	};

	// Douar house. Origin at the centre of the footprint. Room sizes are interior (clear)
	// sizes; walls are outside them. Rooms are side by side along X; the front wall is the
	// -X wall, with the front door centred in it (the player walks +X into the house). With
	// 2 rooms the dividing wall has an interior door. Exterior stairs run along the +Y side
	// wall, rising toward +X, and arrive at a gap in the +Y parapet near the back (+X) corner.
	struct FDouarHouseParams
	{
		int Rooms = 1; // 1 or 2
		double RoomSizeX = 400.0;
		double RoomSizeY = 400.0;
		double WallHeight = 300.0;
		double WallThickness = 40.0;
		double DoorWidth = 100.0;
		double DoorHeight = 210.0;
		double FloorThickness = 5.0; // interior slab on the ground (0 = none)
		double RoofThickness = 20.0;
		double ParapetHeight = 40.0; // 0 = none
		bool bExteriorStairs = true;
		double StairWidth = 100.0;
		double StairRise = 20.0; // maximum; adjusted down so the top step is flush with the roof
		double StairRun = 30.0;
	};

	// Casbah alley along +X from the origin, centred on Y = 0. Terraces (solid, under the walls
	// too) rise by StepRise every StepRun; wall segments sit on the terraces and share one top.
	// Doorways are staggered: -Y wall at (k + 0.5) * DoorSpacing, +Y wall at (k + 1) * DoorSpacing.
	// Sabats (covered passages bridging the alley) at (k + 0.5) * SabatSpacing.
	struct FAlleyParams
	{
		double Length = 1200.0;
		double Width = 200.0; // clear width between the walls
		double WallHeight = 600.0; // above the highest terrace
		double WallThickness = 40.0;
		double FloorThickness = 5.0; // height of the first terrace above the ground
		double StepRise = 15.0; // 0 = flat
		double StepRun = 150.0;
		double DoorWidth = 100.0;
		double DoorHeight = 210.0;
		double DoorSpacing = 400.0; // 0 = no doorways
		double SabatSpacing = 600.0; // 0 = no sabats
		double SabatThickness = 60.0; // vertical thickness of the passage slab
		double SabatDepth = 200.0; // along the alley
	};

	// Plank along X centred on the origin, TOP at Z = 0: place the actor at roof height.
	struct FRoofBridgeParams
	{
		double Length = 400.0;
		double Width = 100.0;
		double Thickness = 20.0;
		double RailHeight = 0.0; // 0 = no rails
	};

	// Along X, centred on the origin.
	struct FWallParams
	{
		double Length = 400.0;
		double Height = 250.0;
		double Thickness = 30.0;
	};

	// Solid steps rising along +X from the origin, centred on Y = 0, then an optional landing.
	struct FStairsParams
	{
		double Width = 200.0;
		double StepRise = 20.0;
		double StepRun = 30.0;
		int StepCount = 10;
		double LandingDepth = 200.0; // 0 = none
	};

	// Deterministic cluster of yaw-rotated boxes inside SizeX x SizeY x SizeZ, centred on the origin.
	struct FRockParams
	{
		double SizeX = 200.0;
		double SizeY = 160.0;
		double SizeZ = 120.0;
		int Chunks = 4; // 1..8
		uint32_t Seed = 1;
	};

	// Pitched slab: the low end touches the ground at the origin, rising toward +X.
	struct FSlopeParams
	{
		double AngleDeg = 30.0;
		double Length = 600.0; // along the surface
		double Width = 300.0;
		double Thickness = 20.0;
		double WalkableSlopeDeg = 44.765; // UCharacterMovementComponent default
	};

	// One cover block along X, centred on the origin.
	struct FCoverParams
	{
		ECoverHeight Height = ECoverHeight::Low;
		double Length = 250.0;
		double Thickness = 50.0;
		double HeightLow = 100.0;
		double HeightHigh = 200.0;
	};

	FKitShape BuildDouarHouse(const FDouarHouseParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildAlley(const FAlleyParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildRoofBridge(const FRoofBridgeParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildWall(const FWallParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildStairs(const FStairsParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildRock(const FRockParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildSlope(const FSlopeParams& Params, const FCoverRules& Rules = FCoverRules());
	FKitShape BuildCover(const FCoverParams& Params, const FCoverRules& Rules = FCoverRules());

	// Cover points along the vertical faces of every unpitched cover-source box.
	// A point stands on the floor in front of the face: the ground (Z = 0) or the top of a box
	// under it that is at most MaxStepHeight above the cover's base. Points whose standing
	// capsule would overlap any box are dropped (boxes whose top is within 2 cm of the feet
	// are floor, not obstacles).
	std::vector<FCoverPoint> GenerateCoverPoints(const std::vector<FKitBox>& Boxes, const FCoverRules& Rules);

	// Upright capsule (treated as a cylinder) against a box: exact for yaw-only boxes,
	// conservative (world bounds) for pitched boxes. Touching is not overlapping.
	bool CapsuleOverlapsKitBox(const FVec3& Centre, double Radius, double HalfHeight, const FKitBox& Box);
} // namespace GoN::Blockout
