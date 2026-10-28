// Ghosts of November — layout of the C++-generated test gym (pure C++).
// The gym is a metrics course: cover heights, vault/climb heights, stair rise, ramp angles
// and doorway widths taken from FGymMetrics, so movement/stealth tuning can be judged
// against fixed references. AGoNGymBuilder turns the boxes into instanced cubes.
// Units: centimetres, Unreal axes (X forward, Y right, Z up), floor top at Z = 0.
#pragma once

#include <string>
#include <vector>

namespace GoN::Blockout
{
	enum class EGymPiece
	{
		Floor,
		Wall,
		LowCover,
		HighCover,
		Vault,
		Ledge,
		Stairs,
		Ramp,
		DoorWall
	};

	const char* GymPieceName(EGymPiece Piece);

	struct FVec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	struct FGymMetrics
	{
		// Defaults match UE's ACharacter / UCharacterMovementComponent defaults.
		double CapsuleRadius = 42.0;
		double CapsuleHalfHeight = 96.0;
		double MaxStepHeight = 45.0;
		double WalkableSlopeDeg = 44.765;

		double FloorSize = 6000.0;
		double WallHeight = 400.0;
		double LowCoverHeight = 100.0;
		double HighCoverHeight = 200.0;
		double StepRise = 20.0;
		double StepRun = 30.0;
		int StepCount = 10;
		std::vector<double> VaultHeights{60.0, 90.0, 120.0};
		std::vector<double> LedgeHeights{150.0, 200.0, 250.0};
		std::vector<double> RampAnglesDeg{20.0, 35.0, 50.0};
		std::vector<double> DoorWidths{90.0, 120.0, 200.0};
	};

	struct FGymBox
	{
		EGymPiece Piece = EGymPiece::Floor;
		std::string Label; // shown above the piece in the gym ("" = no label)
		FVec3 Center;
		FVec3 Extent; // half size
		double PitchDeg = 0.0;
		double YawDeg = 0.0;

		// World-space axis-aligned bounds (accounts for pitch/yaw).
		FVec3 BoundsExtent() const;
		double Top() const { return Center.Z + BoundsExtent().Z; }
	};

	// Free-standing label (e.g. over a doorway gap, where there is no box).
	struct FGymLabel
	{
		std::string Text;
		FVec3 Position;
	};

	struct FGymLayout
	{
		std::vector<FGymBox> Boxes;
		std::vector<FGymLabel> Labels;
		FVec3 PlayerStart; // capsule centre
		double PlayerYawDeg = 0.0;
	};

	FGymLayout BuildGymLayout(const FGymMetrics& Metrics);

	// Does an upright capsule at Center overlap the box bounds? (conservative)
	bool CapsuleOverlapsBox(const FVec3& Center, double Radius, double HalfHeight, const FGymBox& Box);

	// Returns issues as text (empty = layout is consistent with the metrics).
	std::vector<std::string> ValidateGymLayout(const FGymLayout& Layout, const FGymMetrics& Metrics);
} // namespace GoN::Blockout
