// Ghosts of November — calendar dates for the historical timeline (pure C++).
#pragma once

#include <cstdio>
#include <string>

namespace GoN
{
	struct FDate
	{
		int Year = 0;
		int Month = 0;
		int Day = 0;

		static bool IsLeapYear(int InYear)
		{
			return (InYear % 4 == 0 && InYear % 100 != 0) || InYear % 400 == 0;
		}

		static int DaysInMonth(int InYear, int InMonth)
		{
			static const int Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
			if (InMonth < 1 || InMonth > 12)
			{
				return 0;
			}
			return (InMonth == 2 && IsLeapYear(InYear)) ? 29 : Days[InMonth - 1];
		}

		bool IsValid() const
		{
			return Year >= 1800 && Year <= 2100 && Month >= 1 && Month <= 12 && Day >= 1 && Day <= DaysInMonth(Year, Month);
		}

		// Days since 1970-01-01 (Howard Hinnant's days_from_civil); negative before 1970.
		long ToDayNumber() const
		{
			const int Y = Year - (Month <= 2 ? 1 : 0);
			const int Era = (Y >= 0 ? Y : Y - 399) / 400;
			const unsigned YearOfEra = static_cast<unsigned>(Y - Era * 400);
			const unsigned MonthIndex = static_cast<unsigned>(Month > 2 ? Month - 3 : Month + 9);
			const unsigned DayOfYear = (153 * MonthIndex + 2) / 5 + static_cast<unsigned>(Day) - 1;
			const unsigned DayOfEra = YearOfEra * 365 + YearOfEra / 4 - YearOfEra / 100 + DayOfYear;
			return static_cast<long>(Era) * 146097L + static_cast<long>(DayOfEra) - 719468L;
		}

		// Accepts strict ISO "YYYY-MM-DD".
		static bool Parse(const std::string& Text, FDate& Out)
		{
			if (Text.size() != 10 || Text[4] != '-' || Text[7] != '-')
			{
				return false;
			}
			int Parts[3] = {0, 0, 0};
			const int Starts[3] = {0, 5, 8};
			const int Lengths[3] = {4, 2, 2};
			for (int PartIndex = 0; PartIndex < 3; ++PartIndex)
			{
				for (int Offset = 0; Offset < Lengths[PartIndex]; ++Offset)
				{
					const char C = Text[static_cast<size_t>(Starts[PartIndex] + Offset)];
					if (C < '0' || C > '9')
					{
						return false;
					}
					Parts[PartIndex] = Parts[PartIndex] * 10 + (C - '0');
				}
			}
			FDate Parsed;
			Parsed.Year = Parts[0];
			Parsed.Month = Parts[1];
			Parsed.Day = Parts[2];
			if (!Parsed.IsValid())
			{
				return false;
			}
			Out = Parsed;
			return true;
		}

		std::string ToString() const
		{
			char Buffer[16];
			std::snprintf(Buffer, sizeof(Buffer), "%04d-%02d-%02d", Year, Month, Day);
			return Buffer;
		}

		friend bool operator==(const FDate& A, const FDate& B) { return A.Year == B.Year && A.Month == B.Month && A.Day == B.Day; }
		friend bool operator!=(const FDate& A, const FDate& B) { return !(A == B); }
		friend bool operator<(const FDate& A, const FDate& B) { return A.ToDayNumber() < B.ToDayNumber(); }
		friend bool operator<=(const FDate& A, const FDate& B) { return A.ToDayNumber() <= B.ToDayNumber(); }
		friend bool operator>(const FDate& A, const FDate& B) { return B < A; }
		friend bool operator>=(const FDate& A, const FDate& B) { return B <= A; }
	};

	// Inclusive date range [From, To].
	struct FDateRange
	{
		FDate From;
		FDate To;

		bool Contains(const FDate& Date) const { return From <= Date && Date <= To; }
		bool Contains(const FDateRange& Other) const { return From <= Other.From && Other.To <= To; }
		bool Overlaps(const FDateRange& Other) const { return From <= Other.To && Other.From <= To; }
		std::string ToString() const { return From.ToString() + ".." + To.ToString(); }
	};
} // namespace GoN
