#include "Logic/GoNBlockoutKit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace GoN::Blockout
{
	namespace KitDetail
	{
		constexpr double KitDegToRad = 3.14159265358979323846 / 180.0;
		// Boxes whose top is this close above the feet are floor, not obstacles.
		constexpr double FloorTolerance = 2.0;

		std::string Num(double Value)
		{
			char Buffer[48];
			const bool bWhole = std::fabs(Value - std::round(Value)) < 0.05;
			std::snprintf(Buffer, sizeof(Buffer), bWhole ? "%.0f" : "%.1f", Value);
			return Buffer;
		}

		double Clamped(double Value, double MinValue, double MaxValue, const char* Name, std::vector<std::string>& Issues)
		{
			if (!std::isfinite(Value))
			{
				Issues.push_back(std::string(Name) + " is not a number; using " + Num(MinValue));
				return MinValue;
			}
			if (Value < MinValue)
			{
				Issues.push_back(std::string(Name) + " " + Num(Value) + " is below " + Num(MinValue) + "; clamped");
				return MinValue;
			}
			if (Value > MaxValue)
			{
				Issues.push_back(std::string(Name) + " " + Num(Value) + " is above " + Num(MaxValue) + "; clamped");
				return MaxValue;
			}
			return Value;
		}

		int ClampedInt(int Value, int MinValue, int MaxValue, const char* Name, std::vector<std::string>& Issues)
		{
			if (Value < MinValue || Value > MaxValue)
			{
				const int Result = std::clamp(Value, MinValue, MaxValue);
				Issues.push_back(std::string(Name) + " " + std::to_string(Value) + " is outside " + std::to_string(MinValue) + ".." + std::to_string(MaxValue) + "; using " + std::to_string(Result));
				return Result;
			}
			return Value;
		}

		// Rules are clamped silently by GenerateCoverPoints and with issues by the builders.
		FCoverRules SafeRules(const FCoverRules& In, std::vector<std::string>* Issues)
		{
			std::vector<std::string> Ignored;
			std::vector<std::string>& Sink = Issues != nullptr ? *Issues : Ignored;
			FCoverRules Rules;
			Rules.CapsuleRadius = Clamped(In.CapsuleRadius, 1.0, 500.0, "cover rules: capsule radius", Sink);
			Rules.CapsuleHalfHeight = Clamped(In.CapsuleHalfHeight, Rules.CapsuleRadius, 1000.0, "cover rules: capsule half height", Sink);
			Rules.Spacing = Clamped(In.Spacing, 10.0, 10000.0, "cover rules: spacing", Sink);
			Rules.EdgeMargin = Clamped(In.EdgeMargin, 0.0, 1000.0, "cover rules: edge margin", Sink);
			Rules.Clearance = Clamped(In.Clearance, 0.0, 100.0, "cover rules: clearance", Sink);
			Rules.LowMinHeight = Clamped(In.LowMinHeight, 0.0, 1000.0, "cover rules: low min height", Sink);
			Rules.LowMaxHeight = Clamped(In.LowMaxHeight, Rules.LowMinHeight, 1000.0, "cover rules: low max height", Sink);
			Rules.HighMinHeight = Clamped(In.HighMinHeight, Rules.LowMaxHeight, 10000.0, "cover rules: high min height", Sink);
			Rules.MinFaceLength = Clamped(In.MinFaceLength, 1.0, 100000.0, "cover rules: min face length", Sink);
			Rules.MaxStepHeight = Clamped(In.MaxStepHeight, 0.0, 200.0, "cover rules: max step height", Sink);
			return Rules;
		}

		FKitBox SpanBox(EKitRole Role, double MinX, double MaxX, double MinY, double MaxY, double MinZ, double MaxZ, bool bCover)
		{
			FKitBox Box;
			Box.Role = Role;
			Box.Center = FVec3{(MinX + MaxX) * 0.5, (MinY + MaxY) * 0.5, (MinZ + MaxZ) * 0.5};
			Box.Extent = FVec3{(MaxX - MinX) * 0.5, (MaxY - MinY) * 0.5, (MaxZ - MinZ) * 0.5};
			Box.bCoverSource = bCover;
			return Box;
		}

		bool IsPitched(const FKitBox& Box)
		{
			return std::fabs(Box.PitchDeg) > 1e-9;
		}

		// Is (X, Y) inside the box footprint (yaw-aware; not for pitched boxes)?
		bool FootprintContains(const FKitBox& Box, double X, double Y)
		{
			const double Yaw = Box.YawDeg * KitDegToRad;
			const double Dx = X - Box.Center.X;
			const double Dy = Y - Box.Center.Y;
			const double LocalX = Dx * std::cos(Yaw) + Dy * std::sin(Yaw);
			const double LocalY = -Dx * std::sin(Yaw) + Dy * std::cos(Yaw);
			return std::fabs(LocalX) <= Box.Extent.X && std::fabs(LocalY) <= Box.Extent.Y;
		}

		const char* CoverClass(double Height, const FCoverRules& Rules)
		{
			if (Height >= Rules.LowMinHeight && Height <= Rules.LowMaxHeight)
			{
				return "low cover";
			}
			if (Height >= Rules.HighMinHeight)
			{
				return "high cover";
			}
			return Height < Rules.LowMinHeight ? "too low for cover" : "no cover: between low and high";
		}

		void Finish(FKitShape& Shape, const FCoverRules& Rules)
		{
			Shape.CoverPoints = GenerateCoverPoints(Shape.Boxes, Rules);
		}

		// Small deterministic LCG (Numerical Recipes constants); same seed -> same rock.
		struct FKitLcg
		{
			uint32_t State = 1u;

			double Next01()
			{
				State = State * 1664525u + 1013904223u;
				return static_cast<double>(State >> 8) / 16777216.0;
			}

			double Range(double MinValue, double MaxValue)
			{
				return MinValue + (MaxValue - MinValue) * Next01();
			}
		};

		// Emits [From, To] minus the sorted, non-overlapping Gaps as wall segments.
		void EmitSegments(double From, double To, const std::vector<std::pair<double, double>>& Gaps, const FKitBox& Template, std::vector<FKitBox>& Boxes)
		{
			const auto Emit = [&Template, &Boxes](double SegmentStart, double SegmentEnd)
			{
				if (SegmentEnd - SegmentStart > 0.5)
				{
					FKitBox Segment = Template;
					Segment.Center.X = (SegmentStart + SegmentEnd) * 0.5;
					Segment.Extent.X = (SegmentEnd - SegmentStart) * 0.5;
					Boxes.push_back(Segment);
				}
			};
			double Cursor = From;
			for (const std::pair<double, double>& Gap : Gaps)
			{
				if (Gap.second <= From || Gap.first >= To)
				{
					continue;
				}
				Emit(Cursor, std::min(Gap.first, To));
				Cursor = std::max(Cursor, Gap.second);
			}
			Emit(Cursor, To);
		}
	} // namespace KitDetail

	const char* KitRoleName(EKitRole Role)
	{
		switch (Role)
		{
		case EKitRole::Floor: return "Floor";
		case EKitRole::Wall: return "Wall";
		case EKitRole::Roof: return "Roof";
		case EKitRole::Parapet: return "Parapet";
		case EKitRole::LowCover: return "LowCover";
		case EKitRole::HighCover: return "HighCover";
		case EKitRole::Stairs: return "Stairs";
		case EKitRole::Slope: return "Slope";
		case EKitRole::SlopeTooSteep: return "SlopeTooSteep";
		case EKitRole::Rock: return "Rock";
		case EKitRole::Bridge: return "Bridge";
		}
		return "?";
	}

	FVec3 FKitBox::BoundsExtent() const
	{
		FGymBox Equivalent;
		Equivalent.Extent = Extent;
		Equivalent.PitchDeg = PitchDeg;
		Equivalent.YawDeg = YawDeg;
		return Equivalent.BoundsExtent();
	}

	double FKitBox::Bottom() const
	{
		return Center.Z - BoundsExtent().Z;
	}

	double FKitBox::Top() const
	{
		return Center.Z + BoundsExtent().Z;
	}

	bool CapsuleOverlapsKitBox(const FVec3& Centre, double Radius, double HalfHeight, const FKitBox& Box)
	{
		if (KitDetail::IsPitched(Box))
		{
			const FVec3 Bounds = Box.BoundsExtent();
			return std::fabs(Centre.X - Box.Center.X) < Bounds.X + Radius
				&& std::fabs(Centre.Y - Box.Center.Y) < Bounds.Y + Radius
				&& std::fabs(Centre.Z - Box.Center.Z) < Bounds.Z + HalfHeight;
		}
		if (std::fabs(Centre.Z - Box.Center.Z) >= Box.Extent.Z + HalfHeight)
		{
			return false;
		}
		const double Yaw = Box.YawDeg * KitDetail::KitDegToRad;
		const double Dx = Centre.X - Box.Center.X;
		const double Dy = Centre.Y - Box.Center.Y;
		const double LocalX = Dx * std::cos(Yaw) + Dy * std::sin(Yaw);
		const double LocalY = -Dx * std::sin(Yaw) + Dy * std::cos(Yaw);
		const double OutsideX = std::max(std::fabs(LocalX) - Box.Extent.X, 0.0);
		const double OutsideY = std::max(std::fabs(LocalY) - Box.Extent.Y, 0.0);
		return OutsideX * OutsideX + OutsideY * OutsideY < Radius * Radius;
	}

	std::vector<FCoverPoint> GenerateCoverPoints(const std::vector<FKitBox>& Boxes, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		const FCoverRules Rules = SafeRules(InRules, nullptr);
		std::vector<FCoverPoint> Points;
		for (const FKitBox& Source : Boxes)
		{
			if (!Source.bCoverSource || IsPitched(Source))
			{
				continue;
			}
			const double Yaw = Source.YawDeg * KitDegToRad;
			const double Cos = std::cos(Yaw);
			const double Sin = std::sin(Yaw);
			const double Base = Source.Bottom();
			const double CoverTop = Source.Top();
			for (int Face = 0; Face < 4; ++Face)
			{
				// Local outward normal: +X, -X, +Y, -Y.
				const double NormalX = Face == 0 ? 1.0 : (Face == 1 ? -1.0 : 0.0);
				const double NormalY = Face == 2 ? 1.0 : (Face == 3 ? -1.0 : 0.0);
				const bool bXFace = Face < 2;
				const double FaceLength = 2.0 * (bXFace ? Source.Extent.Y : Source.Extent.X);
				const double Depth = bXFace ? Source.Extent.X : Source.Extent.Y;
				if (FaceLength < Rules.MinFaceLength)
				{
					continue;
				}
				// Points are centred on the face so both ends keep at least EdgeMargin.
				const double Usable = FaceLength - 2.0 * Rules.EdgeMargin;
				const int Count = Usable > 0.0 ? static_cast<int>(std::floor(Usable / Rules.Spacing + 1e-9)) + 1 : 1;
				const double First = -0.5 * Rules.Spacing * static_cast<double>(Count - 1);
				const double Offset = Depth + Rules.CapsuleRadius + Rules.Clearance;
				const FVec3 Normal{NormalX * Cos - NormalY * Sin, NormalX * Sin + NormalY * Cos, 0.0};
				for (int Index = 0; Index < Count; ++Index)
				{
					const double Along = First + Rules.Spacing * static_cast<double>(Index);
					const double LocalX = bXFace ? NormalX * Offset : Along;
					const double LocalY = bXFace ? Along : NormalY * Offset;
					const double PointX = Source.Center.X + LocalX * Cos - LocalY * Sin;
					const double PointY = Source.Center.Y + LocalX * Sin + LocalY * Cos;

					// Floor: the ground or a box top just above the cover's base (slab, terrace).
					double FloorZ = 0.0;
					for (const FKitBox& Other : Boxes)
					{
						if (&Other == &Source || IsPitched(Other))
						{
							continue;
						}
						const double OtherTop = Other.Top();
						if (OtherTop > FloorZ && OtherTop <= Base + Rules.MaxStepHeight && FootprintContains(Other, PointX, PointY))
						{
							FloorZ = OtherTop;
						}
					}

					const double CoverHeight = CoverTop - FloorZ;
					ECoverHeight Height = ECoverHeight::Low;
					if (CoverHeight >= Rules.LowMinHeight && CoverHeight <= Rules.LowMaxHeight)
					{
						Height = ECoverHeight::Low;
					}
					else if (CoverHeight >= Rules.HighMinHeight)
					{
						Height = ECoverHeight::High;
					}
					else
					{
						continue;
					}

					const FVec3 CapsuleCentre{PointX, PointY, FloorZ + Rules.CapsuleHalfHeight};
					bool bBlocked = false;
					for (const FKitBox& Other : Boxes)
					{
						if (Other.Top() <= FloorZ + FloorTolerance)
						{
							continue;
						}
						if (CapsuleOverlapsKitBox(CapsuleCentre, Rules.CapsuleRadius, Rules.CapsuleHalfHeight, Other))
						{
							bBlocked = true;
							break;
						}
					}
					if (bBlocked)
					{
						continue;
					}

					const FVec3 Position{PointX, PointY, FloorZ};
					const double MinDistanceSq = (Rules.Spacing * 0.5) * (Rules.Spacing * 0.5);
					const bool bDuplicate = std::any_of(Points.begin(), Points.end(), [&Position, &Normal, MinDistanceSq](const FCoverPoint& Accepted)
					{
						const double Dot = Accepted.Normal.X * Normal.X + Accepted.Normal.Y * Normal.Y;
						const double DistX = Accepted.Position.X - Position.X;
						const double DistY = Accepted.Position.Y - Position.Y;
						const double DistZ = Accepted.Position.Z - Position.Z;
						return Dot > 0.99 && DistX * DistX + DistY * DistY + DistZ * DistZ < MinDistanceSq;
					});
					if (!bDuplicate)
					{
						Points.push_back(FCoverPoint{Position, Normal, Height, CoverTop});
					}
				}
			}
		}
		return Points;
	}

	FKitShape BuildDouarHouse(const FDouarHouseParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const int Rooms = ClampedInt(Params.Rooms, 1, 2, "rooms", Issues);
		const double RoomX = Clamped(Params.RoomSizeX, 100.0, 5000.0, "room size X", Issues);
		const double RoomY = Clamped(Params.RoomSizeY, 100.0, 5000.0, "room size Y", Issues);
		const double WallHeight = Clamped(Params.WallHeight, 100.0, 2000.0, "wall height", Issues);
		const double Thick = Clamped(Params.WallThickness, 5.0, 200.0, "wall thickness", Issues);
		const double DoorWidth = Clamped(Params.DoorWidth, 20.0, RoomY, "door width", Issues);
		double DoorHeight = Clamped(Params.DoorHeight, 50.0, 100000.0, "door height", Issues);
		const double FloorThick = Clamped(Params.FloorThickness, 0.0, 50.0, "floor thickness", Issues);
		const double RoofThick = Clamped(Params.RoofThickness, 1.0, 200.0, "roof thickness", Issues);
		const double Parapet = Clamped(Params.ParapetHeight, 0.0, 200.0, "parapet height", Issues);
		const double StairWidth = Clamped(Params.StairWidth, 40.0, 1000.0, "stair width", Issues);
		const double StairRise = Clamped(Params.StairRise, 1.0, 100.0, "stair rise", Issues);
		const double StairRun = Clamped(Params.StairRun, 10.0, 200.0, "stair run", Issues);

		const double MinDoor = 2.0 * Rules.CapsuleRadius + 10.0;
		if (DoorWidth < MinDoor)
		{
			Issues.push_back("door width " + Num(DoorWidth) + " cm is narrower than the capsule + 10 cm (" + Num(MinDoor) + " cm)");
		}
		if (DoorHeight > WallHeight)
		{
			Issues.push_back("door height " + Num(DoorHeight) + " cm is higher than the wall (" + Num(WallHeight) + " cm); clamped");
			DoorHeight = WallHeight;
		}
		if (Params.bExteriorStairs && StairRise > Rules.MaxStepHeight)
		{
			Issues.push_back("stair rise " + Num(StairRise) + " cm is higher than the max step (" + Num(Rules.MaxStepHeight) + " cm)");
		}

		const double OuterX = Rooms * RoomX + (Rooms + 1) * Thick;
		const double OuterY = RoomY + 2.0 * Thick;
		const double HalfX = OuterX * 0.5;
		const double HalfY = OuterY * 0.5;
		const double HalfDoor = DoorWidth * 0.5;
		std::vector<FKitBox>& Boxes = Shape.Boxes;

		// Wall with a centred door: two segments, lintel, doorway volume.
		const auto AddDoorWall = [&](double MinX, double MaxX, double MinY, double MaxY)
		{
			if (-HalfDoor - MinY > 0.5)
			{
				Boxes.push_back(SpanBox(EKitRole::Wall, MinX, MaxX, MinY, -HalfDoor, 0.0, WallHeight, true));
			}
			if (MaxY - HalfDoor > 0.5)
			{
				Boxes.push_back(SpanBox(EKitRole::Wall, MinX, MaxX, HalfDoor, MaxY, 0.0, WallHeight, true));
			}
			if (DoorHeight < WallHeight)
			{
				Boxes.push_back(SpanBox(EKitRole::Wall, MinX, MaxX, -HalfDoor, HalfDoor, DoorHeight, WallHeight, false));
			}
			Shape.Doorways.push_back(SpanBox(EKitRole::Wall, MinX, MaxX, -HalfDoor, HalfDoor, 0.0, DoorHeight, false));
		};

		AddDoorWall(-HalfX, -HalfX + Thick, -HalfY, HalfY); // front (-X), front door
		Boxes.push_back(SpanBox(EKitRole::Wall, HalfX - Thick, HalfX, -HalfY, HalfY, 0.0, WallHeight, true)); // back
		Boxes.push_back(SpanBox(EKitRole::Wall, -HalfX + Thick, HalfX - Thick, -HalfY, -HalfY + Thick, 0.0, WallHeight, true)); // -Y side
		Boxes.push_back(SpanBox(EKitRole::Wall, -HalfX + Thick, HalfX - Thick, HalfY - Thick, HalfY, 0.0, WallHeight, true)); // +Y side
		if (Rooms == 2)
		{
			const double DividerX = -HalfX + Thick + RoomX;
			AddDoorWall(DividerX, DividerX + Thick, -HalfY + Thick, HalfY - Thick);
		}
		if (FloorThick > 0.0)
		{
			for (int Room = 0; Room < Rooms; ++Room)
			{
				const double RoomMinX = -HalfX + Thick + Room * (RoomX + Thick);
				Boxes.push_back(SpanBox(EKitRole::Floor, RoomMinX, RoomMinX + RoomX, -HalfY + Thick, HalfY - Thick, 0.0, FloorThick, false));
			}
		}
		const double RoofTop = WallHeight + RoofThick;
		Boxes.push_back(SpanBox(EKitRole::Roof, -HalfX, HalfX, -HalfY, HalfY, WallHeight, RoofTop, false));

		// Exterior stairs along the +Y wall, rising toward +X; top step flush with the roof,
		// ending at the inner face of the back wall, next to the parapet gap.
		const double StairEndX = HalfX - Thick;
		const double SideParapetLength = OuterX - 2.0 * Thick;
		const double ParapetGap = Params.bExteriorStairs ? std::min(std::max(StairWidth, 2.0 * Rules.CapsuleRadius + 20.0), SideParapetLength) : 0.0;
		if (Params.bExteriorStairs)
		{
			const int StepCount = std::clamp(static_cast<int>(std::ceil(RoofTop / StairRise - 1e-9)), 1, 1000);
			const double Rise = RoofTop / StepCount;
			for (int Step = 0; Step < StepCount; ++Step)
			{
				const double StepMinX = StairEndX - StairRun * (StepCount - Step);
				Boxes.push_back(SpanBox(EKitRole::Stairs, StepMinX, StepMinX + StairRun, HalfY, HalfY + StairWidth, 0.0, Rise * (Step + 1), false));
			}
		}
		if (Parapet > 0.0)
		{
			const double ParapetTop = RoofTop + Parapet;
			Boxes.push_back(SpanBox(EKitRole::Parapet, -HalfX, -HalfX + Thick, -HalfY, HalfY, RoofTop, ParapetTop, false));
			Boxes.push_back(SpanBox(EKitRole::Parapet, HalfX - Thick, HalfX, -HalfY, HalfY, RoofTop, ParapetTop, false));
			Boxes.push_back(SpanBox(EKitRole::Parapet, -HalfX + Thick, HalfX - Thick, -HalfY, -HalfY + Thick, RoofTop, ParapetTop, false));
			const double PlusYEnd = HalfX - Thick - ParapetGap;
			if (PlusYEnd - (-HalfX + Thick) > 0.5)
			{
				Boxes.push_back(SpanBox(EKitRole::Parapet, -HalfX + Thick, PlusYEnd, HalfY - Thick, HalfY, RoofTop, ParapetTop, false));
			}
		}

		Shape.Label = "Douar house " + std::to_string(Rooms) + (Rooms == 1 ? " room " : " rooms ") + Num(RoomX) + " x " + Num(RoomY)
			+ " cm, walls " + Num(WallHeight) + " cm, door " + Num(DoorWidth) + " x " + Num(DoorHeight) + " cm, roof " + Num(RoofTop) + " cm";
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildAlley(const FAlleyParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Length = Clamped(Params.Length, 200.0, 10000.0, "alley length", Issues);
		const double Width = Clamped(Params.Width, 40.0, 2000.0, "alley width", Issues);
		const double WallHeight = Clamped(Params.WallHeight, 100.0, 3000.0, "wall height", Issues);
		const double Thick = Clamped(Params.WallThickness, 5.0, 200.0, "wall thickness", Issues);
		const double FloorThick = Clamped(Params.FloorThickness, 1.0, 50.0, "floor thickness", Issues);
		const double Rise = Clamped(Params.StepRise, 0.0, 100.0, "step rise", Issues);
		const double Run = Clamped(Params.StepRun, 30.0, 2000.0, "step run", Issues);
		const double DoorWidth = Clamped(Params.DoorWidth, 40.0, 400.0, "door width", Issues);
		double DoorHeight = Clamped(Params.DoorHeight, 100.0, 1000.0, "door height", Issues);
		const double DoorSpacing = Params.DoorSpacing > 0.0 ? Clamped(Params.DoorSpacing, DoorWidth + 60.0, 10000.0, "door spacing", Issues) : 0.0;
		const double SabatThick = Clamped(Params.SabatThickness, 10.0, 300.0, "sabat thickness", Issues);
		const double SabatDepth = Clamped(Params.SabatDepth, 50.0, 1000.0, "sabat depth", Issues);
		const double SabatSpacing = Params.SabatSpacing > 0.0 ? Clamped(Params.SabatSpacing, SabatDepth + 100.0, 10000.0, "sabat spacing", Issues) : 0.0;

		const double MinWidth = 2.0 * Rules.CapsuleRadius + 20.0;
		if (Width < MinWidth)
		{
			Issues.push_back("alley width " + Num(Width) + " cm is narrower than the capsule + 20 cm (" + Num(MinWidth) + " cm)");
		}
		if (Rise > Rules.MaxStepHeight)
		{
			Issues.push_back("step rise " + Num(Rise) + " cm is higher than the max step (" + Num(Rules.MaxStepHeight) + " cm)");
		}
		if (DoorHeight > WallHeight)
		{
			Issues.push_back("door height " + Num(DoorHeight) + " cm is higher than the wall (" + Num(WallHeight) + " cm); clamped");
			DoorHeight = WallHeight;
		}

		const int TerraceCount = Rise > 0.0 ? std::max(1, static_cast<int>(std::ceil(Length / Run - 1e-9))) : 1;
		const double TerraceRun = Rise > 0.0 ? Run : Length;
		const auto TerraceTop = [FloorThick, Rise](int Terrace) { return FloorThick + Rise * Terrace; };
		const auto TerraceAt = [TerraceRun, TerraceCount](double X) { return std::clamp(static_cast<int>(std::floor(X / TerraceRun)), 0, TerraceCount - 1); };
		const double WallTop = TerraceTop(TerraceCount - 1) + WallHeight;
		const double HalfWidth = Width * 0.5;
		const double OuterY = HalfWidth + Thick;
		std::vector<FKitBox>& Boxes = Shape.Boxes;

		// Terraces run under the walls, so the walls stand on solid ground all the way down.
		for (int Terrace = 0; Terrace < TerraceCount; ++Terrace)
		{
			const double MinX = TerraceRun * Terrace;
			const double MaxX = std::min(TerraceRun * (Terrace + 1), Length);
			Boxes.push_back(SpanBox(EKitRole::Floor, MinX, MaxX, -OuterY, OuterY, 0.0, TerraceTop(Terrace), false));
		}

		for (int Side = -1; Side <= 1; Side += 2)
		{
			const double WallMinY = Side < 0 ? -OuterY : HalfWidth;
			const double WallMaxY = Side < 0 ? -HalfWidth : OuterY;
			std::vector<std::pair<double, double>> Gaps;
			if (DoorSpacing > 0.0)
			{
				for (int Door = 0;; ++Door)
				{
					const double DoorCentre = DoorSpacing * (Door + (Side < 0 ? 0.5 : 1.0));
					if (DoorCentre + DoorWidth * 0.5 > Length - Thick)
					{
						break;
					}
					if (DoorCentre - DoorWidth * 0.5 >= Thick)
					{
						Gaps.emplace_back(DoorCentre - DoorWidth * 0.5, DoorCentre + DoorWidth * 0.5);
					}
				}
			}
			for (int Terrace = 0; Terrace < TerraceCount; ++Terrace)
			{
				const double MinX = TerraceRun * Terrace;
				const double MaxX = std::min(TerraceRun * (Terrace + 1), Length);
				const FKitBox Template = SpanBox(EKitRole::Wall, MinX, MaxX, WallMinY, WallMaxY, TerraceTop(Terrace), WallTop, true);
				EmitSegments(MinX, MaxX, Gaps, Template, Boxes);
			}
			for (const std::pair<double, double>& Gap : Gaps)
			{
				const double DoorFloor = TerraceTop(TerraceAt((Gap.first + Gap.second) * 0.5));
				if (DoorFloor + DoorHeight < WallTop)
				{
					Boxes.push_back(SpanBox(EKitRole::Wall, Gap.first, Gap.second, WallMinY, WallMaxY, DoorFloor + DoorHeight, WallTop, false));
				}
				Shape.Doorways.push_back(SpanBox(EKitRole::Wall, Gap.first, Gap.second, WallMinY, WallMaxY, DoorFloor, std::min(DoorFloor + DoorHeight, WallTop), false));
			}
		}

		// Sabats: covered passages between the walls, clear height DoorHeight + 40 above the
		// highest terrace they span.
		if (SabatSpacing > 0.0)
		{
			bool bReported = false;
			for (int Sabat = 0;; ++Sabat)
			{
				const double Centre = SabatSpacing * (Sabat + 0.5);
				const double MinX = Centre - SabatDepth * 0.5;
				const double MaxX = Centre + SabatDepth * 0.5;
				if (MaxX > Length)
				{
					break;
				}
				const double Bottom = TerraceTop(TerraceAt(MaxX - 0.01)) + DoorHeight + 40.0;
				if (Bottom + SabatThick > WallTop)
				{
					if (!bReported)
					{
						Issues.push_back("sabats do not fit under the walls (need " + Num(DoorHeight + 40.0 + SabatThick) + " cm above the floor)");
						bReported = true;
					}
					continue;
				}
				Boxes.push_back(SpanBox(EKitRole::Roof, MinX, MaxX, -HalfWidth, HalfWidth, Bottom, Bottom + SabatThick, false));
			}
		}

		Shape.Label = "Casbah alley " + Num(Length) + " x " + Num(Width) + " cm, "
			+ (Rise > 0.0 ? "steps " + Num(Rise) + " / " + Num(Run) + " cm" : std::string("flat"))
			+ ", walls " + Num(WallHeight) + " cm";
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildRoofBridge(const FRoofBridgeParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Length = Clamped(Params.Length, 50.0, 3000.0, "bridge length", Issues);
		const double Width = Clamped(Params.Width, 20.0, 1000.0, "bridge width", Issues);
		const double Thick = Clamped(Params.Thickness, 2.0, 200.0, "bridge thickness", Issues);
		const double RailHeight = Clamped(Params.RailHeight, 0.0, 200.0, "rail height", Issues);
		const double RailThick = 5.0;
		const double HalfLength = Length * 0.5;
		const double HalfWidth = Width * 0.5;

		Shape.Boxes.push_back(SpanBox(EKitRole::Bridge, -HalfLength, HalfLength, -HalfWidth, HalfWidth, -Thick, 0.0, false));
		double Walkable = Width;
		if (RailHeight > 0.0 && Width > 4.0 * RailThick)
		{
			Shape.Boxes.push_back(SpanBox(EKitRole::Bridge, -HalfLength, HalfLength, -HalfWidth, -HalfWidth + RailThick, 0.0, RailHeight, false));
			Shape.Boxes.push_back(SpanBox(EKitRole::Bridge, -HalfLength, HalfLength, HalfWidth - RailThick, HalfWidth, 0.0, RailHeight, false));
			Walkable = Width - 2.0 * RailThick;
		}
		if (Walkable < 2.0 * Rules.CapsuleRadius)
		{
			Issues.push_back("bridge walkable width " + Num(Walkable) + " cm is narrower than the capsule (" + Num(2.0 * Rules.CapsuleRadius) + " cm)");
		}
		Shape.Label = "Roof bridge " + Num(Length) + " x " + Num(Width) + " cm" + (RailHeight > 0.0 ? ", rails " + Num(RailHeight) + " cm" : std::string());
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildWall(const FWallParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Length = Clamped(Params.Length, 10.0, 10000.0, "wall length", Issues);
		const double Height = Clamped(Params.Height, 10.0, 2000.0, "wall height", Issues);
		const double Thick = Clamped(Params.Thickness, 5.0, 500.0, "wall thickness", Issues);
		Shape.Boxes.push_back(SpanBox(EKitRole::Wall, -Length * 0.5, Length * 0.5, -Thick * 0.5, Thick * 0.5, 0.0, Height, true));
		Shape.Label = "Wall " + Num(Length) + " x " + Num(Height) + " cm (" + CoverClass(Height, Rules) + ")";
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildStairs(const FStairsParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Width = Clamped(Params.Width, 40.0, 5000.0, "stair width", Issues);
		const double Rise = Clamped(Params.StepRise, 1.0, 100.0, "step rise", Issues);
		const double Run = Clamped(Params.StepRun, 10.0, 500.0, "step run", Issues);
		const int StepCount = ClampedInt(Params.StepCount, 1, 200, "step count", Issues);
		const double Landing = Clamped(Params.LandingDepth, 0.0, 5000.0, "landing depth", Issues);
		const bool bTooHigh = Rise > Rules.MaxStepHeight;
		if (bTooHigh)
		{
			Issues.push_back("step rise " + Num(Rise) + " cm is higher than the max step (" + Num(Rules.MaxStepHeight) + " cm)");
		}
		const double HalfWidth = Width * 0.5;
		for (int Step = 0; Step < StepCount; ++Step)
		{
			Shape.Boxes.push_back(SpanBox(EKitRole::Stairs, Run * Step, Run * (Step + 1), -HalfWidth, HalfWidth, 0.0, Rise * (Step + 1), false));
		}
		if (Landing > 0.0)
		{
			const double LandingX = Run * StepCount;
			Shape.Boxes.push_back(SpanBox(EKitRole::Stairs, LandingX, LandingX + Landing, -HalfWidth, HalfWidth, 0.0, Rise * StepCount, false));
		}
		Shape.Label = "Stairs " + std::to_string(StepCount) + " x " + Num(Rise) + " / " + Num(Run) + " cm, top " + Num(Rise * StepCount) + " cm"
			+ (bTooHigh ? " (step too high)" : " (walkable)");
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildRock(const FRockParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double SizeX = Clamped(Params.SizeX, 20.0, 5000.0, "rock size X", Issues);
		const double SizeY = Clamped(Params.SizeY, 20.0, 5000.0, "rock size Y", Issues);
		const double SizeZ = Clamped(Params.SizeZ, 20.0, 5000.0, "rock size Z", Issues);
		const int Chunks = ClampedInt(Params.Chunks, 1, 8, "rock chunks", Issues);
		const double HalfX = SizeX * 0.5;
		const double HalfY = SizeY * 0.5;

		FKitLcg Random;
		Random.State = Params.Seed * 2654435761u + 0x9E3779B9u;
		for (int Chunk = 0; Chunk < Chunks; ++Chunk)
		{
			const bool bMain = Chunk == 0; // the main body is tall and near the centre
			double ExtentX = HalfX * (bMain ? Random.Range(0.6, 0.85) : Random.Range(0.3, 0.6));
			double ExtentY = HalfY * (bMain ? Random.Range(0.6, 0.85) : Random.Range(0.3, 0.6));
			const double Height = SizeZ * (bMain ? 1.0 : Random.Range(0.35, 0.8));
			const double YawDeg = Random.Range(-30.0, 30.0);
			const double Yaw = YawDeg * KitDegToRad;
			const double AbsCos = std::fabs(std::cos(Yaw));
			const double AbsSin = std::fabs(std::sin(Yaw));
			double BoundX = AbsCos * ExtentX + AbsSin * ExtentY;
			double BoundY = AbsSin * ExtentX + AbsCos * ExtentY;
			const double Shrink = std::min({1.0, HalfX / BoundX, HalfY / BoundY});
			ExtentX *= Shrink;
			ExtentY *= Shrink;
			BoundX *= Shrink;
			BoundY *= Shrink;
			const double Spread = bMain ? 0.2 : 1.0;
			const double CentreX = Random.Range(-1.0, 1.0) * std::max(HalfX - BoundX, 0.0) * Spread;
			const double CentreY = Random.Range(-1.0, 1.0) * std::max(HalfY - BoundY, 0.0) * Spread;

			FKitBox Box;
			Box.Role = EKitRole::Rock;
			Box.Center = FVec3{CentreX, CentreY, Height * 0.5};
			Box.Extent = FVec3{ExtentX, ExtentY, Height * 0.5};
			Box.YawDeg = YawDeg;
			Box.bCoverSource = true;
			Shape.Boxes.push_back(Box);
		}
		Shape.Label = "Rock " + Num(SizeX) + " x " + Num(SizeY) + " x " + Num(SizeZ) + " cm, " + std::to_string(Chunks) + " chunks, seed " + std::to_string(Params.Seed);
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildSlope(const FSlopeParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Angle = Clamped(Params.AngleDeg, 1.0, 80.0, "slope angle", Issues);
		const double Length = Clamped(Params.Length, 50.0, 5000.0, "slope length", Issues);
		const double Width = Clamped(Params.Width, 20.0, 5000.0, "slope width", Issues);
		const double Thick = Clamped(Params.Thickness, 2.0, 200.0, "slope thickness", Issues);
		const double Walkable = Clamped(Params.WalkableSlopeDeg, 1.0, 89.0, "walkable slope", Issues);
		const bool bWalkable = Angle <= Walkable;

		// Same maths as the gym ramps: top surface starts at the origin, centre is half the
		// thickness below the top-surface midpoint along the surface normal.
		const double Radians = Angle * KitDegToRad;
		const double MidX = std::cos(Radians) * Length * 0.5;
		const double MidZ = std::sin(Radians) * Length * 0.5;
		FKitBox Box;
		Box.Role = bWalkable ? EKitRole::Slope : EKitRole::SlopeTooSteep;
		Box.PitchDeg = Angle;
		Box.Extent = FVec3{Length * 0.5, Width * 0.5, Thick * 0.5};
		Box.Center = FVec3{MidX + std::sin(Radians) * Thick * 0.5, 0.0, MidZ - std::cos(Radians) * Thick * 0.5};
		Shape.Boxes.push_back(Box);
		Shape.Label = "Slope " + Num(Angle) + " deg, rises " + Num(std::sin(Radians) * Length) + " cm (" + (bWalkable ? "walkable" : "too steep") + ")";
		Finish(Shape, Rules);
		return Shape;
	}

	FKitShape BuildCover(const FCoverParams& Params, const FCoverRules& InRules)
	{
		using namespace KitDetail;
		FKitShape Shape;
		std::vector<std::string>& Issues = Shape.Issues;
		const FCoverRules Rules = SafeRules(InRules, &Issues);
		const double Length = Clamped(Params.Length, 10.0, 5000.0, "cover length", Issues);
		const double Thick = Clamped(Params.Thickness, 5.0, 500.0, "cover thickness", Issues);
		const double HeightLow = Clamped(Params.HeightLow, 10.0, 1000.0, "low cover height", Issues);
		const double HeightHigh = Clamped(Params.HeightHigh, 10.0, 2000.0, "high cover height", Issues);
		const bool bLow = Params.Height == ECoverHeight::Low;
		const double Height = bLow ? HeightLow : HeightHigh;
		if (bLow && (Height < Rules.LowMinHeight || Height > Rules.LowMaxHeight))
		{
			Issues.push_back("low cover height " + Num(Height) + " cm is outside " + Num(Rules.LowMinHeight) + "-" + Num(Rules.LowMaxHeight) + " cm: no low cover points");
		}
		if (!bLow && Height < Rules.HighMinHeight)
		{
			Issues.push_back("high cover height " + Num(Height) + " cm is below " + Num(Rules.HighMinHeight) + " cm: no high cover points");
		}
		Shape.Boxes.push_back(SpanBox(bLow ? EKitRole::LowCover : EKitRole::HighCover, -Length * 0.5, Length * 0.5, -Thick * 0.5, Thick * 0.5, 0.0, Height, true));
		Shape.Label = std::string(bLow ? "Low" : "High") + " cover " + Num(Length) + " x " + Num(Height) + " cm";
		Finish(Shape, Rules);
		return Shape;
	}
} // namespace GoN::Blockout
