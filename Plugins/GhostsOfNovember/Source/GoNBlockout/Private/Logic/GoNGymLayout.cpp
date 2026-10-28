#include "Logic/GoNGymLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace GoN::Blockout
{
	namespace GymDetail
	{
		constexpr double DegToRad = 3.14159265358979323846 / 180.0;

		std::string Centimetres(double Value)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.0f cm", Value);
			return Buffer;
		}

		std::string Degrees(double Value)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.0f deg", Value);
			return Buffer;
		}

		FGymBox MakeBox(EGymPiece Piece, const std::string& Label, double CenterX, double CenterY, double SizeX, double SizeY, double Height)
		{
			FGymBox Box;
			Box.Piece = Piece;
			Box.Label = Label;
			Box.Center = FVec3{CenterX, CenterY, Height * 0.5};
			Box.Extent = FVec3{SizeX * 0.5, SizeY * 0.5, Height * 0.5};
			return Box;
		}
	} // namespace GymDetail

	const char* GymPieceName(EGymPiece Piece)
	{
		switch (Piece)
		{
		case EGymPiece::Floor: return "Floor";
		case EGymPiece::Wall: return "Wall";
		case EGymPiece::LowCover: return "LowCover";
		case EGymPiece::HighCover: return "HighCover";
		case EGymPiece::Vault: return "Vault";
		case EGymPiece::Ledge: return "Ledge";
		case EGymPiece::Stairs: return "Stairs";
		case EGymPiece::Ramp: return "Ramp";
		case EGymPiece::DoorWall: return "DoorWall";
		}
		return "?";
	}

	FVec3 FGymBox::BoundsExtent() const
	{
		const double Pitch = PitchDeg * GymDetail::DegToRad;
		const double Yaw = YawDeg * GymDetail::DegToRad;
		const double PitchedX = std::fabs(std::cos(Pitch)) * Extent.X + std::fabs(std::sin(Pitch)) * Extent.Z;
		const double PitchedZ = std::fabs(std::sin(Pitch)) * Extent.X + std::fabs(std::cos(Pitch)) * Extent.Z;
		FVec3 Out;
		Out.X = std::fabs(std::cos(Yaw)) * PitchedX + std::fabs(std::sin(Yaw)) * Extent.Y;
		Out.Y = std::fabs(std::sin(Yaw)) * PitchedX + std::fabs(std::cos(Yaw)) * Extent.Y;
		Out.Z = PitchedZ;
		return Out;
	}

	bool CapsuleOverlapsBox(const FVec3& Center, double Radius, double HalfHeight, const FGymBox& Box)
	{
		const FVec3 B = Box.BoundsExtent();
		return std::fabs(Center.X - Box.Center.X) < B.X + Radius
			&& std::fabs(Center.Y - Box.Center.Y) < B.Y + Radius
			&& std::fabs(Center.Z - Box.Center.Z) < B.Z + HalfHeight;
	}

	FGymLayout BuildGymLayout(const FGymMetrics& Metrics)
	{
		using namespace GymDetail;
		FGymLayout Layout;
		auto& Boxes = Layout.Boxes;
		const double Half = Metrics.FloorSize * 0.5;

		// Floor slab, top at Z = 0.
		FGymBox Floor;
		Floor.Piece = EGymPiece::Floor;
		Floor.Center = FVec3{0.0, 0.0, -10.0};
		Floor.Extent = FVec3{Half, Half, 10.0};
		Boxes.push_back(Floor);

		// Perimeter.
		const double WallThickness = 40.0;
		Boxes.push_back(MakeBox(EGymPiece::Wall, "", Half - WallThickness * 0.5, 0.0, WallThickness, Metrics.FloorSize, Metrics.WallHeight));
		Boxes.push_back(MakeBox(EGymPiece::Wall, "", -Half + WallThickness * 0.5, 0.0, WallThickness, Metrics.FloorSize, Metrics.WallHeight));
		Boxes.push_back(MakeBox(EGymPiece::Wall, "", 0.0, Half - WallThickness * 0.5, Metrics.FloorSize - 2.0 * WallThickness, WallThickness, Metrics.WallHeight));
		Boxes.push_back(MakeBox(EGymPiece::Wall, "", 0.0, -Half + WallThickness * 0.5, Metrics.FloorSize - 2.0 * WallThickness, WallThickness, Metrics.WallHeight));

		// Lane A (Y = -1800): cover heights.
		Boxes.push_back(MakeBox(EGymPiece::LowCover, "Low cover " + Centimetres(Metrics.LowCoverHeight), 600.0, -1800.0, 50.0, 250.0, Metrics.LowCoverHeight));
		Boxes.push_back(MakeBox(EGymPiece::LowCover, "", 1100.0, -1800.0, 250.0, 50.0, Metrics.LowCoverHeight));
		Boxes.push_back(MakeBox(EGymPiece::HighCover, "High cover " + Centimetres(Metrics.HighCoverHeight), 1700.0, -1800.0, 50.0, 250.0, Metrics.HighCoverHeight));
		Boxes.push_back(MakeBox(EGymPiece::HighCover, "", 2200.0, -1800.0, 250.0, 50.0, Metrics.HighCoverHeight));

		// Lane B (Y = -700): vault then climb heights.
		double X = 600.0;
		for (const double Height : Metrics.VaultHeights)
		{
			Boxes.push_back(MakeBox(EGymPiece::Vault, "Vault " + Centimetres(Height), X, -700.0, 40.0, 300.0, Height));
			X += 400.0;
		}
		for (const double Height : Metrics.LedgeHeights)
		{
			Boxes.push_back(MakeBox(EGymPiece::Ledge, "Climb " + Centimetres(Height), X, -700.0, 200.0, 300.0, Height));
			X += 400.0;
		}

		// Lane C (Y = 400): solid stairs up to a landing.
		const double StairStartX = 600.0;
		for (int Step = 0; Step < Metrics.StepCount; ++Step)
		{
			const double Height = Metrics.StepRise * (Step + 1);
			const std::string Label = Step == 0 ? "Stairs rise " + Centimetres(Metrics.StepRise) + " / run " + Centimetres(Metrics.StepRun) : std::string();
			Boxes.push_back(MakeBox(EGymPiece::Stairs, Label, StairStartX + Metrics.StepRun * (Step + 0.5), 400.0, Metrics.StepRun, 300.0, Height));
		}
		const double LandingSize = 400.0;
		const double LandingX = StairStartX + Metrics.StepRun * Metrics.StepCount + LandingSize * 0.5;
		Boxes.push_back(MakeBox(EGymPiece::Stairs, "", LandingX, 400.0, LandingSize, 300.0, Metrics.StepRise * Metrics.StepCount));

		// Lane D (Y = 1500): ramps, low end on the floor at RampStartX, rising toward +X.
		const double RampLength = 600.0;
		const double RampThickness = 20.0;
		double RampStartX = 400.0;
		for (const double Angle : Metrics.RampAnglesDeg)
		{
			const double Radians = Angle * DegToRad;
			FGymBox Ramp;
			Ramp.Piece = EGymPiece::Ramp;
			Ramp.Label = "Ramp " + Degrees(Angle) + (Angle <= Metrics.WalkableSlopeDeg ? " (walkable)" : " (too steep)");
			Ramp.PitchDeg = Angle;
			Ramp.Extent = FVec3{RampLength * 0.5, 150.0, RampThickness * 0.5};
			// Centre of the top-surface midpoint, then down by half the thickness along the normal.
			const double MidX = RampStartX + std::cos(Radians) * RampLength * 0.5;
			const double MidZ = std::sin(Radians) * RampLength * 0.5;
			Ramp.Center = FVec3{MidX + std::sin(Radians) * RampThickness * 0.5, 1500.0, MidZ - std::cos(Radians) * RampThickness * 0.5};
			Boxes.push_back(Ramp);
			RampStartX += 800.0;
		}

		// Doorway wall across the arena at X = -1200, one gap per door width.
		const double DoorWallX = -1200.0;
		const double DoorWallHeight = 300.0;
		const double Inner = Half - WallThickness;
		const size_t DoorCount = Metrics.DoorWidths.size();
		std::vector<double> GapCenters;
		for (size_t Index = 0; Index < DoorCount; ++Index)
		{
			GapCenters.push_back(-Inner + 2.0 * Inner * (static_cast<double>(Index) + 1.0) / (static_cast<double>(DoorCount) + 1.0));
		}
		double SegmentStart = -Inner;
		for (size_t Index = 0; Index <= DoorCount; ++Index)
		{
			const double SegmentEnd = Index < DoorCount ? GapCenters[Index] - Metrics.DoorWidths[Index] * 0.5 : Inner;
			Boxes.push_back(MakeBox(EGymPiece::DoorWall, std::string(), DoorWallX, (SegmentStart + SegmentEnd) * 0.5, 30.0, SegmentEnd - SegmentStart, DoorWallHeight));
			if (Index < DoorCount)
			{
				Layout.Labels.push_back(FGymLabel{"Door " + Centimetres(Metrics.DoorWidths[Index]), FVec3{DoorWallX, GapCenters[Index], DoorWallHeight + 40.0}});
				SegmentStart = GapCenters[Index] + Metrics.DoorWidths[Index] * 0.5;
			}
		}

		// Spawn in the open patrol yard behind the doorway wall, facing the courses.
		Layout.PlayerStart = FVec3{-2200.0, 0.0, Metrics.CapsuleHalfHeight + 2.0};
		Layout.PlayerYawDeg = 0.0;
		return Layout;
	}

	std::vector<std::string> ValidateGymLayout(const FGymLayout& Layout, const FGymMetrics& Metrics)
	{
		std::vector<std::string> Issues;
		const double Half = Metrics.FloorSize * 0.5;

		for (const FGymBox& Box : Layout.Boxes)
		{
			const FVec3 B = Box.BoundsExtent();
			if (Box.Piece != EGymPiece::Floor && (std::fabs(Box.Center.X) + B.X > Half + 0.01 || std::fabs(Box.Center.Y) + B.Y > Half + 0.01))
			{
				Issues.push_back(std::string(GymPieceName(Box.Piece)) + " '" + Box.Label + "' extends past the floor");
			}
			if (Box.Piece != EGymPiece::Floor && CapsuleOverlapsBox(Layout.PlayerStart, Metrics.CapsuleRadius, Metrics.CapsuleHalfHeight, Box))
			{
				Issues.push_back(std::string("player start overlaps ") + GymPieceName(Box.Piece));
			}
		}
		if (Layout.PlayerStart.Z - Metrics.CapsuleHalfHeight < 0.0)
		{
			Issues.push_back("player start capsule is below the floor");
		}
		if (Metrics.StepRise > Metrics.MaxStepHeight)
		{
			Issues.push_back("stair rise is higher than MaxStepHeight; stairs cannot be walked");
		}
		if (Metrics.LowCoverHeight >= Metrics.CapsuleHalfHeight * 2.0)
		{
			Issues.push_back("low cover is taller than a standing character");
		}
		bool bWalkableRamp = false;
		bool bSteepRamp = false;
		for (const double Angle : Metrics.RampAnglesDeg)
		{
			bWalkableRamp = bWalkableRamp || Angle <= Metrics.WalkableSlopeDeg;
			bSteepRamp = bSteepRamp || Angle > Metrics.WalkableSlopeDeg;
		}
		if (!bWalkableRamp || !bSteepRamp)
		{
			Issues.push_back("ramps should include both a walkable and a too-steep slope");
		}
		for (const double Width : Metrics.DoorWidths)
		{
			if (Width < Metrics.CapsuleRadius * 2.0)
			{
				Issues.push_back("door width " + GymDetail::Centimetres(Width) + " is narrower than the capsule");
			}
		}
		return Issues;
	}
} // namespace GoN::Blockout
