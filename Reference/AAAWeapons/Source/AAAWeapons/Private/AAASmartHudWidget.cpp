#include "AAASmartHudWidget.h"
#include "AAAWeaponComponent.h"
#include "TrafficAIManager.h"
#include "Rendering/DrawElementTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace
{
	struct FRadialWheelAssets
	{
		bool bLoaded = false;
		UTexture2D* WheelTex = nullptr;
		UTexture2D* FrameTex = nullptr;
		UTexture2D* NeedleTex = nullptr;
		UTexture2D* SlabTex[8] = { nullptr };
		UTexture2D* BinocularsTex = nullptr;

		FSlateBrush WheelBrush;
		FSlateBrush FrameBrush;
		FSlateBrush NeedleBrush;
		FSlateBrush SlabBrush[8];
		FSlateBrush BinocularsBrush;

		float NeedleAngle = 0.f;
		float NeedleVel = 0.f;
		float FrameAngle = 0.f;
		float FrameVel = 0.f;
		float JitterClock = 0.f;
		int32 LastIdx = 0;
		int32 PrevIdx = 0;
		float SlabTransition = 1.f;

		void EnsureLoaded()
		{
			const FString Dir = FPaths::ProjectContentDir() / TEXT("AAACity/UI");
			auto LoadPng = [&](const FString& File, FSlateBrush& OutBrush) -> UTexture2D*
			{
				UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Dir / File);
				if (Tex)
				{
					Tex->AddToRoot();
					OutBrush.SetResourceObject(Tex);
					OutBrush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
					OutBrush.DrawAs = ESlateBrushDrawType::Image;
				}
				return Tex;
			};

			if (!WheelTex || !WheelTex->IsValidLowLevel())
			{
				WheelTex = LoadPng(TEXT("radial_wheel.png"), WheelBrush);
				FrameTex = LoadPng(TEXT("radial_frame.png"), FrameBrush);
				NeedleTex = LoadPng(TEXT("radial_needle.png"), NeedleBrush);
				for (int32 i = 0; i < 8; ++i)
				{
					SlabTex[i] = LoadPng(FString::Printf(TEXT("radial_slab_%d.png"), i), SlabBrush[i]);
				}
			}
			if (!BinocularsTex || !BinocularsTex->IsValidLowLevel())
			{
				BinocularsTex = LoadPng(TEXT("binoculars_overlay.png"), BinocularsBrush);
			}
		}
	};

	static FRadialWheelAssets* GRadialAssetsPtr = nullptr;
	static FRadialWheelAssets& GetRadialAssets()
	{
		if (!GRadialAssetsPtr) GRadialAssetsPtr = new FRadialWheelAssets();
		return *GRadialAssetsPtr;
	}
}
#define GRadialAssets (GetRadialAssets())

void UAAASmartHudWidget::InitWeapon(UAAAWeaponComponent* InWeapon)
{
	Weapon = InWeapon;
	if (Weapon.IsValid())
	{
		SmoothHealth = Weapon->GetHealth();
		GhostHealth = SmoothHealth;
	}
}

void UAAASmartHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Weapon.IsValid()) return;

	const float RealHP = Weapon->GetHealth();
	SmoothHealth = FMath::FInterpTo(SmoothHealth, RealHP, InDeltaTime, 9.f);

	if (RealHP < GhostHealth)
	{
		GhostDelay += InDeltaTime;
		if (GhostDelay > 0.45f)
		{
			GhostHealth = FMath::FInterpTo(GhostHealth, RealHP, InDeltaTime, 4.5f);
		}
	}
	else
	{
		GhostHealth = RealHP;
		GhostDelay = 0.f;
	}

	if (Weapon->HitMarkT > 0.f)
	{
		HitMarkerAlpha = 1.0f;
	}
	else
	{
		HitMarkerAlpha = FMath::FInterpTo(HitMarkerAlpha, 0.f, InDeltaTime, 7.f);
	}

	WantedPulse += InDeltaTime * 7.f;
	if (WantedPulse > 1000.f) WantedPulse = 0.f;

	CrosshairSpread = FMath::FInterpTo(CrosshairSpread, Weapon->Spread, InDeltaTime, 14.f);

	// Update 3D Radial Compass Needle, Selector Frame Spring & Slab Crossfade
	const float Dt = FMath::Clamp(InDeltaTime, 0.001f, 0.05f);
	const int32 CurIdx = FMath::Clamp(Weapon->RadialSelectedIndex, 0, 7);
	if (CurIdx != GRadialAssets.LastIdx)
	{
		GRadialAssets.PrevIdx = GRadialAssets.LastIdx;
		GRadialAssets.LastIdx = CurIdx;
		GRadialAssets.SlabTransition = 0.f;
	}
	GRadialAssets.SlabTransition = FMath::FInterpTo(GRadialAssets.SlabTransition, 1.f, Dt, 16.f);

	const float TargetDeg = (float)(CurIdx * 45);
	const float NeedleDiff = FMath::Fmod(TargetDeg - GRadialAssets.NeedleAngle + 540.f, 360.f) - 180.f;
	GRadialAssets.NeedleVel += (75.f * NeedleDiff - 8.5f * GRadialAssets.NeedleVel) * Dt;
	GRadialAssets.NeedleAngle += GRadialAssets.NeedleVel * Dt;
	GRadialAssets.JitterClock += Dt;

	const float FrameDiff = FMath::Fmod(TargetDeg - GRadialAssets.FrameAngle + 540.f, 360.f) - 180.f;
	GRadialAssets.FrameVel += (220.f * FrameDiff - 22.f * GRadialAssets.FrameVel) * Dt;
	GRadialAssets.FrameAngle += GRadialAssets.FrameVel * Dt;

	TickGhosts(InDeltaTime);
}

int32 UAAASmartHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!Weapon.IsValid() || !Weapon->bHudOn) return LayerId;

	const FVector2f ViewSize = AllottedGeometry.GetLocalSize();
	const float W = ViewSize.X;
	const float H = ViewSize.Y;
	if (W < 100.f || H < 100.f) return LayerId;

	const float Scale = FMath::Clamp(H / 1080.f, 0.7f, 1.6f);
	static FSlateColorBrush SolidBrush(FLinearColor::White);

	auto DrawBox = [&](float X, float Y, float BoxW, float BoxH, const FLinearColor& Tint, int32 InLayerOffset = 0)
	{
		FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry(FVector2f(BoxW, BoxH), FSlateLayoutTransform(FVector2f(X, Y)));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + InLayerOffset, Geo, &SolidBrush, ESlateDrawEffect::None, Tint);
	};

	auto DrawBorder = [&](float X, float Y, float BoxW, float BoxH, const FLinearColor& FillCol, const FLinearColor& BorderCol, float Th, int32 InLayerOffset = 0)
	{
		DrawBox(X, Y, BoxW, BoxH, FillCol, InLayerOffset);
		DrawBox(X, Y, BoxW, Th, BorderCol, InLayerOffset + 1);
		DrawBox(X, Y + BoxH - Th, BoxW, Th, BorderCol, InLayerOffset + 1);
		DrawBox(X, Y, Th, BoxH, BorderCol, InLayerOffset + 1);
		DrawBox(X + BoxW - Th, Y, Th, BoxH, BorderCol, InLayerOffset + 1);
	};

	auto DrawText = [&](const FString& Text, float X, float Y, float FontSize, const FLinearColor& Tint, bool bBold, int32 InLayerOffset = 2)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize * Scale);
		FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry(FVector2f(1000.f, 100.f), FSlateLayoutTransform(FVector2f(X, Y)));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + InLayerOffset, Geo, Text, Font, ESlateDrawEffect::None, Tint);
	};

	auto DrawLine = [&](float X0, float Y0, float X1, float Y1, const FLinearColor& Tint, float Th, int32 InLayerOffset = 2)
	{
		TArray<FVector2f> Points;
		Points.Add(FVector2f(X0, Y0));
		Points.Add(FVector2f(X1, Y1));
		FPaintGeometry Geo = AllottedGeometry.ToPaintGeometry();
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + InLayerOffset, Geo, Points, ESlateDrawEffect::None, Tint, true, Th * Scale);
	};

	// "Ghosts of November" HUD replaces the old cards (1-3) and the wheel (5); crosshair (4) and binoculars (6) stay
	const bool bGhosts = true;
	int32 GhostTop = LayerId;
	if (bGhosts && Weapon->GetBinocularsBlend() < 0.5f) GhostTop = PaintGhosts(AllottedGeometry, OutDrawElements, LayerId);
	if (!bGhosts)
	{
	// =========================================================================================
	// 1. VITAL STATUS CARD (BOTTOM LEFT)
	// =========================================================================================
	{
		const float CardW = 315.f * Scale;
		const float CardH = 68.f * Scale;
		const float PosX = 42.f * Scale;
		const float PosY = H - CardH - 42.f * Scale;

		// Card background & high-tech cyber border
		DrawBorder(PosX, PosY, CardW, CardH, FLinearColor(0.015f, 0.02f, 0.03f, 0.85f), FLinearColor(0.12f, 0.52f, 0.95f, 0.75f), 1.5f * Scale);
		// Left cyan accent pillar
		DrawBox(PosX, PosY, 4.f * Scale, CardH, FLinearColor(0.18f, 0.72f, 1.0f, 0.95f));

		// Vital header
		DrawText(TEXT("OPERATOR VITAL // SYS-ONLINE"), PosX + 16.f * Scale, PosY + 8.f * Scale, 8.5f, FLinearColor(0.4f, 0.75f, 1.0f, 0.85f), true);

		// Numerical HP
		const int32 CurHP = FMath::CeilToInt(Weapon->GetHealth());
		const FString HpStr = FString::Printf(TEXT("%d / 100 HP"), FMath::Max(0, CurHP));
		DrawText(HpStr, PosX + CardW - 82.f * Scale, PosY + 8.f * Scale, 10.5f, FLinearColor::White, true);

		// Health Bar track & fill
		const float BarX = PosX + 16.f * Scale;
		const float BarY = PosY + 34.f * Scale;
		const float BarW = CardW - 32.f * Scale;
		const float BarH = 16.f * Scale;

		// Track
		DrawBox(BarX, BarY, BarW, BarH, FLinearColor(0.06f, 0.08f, 0.11f, 0.95f));

		// Ghost damage bar
		const float GhostFrac = FMath::Clamp(GhostHealth / Weapon->MaxHealth, 0.f, 1.f);
		DrawBox(BarX, BarY, BarW * GhostFrac, BarH, FLinearColor(0.9f, 0.22f, 0.15f, 0.65f));

		// Active Health Fill with dynamic status colors
		const float RealFrac = FMath::Clamp(SmoothHealth / Weapon->MaxHealth, 0.f, 1.f);
		FLinearColor FillCol;
		if (RealFrac > 0.5f)
		{
			FillCol = FLinearColor(0.12f, 0.85f, 0.62f, 0.95f); // Modern emerald
		}
		else if (RealFrac > 0.25f)
		{
			FillCol = FLinearColor(1.0f, 0.65f, 0.12f, 0.95f); // Amber alert
		}
		else
		{
			const float Flash = 0.7f + 0.3f * FMath::Sin(WantedPulse * 1.5f);
			FillCol = FLinearColor(1.0f, 0.15f, 0.20f, Flash); // Pulsing critical red
		}
		DrawBox(BarX, BarY, BarW * RealFrac, BarH, FillCol);

		// Segment ticks across the health bar
		for (int32 i = 1; i < 5; ++i)
		{
			const float TickX = BarX + (BarW * 0.2f * i);
			DrawBox(TickX, BarY, 1.5f * Scale, BarH, FLinearColor(0.02f, 0.02f, 0.02f, 0.6f));
		}
	}

	// =========================================================================================
	// 2. WEAPON & MUNITIONS CARD (BOTTOM RIGHT)
	// =========================================================================================
	if (Weapon->bArmed)
	{
		const float CardW = 320.f * Scale;
		const float CardH = 92.f * Scale;
		const float PosX = W - CardW - 42.f * Scale;
		const float PosY = H - CardH - 42.f * Scale;

		// Card Background & Amber top accent
		DrawBorder(PosX, PosY, CardW, CardH, FLinearColor(0.015f, 0.02f, 0.03f, 0.85f), FLinearColor(0.35f, 0.35f, 0.40f, 0.5f), 1.2f * Scale);
		DrawBox(PosX, PosY, CardW, 3.5f * Scale, FLinearColor(0.98f, 0.68f, 0.18f, 0.95f)); // Amber stripe

		// Weapon Name (Rifle vs LMG vs Pistol vs Kar98k)
		const bool bIsPistol = (Weapon->CurrentWeaponType == EAAAWeaponType::Pistol);
		const bool bIsLMG = (Weapon->CurrentWeaponType == EAAAWeaponType::LMG);
		const bool bIsKar = (Weapon->CurrentWeaponType == EAAAWeaponType::Kar98);
		const FString WeapName = bIsKar ? TEXT("KARABINER 98K // SNIPER RIFLE") : (bIsPistol ? TEXT("COLT 1911 // .45 ACP PISTOL") : (bIsLMG ? TEXT("M249 SAW // SQUAD LMG") : TEXT("M4A1 VLTOR // ASSAULT RIFLE")));
		DrawText(WeapName, PosX + 18.f * Scale, PosY + 12.f * Scale, 11.f, FLinearColor(0.98f, 0.75f, 0.25f), true);

		// Caliber & Fire mode
		const FString CaliberMode = bIsKar ? TEXT("7.92x57mm MAUSER • [ BOLT-ACTION ]") : (bIsPistol ? TEXT(".45 ACP • [ SEMI-AUTO ]") : (bIsLMG ? TEXT("5.56x45mm BELT-FED • [ FULL-AUTO ]") : TEXT("5.56x45mm NATO • [ FULL-AUTO ]")));
		DrawText(CaliberMode, PosX + 18.f * Scale, PosY + 30.f * Scale, 9.f, FLinearColor(0.65f, 0.68f, 0.75f), false);

		// Large Digital Ammo Display
		const FString AmmoStr = FString::Printf(TEXT("%02d"), Weapon->Ammo);
		DrawText(AmmoStr, PosX + 18.f * Scale, PosY + 44.f * Scale, 26.f, Weapon->Ammo > 0 ? FLinearColor::White : FLinearColor(1.0f, 0.25f, 0.2f), true);

		const FString ReserveStr = Weapon->bInfiniteReserve ? TEXT("/ ∞") : FString::Printf(TEXT("/ %d"), Weapon->ReserveAmmo);
		DrawText(ReserveStr, PosX + 75.f * Scale, PosY + 58.f * Scale, 13.f, FLinearColor(0.65f, 0.68f, 0.75f), false);

		// Segmented Ammo Gauge (mini bullet pips)
		const float PipAreaX = PosX + 130.f * Scale;
		const float PipAreaY = PosY + 52.f * Scale;
		const int32 TotalPips = bIsKar ? 5 : (bIsLMG ? 25 : (bIsPistol ? 8 : 15));
		const float PipW = 4.5f * Scale;
		const float PipH = 14.f * Scale;
		const float PipGap = 2.0f * Scale;
		const float AmmoRatio = (float)Weapon->Ammo / (float)Weapon->MagazineSize;
		const int32 LitPips = FMath::CeilToInt(AmmoRatio * TotalPips);

		for (int32 i = 0; i < TotalPips; ++i)
		{
			const float Px = PipAreaX + i * (PipW + PipGap);
			const bool bLit = i < LitPips;
			const FLinearColor PipCol = bLit ? (AmmoRatio > 0.25f ? FLinearColor(0.98f, 0.75f, 0.25f, 0.9f) : FLinearColor(1.0f, 0.2f, 0.2f, 0.95f)) : FLinearColor(0.12f, 0.14f, 0.16f, 0.5f);
			DrawBox(Px, PipAreaY, PipW, PipH, PipCol);
		}

		// Reloading notification overlay
		if (Weapon->ReloadT >= 0.f)
		{
			const float Prog = FMath::Clamp(Weapon->ReloadT / 2.2f, 0.f, 1.f);
			DrawBox(PosX, PosY + CardH - 6.f * Scale, CardW * Prog, 6.f * Scale, FLinearColor(0.15f, 0.75f, 1.0f, 0.95f));
			DrawText(TEXT("RELOADING..."), PosX + CardW - 110.f * Scale, PosY + 12.f * Scale, 10.f, FLinearColor(0.2f, 0.85f, 1.0f), true);
		}

		// Grenade Badge (Top of weapon card)
		const float GrenadeBadgeW = 180.f * Scale;
		const float GrenadeBadgeH = 28.f * Scale;
		const float GrenadeX = W - GrenadeBadgeW - 42.f * Scale;
		const float GrenadeY = PosY - GrenadeBadgeH - 8.f * Scale;

		DrawBorder(GrenadeX, GrenadeY, GrenadeBadgeW, GrenadeBadgeH, FLinearColor(0.015f, 0.02f, 0.03f, 0.8f), FLinearColor(0.2f, 0.65f, 0.4f, 0.7f), 1.0f * Scale);
		const FString GrenadeText = FString::Printf(TEXT("[G] FRAG GRENADE  x%d"), Weapon->Grenades);
		DrawText(GrenadeText, GrenadeX + 12.f * Scale, GrenadeY + 7.f * Scale, 10.f, Weapon->Grenades > 0 ? FLinearColor(0.35f, 0.95f, 0.55f) : FLinearColor(0.5f, 0.5f, 0.5f), true);

		// Grenade cooking bar
		if (Weapon->bCookingGrenade)
		{
			const float Rem = FMath::Max(0.f, 3.2f - Weapon->CookTimer);
			const float CookFrac = FMath::Clamp(Weapon->CookTimer / 3.2f, 0.f, 1.f);
			DrawBox(GrenadeX, GrenadeY + GrenadeBadgeH - 4.f * Scale, GrenadeBadgeW * CookFrac, 4.f * Scale, FLinearColor(1.0f, 0.15f, 0.15f, 0.95f));
		}
	}

	// =========================================================================================
	// 3. TACTICAL POLICE PURSUIT / WANTED GAUGE (TOP LEFT)
	// =========================================================================================
	if (ATrafficAIManager* Traffic = Weapon->Traffic())
	{
		const int32 Wanted = Traffic->GetWantedStars();
		if (Wanted > 0)
		{
			const float GaugeW = 310.f * Scale;
			const float GaugeH = 54.f * Scale;
			const float PosX = 42.f * Scale;
			const float PosY = 36.f * Scale;

			// Siren flash alternates red and blue
			const bool bSirenRed = FMath::Sin(WantedPulse) > 0.f;
			const FLinearColor BeaconCol = bSirenRed ? FLinearColor(1.0f, 0.1f, 0.1f, 0.9f) : FLinearColor(0.1f, 0.4f, 1.0f, 0.9f);

			DrawBorder(PosX, PosY, GaugeW, GaugeH, FLinearColor(0.015f, 0.02f, 0.03f, 0.88f), BeaconCol, 1.5f * Scale);
			DrawBox(PosX, PosY, 5.f * Scale, GaugeH, BeaconCol);

			DrawText(FString::Printf(TEXT("POLICE PURSUIT // WANTED LEVEL %d"), Wanted), PosX + 16.f * Scale, PosY + 8.f * Scale, 10.f, FLinearColor(1.0f, 0.85f, 0.3f), true);

			// 5-Star Wanted Rating
			for (int32 s = 0; s < 5; ++s)
			{
				const float StarX = PosX + 16.f * Scale + s * 22.f * Scale;
				const float StarY = PosY + 28.f * Scale;
				const bool bFilled = s < Wanted;
				const FLinearColor StarCol = bFilled ? (bSirenRed ? FLinearColor(1.0f, 0.2f, 0.2f) : FLinearColor(1.0f, 0.85f, 0.2f)) : FLinearColor(0.2f, 0.22f, 0.25f, 0.6f);
				DrawText(TEXT("★"), StarX, StarY, 13.f, StarCol, true);
			}
		}
	}

	}	// !bGhosts

	// =========================================================================================
	// 4. SMART DYNAMIC RETICLE & CROSSHAIR (CENTER)
	// =========================================================================================
	if (Weapon->bArmed && !Weapon->bDead)
	{
		const float Cx = W * 0.5f;
		const float Cy = H * 0.5f;

		if (Weapon->bAiming)
		{
			// ADS Holographic Sight: Micro-dot + outer precision brackets
			const float DotSize = 3.f * Scale;
			DrawBox(Cx - DotSize * 0.5f, Cy - DotSize * 0.5f, DotSize, DotSize, FLinearColor(0.15f, 0.85f, 1.0f, 0.95f));

			const float OuterL = 6.f * Scale;
			const float OuterD = 18.f * Scale;
			const FLinearColor AdsCol(0.15f, 0.85f, 1.0f, 0.5f);
			DrawLine(Cx - OuterD - OuterL, Cy, Cx - OuterD, Cy, AdsCol, 1.2f);
			DrawLine(Cx + OuterD, Cy, Cx + OuterD + OuterL, Cy, AdsCol, 1.2f);
			DrawLine(Cx, Cy - OuterD - OuterL, Cx, Cy - OuterD, AdsCol, 1.2f);
			DrawLine(Cx, Cy + OuterD, Cx, Cy + OuterD + OuterL, AdsCol, 1.2f);
		}
		else
		{
			// Dynamic Tactical Hip-Fire Crosshair with spread expansion
			const float Gap = FMath::Max(5.f * Scale, 8.f * Scale + CrosshairSpread * 14.f * Scale);
			const float LineLen = 10.f * Scale;
			const float LineTh = 2.0f * Scale;
			const FLinearColor ReticleCol(1.0f, 1.0f, 1.0f, 0.85f);

			// Center Pip
			DrawBox(Cx - 1.2f * Scale, Cy - 1.2f * Scale, 2.4f * Scale, 2.4f * Scale, ReticleCol);

			// 4 Prongs
			DrawLine(Cx - Gap - LineLen, Cy, Cx - Gap, Cy, ReticleCol, LineTh);
			DrawLine(Cx + Gap, Cy, Cx + Gap + LineLen, Cy, ReticleCol, LineTh);
			DrawLine(Cx, Cy - Gap - LineLen, Cx, Cy - Gap, ReticleCol, LineTh);
			DrawLine(Cx, Cy + Gap, Cx, Cy + Gap + LineLen, ReticleCol, LineTh);
		}

		// Hit Marker (Red 'X' chevrons on hit confirmation)
		if (HitMarkerAlpha > 0.01f)
		{
			const float HLen = 7.f * Scale;
			const float HGap = 4.f * Scale;
			const FLinearColor HitCol(1.0f, 0.15f, 0.15f, HitMarkerAlpha);
			DrawLine(Cx - HGap - HLen, Cy - HGap - HLen, Cx - HGap, Cy - HGap, HitCol, 2.2f);
			DrawLine(Cx + HGap, Cy - HGap, Cx + HGap + HLen, Cy - HGap - HLen, HitCol, 2.2f);
			DrawLine(Cx - HGap - HLen, Cy + HGap + HLen, Cx - HGap, Cy + HGap, HitCol, 2.2f);
			DrawLine(Cx + HGap, Cy + HGap, Cx + HGap + HLen, Cy + HGap + HLen, HitCol, 2.2f);
		}
	}

	// =========================================================================================
	// 5. 3D ILLUSTRATED COMPASS RADIAL MENU (TAB / Q HOLD)
	// =========================================================================================
	const float RadAlpha = Weapon->RadialMenuAlpha;
	if (RadAlpha > 0.01f && !bGhosts)
	{
		GRadialAssets.EnsureLoaded();

		const float CenterX = W * 0.5f;
		const float CenterY = H * 0.46f;
		const int32 SelectedIdx = FMath::Clamp(Weapon->RadialSelectedIndex, 0, 7);
		const float SmoothPop = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(RadAlpha, 0.f, 1.f), 2.5f);

		// 1. Subtle warm vignette backdrop
		DrawBox(0.f, 0.f, W, H, FLinearColor(0.025f, 0.014f, 0.006f, 0.50f * RadAlpha), 0);

		// Compact, sleek wheel size (42% of screen height)
		const float WheelSize = FMath::Min(W, H) * (0.36f + 0.06f * SmoothPop);
		const float WheelX = CenterX - WheelSize * 0.5f;
		const float WheelY = CenterY - WheelSize * 0.5f;
		const FLinearColor ImgTint(1.f, 1.f, 1.f, RadAlpha);

		if (GRadialAssets.WheelTex)
		{
			// 2. Base Illustrated Wheel + Compass Rose + Glass
			FPaintGeometry WheelGeo = AllottedGeometry.ToPaintGeometry(
				FVector2f(WheelSize, WheelSize),
				FSlateLayoutTransform(FVector2f(WheelX, WheelY)));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, WheelGeo, &GRadialAssets.WheelBrush, ESlateDrawEffect::None, ImgTint);

			// 3. Smoothly Crossfaded & Rising 3D Extruded Sector Slab
			const float Trans = FMath::Clamp(GRadialAssets.SlabTransition, 0.f, 1.f);
			if (Trans < 0.99f && GRadialAssets.PrevIdx != SelectedIdx && GRadialAssets.SlabTex[GRadialAssets.PrevIdx])
			{
				const FLinearColor PrevTint(1.f, 1.f, 1.f, RadAlpha * (1.f - Trans));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, WheelGeo, &GRadialAssets.SlabBrush[GRadialAssets.PrevIdx], ESlateDrawEffect::None, PrevTint);
			}
			if (GRadialAssets.SlabTex[SelectedIdx])
			{
				const float RiseY = (1.f - Trans) * 8.f * (WheelSize / 1200.f);
				FPaintGeometry SlabGeo = AllottedGeometry.ToPaintGeometry(
					FVector2f(WheelSize, WheelSize),
					FSlateLayoutTransform(FVector2f(WheelX, WheelY + RiseY)));
				const FLinearColor CurTint(1.f, 1.f, 1.f, RadAlpha * FMath::Clamp(0.25f + 0.75f * Trans, 0.f, 1.f));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, SlabGeo, &GRadialAssets.SlabBrush[SelectedIdx], ESlateDrawEffect::None, CurTint);
			}

			// 4. Spring-Driven Rotating Golden Glowing Selector Frame
			if (GRadialAssets.FrameTex)
			{
				const float LiftOffset = -15.f * (WheelSize / 1200.f);
				FPaintGeometry FrameGeo = AllottedGeometry.ToPaintGeometry(
					FVector2f(WheelSize, WheelSize),
					FSlateLayoutTransform(FVector2f(WheelX, WheelY + LiftOffset)));
				const float FrameRad = FMath::DegreesToRadians(GRadialAssets.FrameAngle);
				FSlateDrawElement::MakeRotatedBox(
					OutDrawElements,
					LayerId + 4,
					FrameGeo,
					&GRadialAssets.FrameBrush,
					ESlateDrawEffect::None,
					FrameRad,
					FVector2f(WheelSize * 0.5f, WheelSize * 0.5f),
					FSlateDrawElement::RelativeToElement,
					ImgTint);
			}

			// 5. Physics-Driven Magnetic Compass Needle in the Center
			if (GRadialAssets.NeedleTex)
			{
				const float Jitter = FMath::Sin(GRadialAssets.JitterClock * 3.2f) * 0.28f + FMath::Sin(GRadialAssets.JitterClock * 7.3f) * 0.14f;
				const float NeedleRad = FMath::DegreesToRadians(GRadialAssets.NeedleAngle + Jitter);
				FSlateDrawElement::MakeRotatedBox(
					OutDrawElements,
					LayerId + 5,
					WheelGeo,
					&GRadialAssets.NeedleBrush,
					ESlateDrawEffect::None,
					NeedleRad,
					FVector2f(WheelSize * 0.5f, WheelSize * 0.5f),
					FSlateDrawElement::RelativeToElement,
					ImgTint);
			}
		}

		// 6. Sleek Compact Status Pill Directly Below the Wheel
		{
			struct FSectorLabel
			{
				FString Title;
				FString ActionDesc;
			};

			FSectorLabel Labels[8] = {
				{ TEXT("[1] SURVEILLANCE"),     TEXT("TACTICAL JUMELLE (BINOCULARS) & RANGEFINDER [B]") },
				{ TEXT("[2] WEAPONS ARSENAL"),  TEXT("CYCLE PRIMARY WEAPON (M4A1 / M249 / BERETTA M9)") },
				{ TEXT("[3] HEAVY EQUIPMENT"),  TEXT("EQUIP SQUAD LMG (M249 SAW 100 RDS)") },
				{ TEXT("[4] TACTICAL ORDERS"),  TEXT("DEPLOY TRAINING TARGETS [T]") },
				{ TEXT("[5] STEALTH MODE"),     TEXT("HOLSTER WEAPON FOR FAST SPRINT") },
				{ TEXT("[6] FIELD MEDICAL"),    TEXT("USE MEDKIT (+50 HP INSTANT HEAL)") },
				{ TEXT("[7] COMBAT MISSIONS"),  TEXT("COOK & THROW M67 FRAG GRENADE [G]") },
				{ TEXT("[8] TACTICAL PISTOL"),  TEXT("EQUIP BERETTA M9 9MM PISTOL [3]") }
			};

			const float BannerW = 340.f * Scale;
			const float BannerH = 48.f * Scale;
			const float BannerX = CenterX - BannerW * 0.5f;
			const float BannerY = WheelY + WheelSize + 10.f * Scale;

			DrawBorder(BannerX, BannerY, BannerW, BannerH,
				FLinearColor(0.05f, 0.03f, 0.012f, 0.90f * RadAlpha),
				FLinearColor(1.0f, 0.71f, 0.15f, 0.92f * RadAlpha),
				1.6f * Scale, 5);

			DrawText(Labels[SelectedIdx].Title, BannerX + 14.f * Scale, BannerY + 6.f * Scale, 10.5f, FLinearColor(1.0f, 0.78f, 0.25f, RadAlpha), true, 6);
			DrawText(Labels[SelectedIdx].ActionDesc, BannerX + 14.f * Scale, BannerY + 26.f * Scale, 8.5f, FLinearColor(0.95f, 0.90f, 0.78f, RadAlpha), false, 6);
		}
	}

	// =========================================================================================
	// 6. TACTICAL JUMELLE / BINOCULARS OPTICAL HUD OVERLAY
	// =========================================================================================
	const float BinoBlend = Weapon->GetBinocularsBlend();
	if (BinoBlend > 0.001f)
	{
		const int32 BinoLayer = LayerId + 20;
		const float BinoAlpha = FMath::Clamp(BinoBlend, 0.f, 1.f);
		const float Cx = W * 0.5f;
		const float Cy = H * 0.5f;
		const float ApertureR = H * 0.44f;
		const float Sep = ApertureR * 0.72f;
		const float LeftCx = Cx - Sep * 0.5f;
		const float RightCx = Cx + Sep * 0.5f;

		// 1. Photorealistic Binoculars Frame & Matte
		GRadialAssets.EnsureLoaded();
		if (GRadialAssets.BinocularsTex)
		{
			// Scale binoculars texture to cover screen
			const float ScaleFactor = FMath::Max(W / 1024.f, H / 599.f);
			const float DrawW = 1024.f * ScaleFactor;
			const float DrawH = 599.f * ScaleFactor;
			const float DrawX = (W - DrawW) * 0.5f;
			const float DrawY = (H - DrawH) * 0.5f;

			// Black borders outside the texture bounds if any (pillarbox / letterbox)
			if (DrawX > 0.f)
			{
				DrawBox(0.f, 0.f, DrawX + 1.f, H, FLinearColor(0.005f, 0.007f, 0.01f, BinoAlpha), BinoLayer);
				DrawBox(DrawX + DrawW - 1.f, 0.f, W - (DrawX + DrawW) + 2.f, H, FLinearColor(0.005f, 0.007f, 0.01f, BinoAlpha), BinoLayer);
			}
			if (DrawY > 0.f)
			{
				DrawBox(0.f, 0.f, W, DrawY + 1.f, FLinearColor(0.005f, 0.007f, 0.01f, BinoAlpha), BinoLayer);
				DrawBox(0.f, DrawY + DrawH - 1.f, W, H - (DrawY + DrawH) + 2.f, FLinearColor(0.005f, 0.007f, 0.01f, BinoAlpha), BinoLayer);
			}

			FPaintGeometry BinoGeo = AllottedGeometry.ToPaintGeometry(
				FVector2f(DrawW, DrawH),
				FSlateLayoutTransform(FVector2f(DrawX, DrawY)));

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				BinoLayer + 1,
				BinoGeo,
				&GRadialAssets.BinocularsBrush,
				ESlateDrawEffect::None,
				FLinearColor(1.f, 1.f, 1.f, BinoAlpha));
		}
		else
		{
			// Procedural fallback
			const FLinearColor MatteCol(0.01f, 0.015f, 0.02f, 0.96f * BinoAlpha);
			const float OuterLeft = LeftCx - ApertureR;
			if (OuterLeft > 0.f) DrawBox(0.f, 0.f, OuterLeft + 1.f, H, MatteCol, BinoLayer);
			const float OuterRight = RightCx + ApertureR;
			if (OuterRight < W) DrawBox(OuterRight - 1.f, 0.f, W - OuterRight + 2.f, H, MatteCol, BinoLayer);
			constexpr int32 NumSlices = 64;
			const float SliceSpan = OuterRight - OuterLeft;
			const float SliceW = SliceSpan / (float)NumSlices;
			const float R2 = ApertureR * ApertureR;
			for (int32 s = 0; s < NumSlices; ++s)
			{
				const float X0 = OuterLeft + s * SliceW;
				const float Xmid = X0 + SliceW * 0.5f;
				float Y_open = 0.f;
				const float DxL = FMath::Abs(Xmid - LeftCx);
				if (DxL < ApertureR) Y_open = FMath::Max(Y_open, FMath::Sqrt(R2 - DxL * DxL));
				const float DxR = FMath::Abs(Xmid - RightCx);
				if (DxR < ApertureR) Y_open = FMath::Max(Y_open, FMath::Sqrt(R2 - DxR * DxR));
				const float TopH = Cy - Y_open;
				if (TopH > 0.f) DrawBox(X0, 0.f, SliceW + 1.5f, TopH, MatteCol, BinoLayer);
				const float BottomY = Cy + Y_open;
				if (BottomY < H) DrawBox(X0, BottomY, SliceW + 1.5f, H - BottomY, MatteCol, BinoLayer);
			}
			const FLinearColor RimCol(0.12f, 0.92f, 0.55f, 0.45f * BinoAlpha);
			DrawBox(Cx - 1.f * Scale, Cy - 30.f * Scale, 2.f * Scale, 60.f * Scale, RimCol, BinoLayer);
		}

		// 2. Tactical Green/Emerald Reticle & Milliradian Graduations
		const FLinearColor RetCol(0.15f, 0.95f, 0.52f, 0.88f * BinoAlpha);
		const FLinearColor MutedCol(0.15f, 0.95f, 0.52f, 0.40f * BinoAlpha);

		// Center rangefinder crosshair & box
		const float CrossSize = 140.f * Scale;
		DrawBox(Cx - CrossSize, Cy, CrossSize * 0.75f, 1.2f * Scale, RetCol, BinoLayer + 1);
		DrawBox(Cx + CrossSize * 0.25f, Cy, CrossSize * 0.75f, 1.2f * Scale, RetCol, BinoLayer + 1);
		DrawBox(Cx, Cy - CrossSize, 1.2f * Scale, CrossSize * 0.75f, RetCol, BinoLayer + 1);
		DrawBox(Cx, Cy + CrossSize * 0.25f, 1.2f * Scale, CrossSize * 0.75f, RetCol, BinoLayer + 1);

		// Milliradian ticks along axes
		for (int32 m = -5; m <= 5; ++m)
		{
			if (m == 0) continue;
			const float Tx = Cx + m * 24.f * Scale;
			const float TickLen = (m % 5 == 0) ? 10.f * Scale : 5.f * Scale;
			DrawBox(Tx, Cy - TickLen * 0.5f, 1.0f * Scale, TickLen, MutedCol, BinoLayer + 1);

			const float Ty = Cy + m * 24.f * Scale;
			DrawBox(Cx - TickLen * 0.5f, Ty, TickLen, 1.0f * Scale, MutedCol, BinoLayer + 1);
		}

		// Center target brackets [ ]
		const float Bkt = 18.f * Scale;
		const float BktTh = 1.6f * Scale;
		// [
		DrawBox(Cx - Bkt - 8.f * Scale, Cy - Bkt, BktTh, Bkt * 2.f, RetCol, BinoLayer + 1);
		DrawBox(Cx - Bkt - 8.f * Scale, Cy - Bkt, 6.f * Scale, BktTh, RetCol, BinoLayer + 1);
		DrawBox(Cx - Bkt - 8.f * Scale, Cy + Bkt - BktTh, 6.f * Scale, BktTh, RetCol, BinoLayer + 1);
		// ]
		DrawBox(Cx + Bkt + 8.f * Scale - BktTh, Cy - Bkt, BktTh, Bkt * 2.f, RetCol, BinoLayer + 1);
		DrawBox(Cx + Bkt + 8.f * Scale - 6.f * Scale, Cy - Bkt, 6.f * Scale, BktTh, RetCol, BinoLayer + 1);
		DrawBox(Cx + Bkt + 8.f * Scale - 6.f * Scale, Cy + Bkt - BktTh, 6.f * Scale, BktTh, RetCol, BinoLayer + 1);

		// 3. Digital Compass Azimuth Tape (Top Center)
		APlayerController* PC = Weapon->ControllingPC();
		const float Yaw = PC ? FRotator::NormalizeAxis(PC->GetControlRotation().Yaw) : 0.f;
		const float Pitch = PC ? FRotator::NormalizeAxis(PC->GetControlRotation().Pitch) : 0.f;
		float NormYaw = Yaw;
		if (NormYaw < 0.f) NormYaw += 360.f;

		const float TapeY = Cy - ApertureR + 32.f * Scale;
		const float TapeW = 320.f * Scale;
		DrawBox(Cx - TapeW * 0.5f, TapeY, TapeW, 1.2f * Scale, MutedCol, BinoLayer + 1);
		DrawBox(Cx - 1.f * Scale, TapeY - 6.f * Scale, 2.f * Scale, 12.f * Scale, RetCol, BinoLayer + 2);

		for (int32 deg = -30; deg <= 30; deg += 5)
		{
			const float TickX = Cx + deg * 4.5f * Scale;
			const float TH = (deg % 10 == 0) ? 8.f * Scale : 4.f * Scale;
			DrawBox(TickX, TapeY - TH, 1.0f * Scale, TH, MutedCol, BinoLayer + 1);
		}

		FString HeadingName = TEXT("N");
		if (NormYaw >= 22.5f && NormYaw < 67.5f) HeadingName = TEXT("NE");
		else if (NormYaw >= 67.5f && NormYaw < 112.5f) HeadingName = TEXT("E");
		else if (NormYaw >= 112.5f && NormYaw < 157.5f) HeadingName = TEXT("SE");
		else if (NormYaw >= 157.5f && NormYaw < 202.5f) HeadingName = TEXT("S");
		else if (NormYaw >= 202.5f && NormYaw < 247.5f) HeadingName = TEXT("SW");
		else if (NormYaw >= 247.5f && NormYaw < 292.5f) HeadingName = TEXT("W");
		else if (NormYaw >= 292.5f && NormYaw < 337.5f) HeadingName = TEXT("NW");

		const FString CompBadge = FString::Printf(TEXT("[ %03d° %s ]"), FMath::RoundToInt(NormYaw), *HeadingName);
		DrawText(CompBadge, Cx - 36.f * Scale, TapeY - 24.f * Scale, 10.5f, RetCol, true, BinoLayer + 2);

		// 4. Center Rangefinder Box (Dynamic Distance to Target)
		const float Dist = Weapon->GetRangefinderDist();
		FString RngStr;
		if (Dist > 0.1f)
		{
			RngStr = FString::Printf(TEXT("DIST: %.1f M"), Dist);
		}
		else
		{
			RngStr = TEXT("DIST: >2000 M");
		}
		const float RngBoxW = 160.f * Scale;
		const float RngBoxH = 26.f * Scale;
		const float RngBoxX = Cx - RngBoxW * 0.5f;
		const float RngBoxY = Cy + Bkt + 22.f * Scale;
		DrawBorder(RngBoxX, RngBoxY, RngBoxW, RngBoxH, FLinearColor(0.02f, 0.05f, 0.03f, 0.85f * BinoAlpha), RetCol, 1.2f * Scale, BinoLayer + 1);
		DrawText(RngStr, RngBoxX + 16.f * Scale, RngBoxY + 5.f * Scale, 10.f, RetCol, true, BinoLayer + 2);

		// 5. Magnification & FOV Badge (Bottom-Right of Optics)
		const float Zoom = Weapon->GetBinocularsZoom();
		const float CurFov = 90.f / FMath::Max(Zoom, 1.0f);
		const float MagBoxW = 180.f * Scale;
		const float MagBoxH = 58.f * Scale;
		const float MagBoxX = RightCx - 40.f * Scale;
		const float MagBoxY = Cy + ApertureR - 85.f * Scale;

		DrawBorder(MagBoxX, MagBoxY, MagBoxW, MagBoxH, FLinearColor(0.02f, 0.05f, 0.03f, 0.85f * BinoAlpha), RetCol, 1.2f * Scale, BinoLayer + 1);
		DrawText(FString::Printf(TEXT("MAG: %.1fX"), Zoom), MagBoxX + 12.f * Scale, MagBoxY + 6.f * Scale, 10.5f, RetCol, true, BinoLayer + 2);
		DrawText(FString::Printf(TEXT("FOV: %.1f°"), CurFov), MagBoxX + 12.f * Scale, MagBoxY + 24.f * Scale, 9.f, MutedCol, false, BinoLayer + 2);
		DrawText(TEXT("[SCROLL / RMB: ZOOM]"), MagBoxX + 12.f * Scale, MagBoxY + 40.f * Scale, 7.5f, FLinearColor(1.0f, 0.75f, 0.2f, BinoAlpha), false, BinoLayer + 2);

		// 6. Tactical Status & Elevation (Bottom-Left of Optics)
		const float StatusBoxW = 190.f * Scale;
		const float StatusBoxH = 58.f * Scale;
		const float StatusBoxX = LeftCx - StatusBoxW + 40.f * Scale;
		const float StatusBoxY = Cy + ApertureR - 85.f * Scale;

		DrawBorder(StatusBoxX, StatusBoxY, StatusBoxW, StatusBoxH, FLinearColor(0.02f, 0.05f, 0.03f, 0.85f * BinoAlpha), RetCol, 1.2f * Scale, BinoLayer + 1);
		DrawText(TEXT("JUMELLE TACTIQUE // V4"), StatusBoxX + 12.f * Scale, StatusBoxY + 6.f * Scale, 9.5f, RetCol, true, BinoLayer + 2);
		DrawText(FString::Printf(TEXT("ELV: %+05.1f°"), Pitch), StatusBoxX + 12.f * Scale, StatusBoxY + 24.f * Scale, 9.f, MutedCol, false, BinoLayer + 2);
		DrawText(TEXT("STABILIZER: ACTIVE [●]"), StatusBoxX + 12.f * Scale, StatusBoxY + 40.f * Scale, 7.5f, RetCol, false, BinoLayer + 2);
	}

	return FMath::Max(LayerId + 8, GhostTop);
}
