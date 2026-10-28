// AAA City Weapons - the player's fire team (leader + 3) and the sides of a fight (AAAWeaponsSubsystem / AAAPoliceOfficer).
#pragma once

#include "CoreMinimal.h"

/** who fights whom: the player's team, the police (only while WANTED), hostile army patrols */
namespace AAATeam
{
	enum : uint8 { Player = 0, Police = 1, Enemy = 2 };
}

/** orders the leader gives (the orders wheel, middle mouse / V / Y / U shortcuts) */
enum class EAAASquadOrder : uint8
{
	Follow,      // اتبعني: keep the formation slot around the leader, fight from near it
	Move,        // تقدّم: go to the point and hold it
	Hold,        // اثبت: stay where you are
	Assault,     // اهجم: push to the point / the marked enemy, bounding from cover to cover
	TakeCover,   // احتمِ: the nearest cover from the threat, stay in it
	Suppress,    // نيران تغطية: bursts on the point (enemies around it keep their heads down)
	FallBack,    // انسحب: run back behind the leader, then follow
};

/** formation slots, in metres from the leader (x forward, y right) */
enum class EAAAFormation : uint8 { Wedge, Line, Column };

struct FAAASquadInfo
{
	static const TCHAR* Name(int32 Idx)
	{
		static const TCHAR* const N[3] = { TEXT("بن مهيدي"), TEXT("ديدوش"), TEXT("زيغود") };
		return N[FMath::Clamp(Idx, 0, 2)];
	}
	static FLinearColor Color(int32 Idx)
	{
		static const FLinearColor C[3] = { FLinearColor(0.80f, 0.22f, 0.14f), FLinearColor(0.22f, 0.45f, 0.85f), FLinearColor(0.30f, 0.66f, 0.26f) };
		return C[FMath::Clamp(Idx, 0, 2)];
	}
	static const TCHAR* OrderName(EAAASquadOrder O)
	{
		switch (O)
		{
		case EAAASquadOrder::Follow: return TEXT("يتبع");
		case EAAASquadOrder::Move: return TEXT("يتقدّم");
		case EAAASquadOrder::Hold: return TEXT("ثابت");
		case EAAASquadOrder::Assault: return TEXT("يهاجم");
		case EAAASquadOrder::TakeCover: return TEXT("يحتمي");
		case EAAASquadOrder::Suppress: return TEXT("نيران تغطية");
		case EAAASquadOrder::FallBack: return TEXT("ينسحب");
		}
		return TEXT("");
	}
	static const TCHAR* FormationName(EAAAFormation F)
	{
		switch (F)
		{
		case EAAAFormation::Wedge: return TEXT("سهم");
		case EAAAFormation::Line: return TEXT("خط");
		case EAAAFormation::Column: return TEXT("طابور");
		}
		return TEXT("");
	}
	/** slot of member Idx in metres (x forward, y right of the leader) */
	static FVector2D Slot(EAAAFormation F, int32 Idx)
	{
		static const FVector2D W[3] = { FVector2D(-3.0, -3.2), FVector2D(-3.0, 3.2), FVector2D(-6.2, 0.0) };
		static const FVector2D L[3] = { FVector2D(-0.8, -3.6), FVector2D(-0.8, 3.6), FVector2D(-0.8, -7.2) };
		static const FVector2D C[3] = { FVector2D(-3.0, 0.4), FVector2D(-6.0, -0.4), FVector2D(-9.0, 0.4) };
		const int32 I = FMath::Clamp(Idx, 0, 2);
		return F == EAAAFormation::Line ? L[I] : (F == EAAAFormation::Column ? C[I] : W[I]);
	}
};
