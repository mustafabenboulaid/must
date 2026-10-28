// "Ghosts of November" HUD (the user's HTML mock-up "أشباح نوفمبر — واجهة اللعب", applied 1:1).
// The page lays everything out on a 1670 x 941 design canvas that is scaled to fit the screen and centred; this
// draws the same canvas with the same coordinates, the page's own art (Content/AAACity/UI/GhostsNovember/*.png,
// converted from the page's embedded images) and its fonts (Amiri, Courier Prime; Arabic is shaped right-to-left).
// Live data: health, stamina, weapon / ammo / magazines, compass heading, detection (wanted), the 8-way wheel and
// its two sub wheels (weapons / equipment, like the page's ring.kind 'w' / 'e').
// Anti-aliasing: every picture is loaded with a full mip chain (trilinear) and a 4 px transparent border, so it stays
// smooth when it is drawn smaller than its file and its edges stay soft when it is rotated.
#include "AAASmartHudWidget.h"
#include "AAAWeaponComponent.h"
#include "TrafficAIManager.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "AAAPoliceOfficer.h"
#include "AAAWeaponsSubsystem.h"
#include "AAAMissionSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Misc/Paths.h"

namespace
{
	const float DW = 1670.f, DH = 941.f;            // design canvas
	const FVector2f RingC(834.5f, 508.f);           // wheel centre
	const float RingR = 248.f;                      // frames' distance from the centre (page: 268; closer together)
	const float PtrR = 354.f;                       // the orange pointer (page: 374)
	const int32 TexPad = 4;                         // transparent border added around every picture (soft edges)

	struct FGhostTex
	{
		UTexture2D* Tex = nullptr;
		FSlateBrush Brush;
		FVector2f Size = FVector2f(1.f, 1.f);       // size of the picture itself (without the border)
		float Pad = 0.f;
		bool IsOk() const { return Tex != nullptr; }
	};

	/** a PNG as a transient texture with all its mips (alpha weighted box filter), trilinear, never streamed */
	UTexture2D* LoadMipped(const FString& Path, int32& OutW, int32& OutH)
	{
		OutW = OutH = 0;
		FImage Src;
		if (!FImageUtils::LoadImage(*Path, Src)) return nullptr;
		FImage Img;
		Src.CopyTo(Img, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const int32 W = Img.SizeX, H = Img.SizeY;
		if (W <= 0 || H <= 0) return nullptr;
		OutW = W; OutH = H;
		UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!T) return nullptr;
		T->SRGB = true;
		T->Filter = TF_Trilinear;
		T->LODGroup = TEXTUREGROUP_UI;
		T->NeverStream = true;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		FTexturePlatformData* PD = T->GetPlatformData();
		if (!PD || PD->Mips.Num() == 0) return nullptr;
		TArray<uint8> Cur;
		Cur.SetNumUninitialized(W * H * 4);
		FMemory::Memcpy(Cur.GetData(), Img.RawData.GetData(), (SIZE_T)W * H * 4);
		{
			FTexture2DMipMap& M0 = PD->Mips[0];
			void* D = M0.BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(D, Cur.GetData(), Cur.Num());
			M0.BulkData.Unlock();
		}
		int32 Cw = W, Ch = H;
		while (Cw > 1 || Ch > 1)
		{
			const int32 Nw = FMath::Max(1, Cw / 2), Nh = FMath::Max(1, Ch / 2);
			TArray<uint8> Next;
			Next.SetNumZeroed(Nw * Nh * 4);
			for (int32 Y = 0; Y < Nh; ++Y)
				for (int32 X = 0; X < Nw; ++X)
				{
					float Acc[3] = { 0.f, 0.f, 0.f }, AccA = 0.f;
					for (int32 Dy = 0; Dy < 2; ++Dy)
						for (int32 Dx = 0; Dx < 2; ++Dx)
						{
							const int32 Sx = FMath::Min(X * 2 + Dx, Cw - 1), Sy = FMath::Min(Y * 2 + Dy, Ch - 1);
							const uint8* Px = &Cur[(Sy * Cw + Sx) * 4];
							const float Al = Px[3];
							Acc[0] += Px[0] * Al; Acc[1] += Px[1] * Al; Acc[2] += Px[2] * Al; AccA += Al;
						}
					uint8* Q = &Next[(Y * Nw + X) * 4];
					if (AccA > 0.f) for (int32 Ci = 0; Ci < 3; ++Ci) Q[Ci] = (uint8)FMath::Clamp(FMath::RoundToInt(Acc[Ci] / AccA), 0, 255);
					Q[3] = (uint8)FMath::Clamp(FMath::RoundToInt(AccA / 4.f), 0, 255);
				}
			FTexture2DMipMap* Mip = new FTexture2DMipMap();
			Mip->SizeX = Nw;
			Mip->SizeY = Nh;
			Mip->SizeZ = 1;
			PD->Mips.Add(Mip);
			Mip->BulkData.Lock(LOCK_READ_WRITE);
			void* D = Mip->BulkData.Realloc(Next.Num());
			FMemory::Memcpy(D, Next.GetData(), Next.Num());
			Mip->BulkData.Unlock();
			Cur = MoveTemp(Next);
			Cw = Nw; Ch = Nh;
		}
		T->UpdateResource();
		return T;
	}

	struct FGhostsAssets
	{
		bool bLoaded = false;
		TMap<FString, FGhostTex> Tex;
		TSharedPtr<FCompositeFont> Amiri, Courier;

		const FGhostTex* Get(const TCHAR* Name) const { const FGhostTex* T = Tex.Find(Name); return (T && T->IsOk()) ? T : nullptr; }

		void EnsureLoaded()
		{
			if (bLoaded) return;
			bLoaded = true;
			const FString Dir = FPaths::ProjectContentDir() / TEXT("AAACity/UI/GhostsNovember");
			const TCHAR* Names[] = {
				TEXT("topbar"), TEXT("dossier"), TEXT("health"), TEXT("weapon"), TEXT("frame"), TEXT("frame_glow"), TEXT("dial"), TEXT("stamp"),
				TEXT("m0"), TEXT("m1"), TEXT("m2"), TEXT("m3"), TEXT("m4"), TEXT("m5"), TEXT("m6"), TEXT("m7"),
				TEXT("s17"), TEXT("s20"), TEXT("s24"), TEXT("s3"), TEXT("s28"), TEXT("s2"), TEXT("s6"), TEXT("s23"),
				TEXT("fx_vignette"), TEXT("fx_hit"), TEXT("fx_splat"), TEXT("fx_rain"), TEXT("fx_grain"),
				TEXT("eye"), TEXT("dot"), TEXT("mark"), TEXT("mark_glow"), TEXT("ptr"), TEXT("ptr_glow"), TEXT("rose"), TEXT("bullet"), TEXT("needle"), TEXT("needle_shadow"),
				TEXT("o0"), TEXT("o1"), TEXT("o2"), TEXT("o3"), TEXT("o4"), TEXT("o5"), TEXT("o6"), TEXT("o7"), TEXT("rooster"), TEXT("kar98") };
			for (const TCHAR* N : Names)
			{
				FGhostTex T;
				int32 Iw = 0, Ih = 0;
				T.Tex = LoadMipped(Dir / (FString(N) + TEXT(".png")), Iw, Ih);
				if (T.Tex)
				{
					T.Tex->AddToRoot();
					// the size comes from the decoded file (UTexture2D::GetSizeX() of a transient texture can read 0)
					const bool bFx = FString(N).StartsWith(TEXT("fx_"));   // full-screen layers / tiles have no border
					T.Pad = (bFx || Iw <= 2 * TexPad + 2 || Ih <= 2 * TexPad + 2) ? 0.f : (float)TexPad;
					T.Size = FVector2f(FMath::Max(1.f, (float)Iw - 2.f * T.Pad), FMath::Max(1.f, (float)Ih - 2.f * T.Pad));
					T.Brush.SetResourceObject(T.Tex);
					T.Brush.ImageSize = FVector2D((double)Iw, (double)Ih);
					UE_LOG(LogTemp, Log, TEXT("AAA HUD: %s %dx%d (GetSizeX %d, mips %d)"), N, Iw, Ih, T.Tex->GetSizeX(), T.Tex->GetPlatformData() ? T.Tex->GetPlatformData()->Mips.Num() : -1);
					T.Brush.DrawAs = ESlateBrushDrawType::Image;
				}
				Tex.Add(N, T);
			}
			const FString FontDir = Dir / TEXT("Fonts");
			if (FPaths::FileExists(FontDir / TEXT("Amiri-Bold.ttf")))
				Amiri = MakeShared<FStandaloneCompositeFont>(NAME_None, FontDir / TEXT("Amiri-Bold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			if (FPaths::FileExists(FontDir / TEXT("CourierPrime-Bold.ttf")))
				Courier = MakeShared<FStandaloneCompositeFont>(NAME_None, FontDir / TEXT("CourierPrime-Bold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			if (!Get(TEXT("topbar")))
				UE_LOG(LogTemp, Warning, TEXT("AAA HUD: Ghosts of November art not found in %s (copy the UI folder)"), *Dir);
		}
	};
	FGhostsAssets* GGhostsPtr = nullptr;
	FGhostsAssets& Ghosts() { if (!GGhostsPtr) GGhostsPtr = new FGhostsAssets(); return *GGhostsPtr; }

	/** CSS cubic-bezier(x1, y1, x2, y2) easing */
	float Bezier(float X1, float Y1, float X2, float Y2, float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		float T = X;
		for (int32 I = 0; I < 8; ++I)
		{
			const float Cx = 3.f * X1, Bx = 3.f * (X2 - X1) - Cx, Ax = 1.f - Cx - Bx;
			const float Xt = ((Ax * T + Bx) * T + Cx) * T - X;
			const float Dx = (3.f * Ax * T + 2.f * Bx) * T + Cx;
			if (FMath::Abs(Xt) < 1e-4f || FMath::Abs(Dx) < 1e-6f) break;
			T = FMath::Clamp(T - Xt / Dx, 0.f, 1.f);
		}
		const float Cy = 3.f * Y1, By = 3.f * (Y2 - Y1) - Cy, Ay = 1.f - Cy - By;
		return ((Ay * T + By) * T + Cy) * T;
	}

	FLinearColor Hex(uint32 Rgb, float A = 1.f)
	{
		return FLinearColor(FColor((Rgb >> 16) & 255, (Rgb >> 8) & 255, Rgb & 255, 255)).CopyWithNewOpacity(A);
	}

	bool HasArabic(const FString& S)
	{
		for (const TCHAR Ch : S) if (Ch >= 0x0600 && Ch <= 0x06FF) return true;
		return false;
	}

	const TCHAR* const WheelNames[8] = { TEXT("المراقبة"), TEXT("الأسلحة"), TEXT("المعدات"), TEXT("الأوامر"), TEXT("التسلل"), TEXT("العلاج"), TEXT("المهام"), TEXT("الخريطة") };

}

/* ============================================================================================ tick */

void UAAASmartHudWidget::TickGhosts(float InDt)
{
	const float Dt = FMath::Clamp(InDt, 0.f, 0.05f);
	GT += Dt;
	if (!Weapon.IsValid()) return;
	UAAAWeaponComponent* Wp = Weapon.Get();
	ACharacter* C = Wp->Char();

	// health / stamina masks: CSS transition .45s cubic-bezier(.3,1.4,.5,1) (a little overshoot) -> spring
	const float HpPct = FMath::Clamp(Wp->Health / FMath::Max(Wp->MaxHealth, 1.f) * 100.f, 0.f, 100.f);
	if (HpPct < LastHp - 0.5f)
	{
		HitFlash = 1.f;   // #hit.on, then fades over .5s
		FGhostSplat Sp;
		Sp.P = FVector2f(FMath::FRandRange(0.1f, 0.9f), FMath::FRandRange(0.1f, 0.9f));
		Sp.Size = FMath::FRandRange(60.f, 180.f);
		Sp.T = 0.f;
		Splats.Add(Sp);
	}
	LastHp = HpPct;
	HpV += ((HpPct - HpShown) * 190.f - HpV * 17.f) * Dt;  HpShown += HpV * Dt;
	HitFlash = FMath::Max(0.f, HitFlash - Dt / 0.5f);
	for (FGhostSplat& Sp : Splats) Sp.T += Dt;
	Splats.RemoveAll([](const FGhostSplat& Sp) { return Sp.T > 2.1f; });

	// stamina (the page's second bar): sprinting / climbing spend it, it comes back 5 per second (the canteen fills it)
	if (C)
	{
		const float Spd = (float)C->GetVelocity().Size2D();
		if (Wp->ClimbState.bMantling) Wp->Stamina -= 18.f * Dt;
		else if (Spd > 420.f) Wp->Stamina -= 12.f * Dt;
		else Wp->Stamina += 5.f * Dt;
		Wp->Stamina = FMath::Clamp(Wp->Stamina, 0.f, 100.f);
	}
	StV += ((Wp->Stamina - StShown) * 190.f - StV * 17.f) * Dt;  StShown += StV * Dt;
	CritT += Dt;

	// weapon: equip slide-in (.32s), fire kick (70 ms)
	const int32 Ty = (int32)Wp->CurrentWeaponType;
	if (Ty != LastWeaponType || Wp->bArmed != bLastArmed)
	{
		if (LastWeaponType >= 0 && Wp->bArmed)
		{
			EquipAnimT = 0.f;
			const TCHAR* Nm = Wp->CurrentWeaponType == EAAAWeaponType::Kar98 ? TEXT("Kar98k") : (Wp->CurrentWeaponType == EAAAWeaponType::Pistol ? TEXT("P08") : (Wp->CurrentWeaponType == EAAAWeaponType::LMG ? TEXT("MG 42") : TEXT("STG 44")));
			ToastText = FString(TEXT("تم تجهيز ")) + Nm; ToastT = 0.f;
		}
		LastWeaponType = Ty; bLastArmed = Wp->bArmed;
	}
	EquipAnimT += Dt;
	if (Wp->SinceShot < LastSinceShot) { KickAnimT = 0.f; NoiseBoostT = 0.2f; }
	LastSinceShot = Wp->SinceShot;
	KickAnimT += Dt;
	NoiseBoostT -= Dt;
	if (LastAmmo >= 0 && Wp->Ammo > LastAmmo + 1 && Wp->ReloadT < 0.f && Ty == LastWeaponType && Wp->RadialActionSerial == LastActionSerial) { ToastText = TEXT("إعادة تلقيم"); ToastT = 0.f; }
	if (LastAmmo > 0 && Wp->Ammo == 0) { ToastText = TEXT("الذخيرة نفدت — R"); ToastT = 0.f; }
	LastAmmo = Wp->Ammo;
	if (Wp->RadialActionSerial != LastActionSerial)
	{
		LastActionSerial = Wp->RadialActionSerial;
		if (!Wp->RadialActionLabel.IsEmpty()) { ToastText = Wp->RadialActionLabel; ToastT = 0.f; }
	}
	LastHpForHeal = HpPct;

	// detection (the eye / noise meter): wanted by the police
	ATrafficAIManager* Tm = Wp->Traffic();
	const bool bDet = Tm && Tm->GetWantedStars() > 0;
	if (bDet != bDetected && GT > 1.f) { ToastText = bDet ? TEXT("تم رصدك!") : TEXT("عدت إلى التخفي"); ToastT = 0.f; }
	bDetected = bDet;
	if ((NoiseT -= Dt) <= 0.f)
	{
		NoiseT = 0.13f;
		const float Boost = NoiseBoostT > 0.f ? 1.f : 0.f;
		for (int32 K = 0; K < 9; ++K) Noise[K] = FMath::Clamp(10.f + FMath::FRand() * (bDetected ? 80.f : 35.f) + Boost * 60.f, 0.f, 100.f) / 100.f;
	}

	// compass heading + "mouse" parallax / pupil from how fast the view turns
	if (APlayerController* PC = Wp->ControllingPC())
	{
		const FRotator R = PC->GetControlRotation();
		const float Yaw = (float)R.Yaw, Pitch = (float)FRotator::NormalizeAxis(R.Pitch);
		if (!bLookInit) { PrevYaw = Yaw; PrevPitch = Pitch; bLookInit = true; }
		const float Dy = FMath::FindDeltaAngleDegrees(PrevYaw, Yaw), Dp = Pitch - PrevPitch;
		PrevYaw = Yaw; PrevPitch = Pitch;
		const float Inv = 1.f / FMath::Max(Dt, 0.001f);
		LookX = FMath::FInterpTo(LookX, FMath::Clamp(Dy * Inv / 240.f, -0.5f, 0.5f), Dt, 6.f);
		LookY = FMath::FInterpTo(LookY, FMath::Clamp(-Dp * Inv / 160.f, -0.5f, 0.5f), Dt, 6.f);
		Heading = FMath::Fmod(Yaw + 360.f, 360.f);
	}

	// wheel: opening clock, main <-> sub ring switch (close .33 s, rebuild, open again), pointer spring
	const bool bOpen = Wp->bRadialMenuOpen;
	if (bOpen && !bRingWasOpen) { RingT = 0.f; ShownKind = Wp->RadialKind; KindCloseT = -1.f; }
	bRingWasOpen = bOpen;
	RingT += Dt;
	if (bOpen && Wp->RadialKind != ShownKind && KindCloseT < 0.f) KindCloseT = 0.f;
	if (KindCloseT >= 0.f)
	{
		KindCloseT += Dt;
		if (KindCloseT >= 0.33f) { ShownKind = Wp->RadialKind; KindCloseT = -1.f; RingT = 0.f; }
	}
	const int32 Sel = FMath::Clamp(Wp->RadialSelectedIndex, 0, 7);
	if (Sel != PtrSel) { const int32 D = ((Sel - PtrSel + 12) % 8) - 4; PtrTarget += D * 45.f; PtrSel = Sel; }
	PtrRotV += ((PtrTarget - PtrRot) * 170.f - PtrRotV * 16.f) * Dt;  PtrRot += PtrRotV * Dt;
	// compass needle: points north (-heading), a magnetised needle on a pivot: springy, slightly under-damped
	{
		const float Want = -Heading;
		const float Err = FMath::FindDeltaAngleDegrees(NeedleRot, Want);
		NeedleV += (Err * 60.f - NeedleV * 7.f) * Dt;
		NeedleRot += NeedleV * Dt;
		NeedleRot = FMath::UnwindDegrees(NeedleRot);
	}

	if (ToastT >= 0.f) { ToastT += Dt; if (ToastT > 2.2f) ToastT = -1.f; }
	RainT += Dt;
	if ((GrainT -= Dt) <= 0.f) { GrainT = 0.15f / 4.f; GrainOff = FVector2f(FMath::FRandRange(-5.f, 0.f), FMath::FRandRange(-5.f, 0.f)); }
}

/* ============================================================================================ paint */

int32 UAAASmartHudWidget::PaintGhosts(const FGeometry& G, FSlateWindowElementList& Out, int32 LayerId) const
{
	if (!Weapon.IsValid()) return LayerId;
	const UAAAWeaponComponent* Wp = Weapon.Get();
	FGhostsAssets& A = Ghosts();
	A.EnsureLoaded();

	const FVector2f View = G.GetLocalSize();
	const float S = FMath::Min(View.X / DW, View.Y / DH);
	const FVector2f O((View.X - DW * S) * 0.5f, (View.Y - DH * S) * 0.5f);
	const float GeoScale = FMath::Max(G.Scale, 0.01f);
	const FVector2f Par(-LookX * 14.f, -LookY * 10.f);   // #par parallax
	static FSlateColorBrush White(FLinearColor::White);
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// group transform: each panel is scaled about its own anchor (HudPanelScale / HudWheelScale)
	FVector2f GA(0.f, 0.f);
	float GK = 1.f;
	auto Group = [&](float Ax, float Ay, float K) { GA = FVector2f(Ax, Ay); GK = K; };

	auto RT = [](float Deg, float Sc) -> FSlateRenderTransform
	{
		const float Rd = FMath::DegreesToRadians(Deg), Cs = FMath::Cos(Rd) * Sc, Sn = FMath::Sin(Rd) * Sc;
		return FSlateRenderTransform(FMatrix2x2f(Cs, Sn, -Sn, Cs));
	};
	// a box in design space (x, y, w, h), optionally rotated / scaled around Pivot (0..1 of the box)
	auto Geo = [&](float X, float Y, float W, float H, float Deg = 0.f, float Sc = 1.f, FVector2f Pivot = FVector2f(0.5f, 0.5f), bool bPar = true) -> FPaintGeometry
	{
		const FVector2f Dp = GA + (FVector2f(X, Y) - GA) * GK;
		const FVector2f P = O + (Dp + (bPar ? Par : FVector2f::ZeroVector)) * S;
		const FVector2f Sz = FVector2f(W, H) * (S * GK);
		if (Deg == 0.f && Sc == 1.f) return G.ToPaintGeometry(Sz, FSlateLayoutTransform(P));
		return G.ToPaintGeometry(Sz, FSlateLayoutTransform(P), RT(Deg, Sc), Pivot);
	};
	// a picture drawn so that its visible part fills (X, Y, W, H); the transparent border goes around it
	auto DrawTex = [&](const FGhostTex* T, float X, float Y, float W, float H, int32 L, const FLinearColor& Tint, float Deg, float Sc, FVector2f Pivot, bool bPar)
	{
		const float Px = T->Pad * W / FMath::Max(T->Size.X, 1.f), Py = T->Pad * H / FMath::Max(T->Size.Y, 1.f);
		const float Ew = W + 2.f * Px, Eh = H + 2.f * Py;
		const FVector2f Pv((Pivot.X * W + Px) / FMath::Max(Ew, 0.01f), (Pivot.Y * H + Py) / FMath::Max(Eh, 0.01f));
		FSlateDrawElement::MakeBox(Out, LayerId + L, Geo(X - Px, Y - Py, Ew, Eh, Deg, Sc, Pv, bPar), &T->Brush, ESlateDrawEffect::None, Tint);
	};
	auto Img = [&](const TCHAR* Name, float X, float Y, float W, float H, int32 L, const FLinearColor& Tint = FLinearColor::White, float Deg = 0.f, float Sc = 1.f, FVector2f Pivot = FVector2f(0.5f, 0.5f), bool bPar = true)
	{
		const FGhostTex* T = A.Get(Name);
		if (!T) return;
		if (H <= 0.f) H = W * T->Size.Y / T->Size.X;   // CSS: width given, height auto
		DrawTex(T, X, Y, W, H, L, Tint, Deg, Sc, Pivot, bPar);
	};
	// object-fit: contain inside a box, aligned (0 left / .5 centre / 1 right)
	auto ImgFit = [&](const TCHAR* Name, float X, float Y, float W, float H, float AlignX, int32 L, const FLinearColor& Tint = FLinearColor::White, float Deg = 0.f, float Dx = 0.f)
	{
		const FGhostTex* T = A.Get(Name);
		if (!T) return;
		const float K = FMath::Min(W / T->Size.X, H / T->Size.Y), Iw = T->Size.X * K, Ih = T->Size.Y * K;
		DrawTex(T, X + (W - Iw) * AlignX + Dx, Y + (H - Ih) * 0.5f, Iw, Ih, L, Tint, Deg, 1.f, FVector2f(0.5f, 0.5f), true);
	};
	auto Box = [&](float X, float Y, float W, float H, int32 L, const FLinearColor& Col, float Deg = 0.f, FVector2f Pivot = FVector2f(0.5f, 0.5f))
	{
		FSlateDrawElement::MakeBox(Out, LayerId + L, Geo(X, Y, W, H, Deg, 1.f, Pivot), &White, ESlateDrawEffect::None, Col);
	};
	auto FontOf = [&](bool bAmiri, float Px) -> FSlateFontInfo
	{
		TSharedPtr<FCompositeFont> F = bAmiri ? A.Amiri : A.Courier;
		return F.IsValid() ? FSlateFontInfo(F, Px * 0.75f * S * GK) : FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(1, FMath::RoundToInt(Px * 0.75f * S * GK)));
	};
	// shaped text (Arabic joins / right to left), CSS px size
	auto Shape = [&](const FString& Str, bool bAmiri, float Px, bool bRtl) -> FShapedGlyphSequenceRef
	{
		return FontCache->ShapeBidirectionalText(Str, FontOf(bAmiri, Px), GeoScale, bRtl ? TextBiDi::ETextDirection::RightToLeft : TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
	};
	auto Width = [&](const FShapedGlyphSequenceRef& Sh) { return (float)Sh->GetMeasuredWidth() / GeoScale / (S * GK); };
	// distance from the top of a Slate text line to its baseline, in design px
	auto Ascent = [&](bool bAmiri, float Px)
	{
		const FSlateFontInfo Fi = FontOf(bAmiri, Px);
		return ((float)FontMeasure->GetMaxCharacterHeight(Fi, GeoScale) + (float)FontMeasure->GetBaseline(Fi, GeoScale)) / GeoScale / (S * GK);
	};
	// draw shaped text with its top-left at (X, Y) in design px; optional rotation about a design-space point
	auto DrawShaped = [&](const FShapedGlyphSequenceRef& Sh, float X, float Y, int32 L, const FLinearColor& Col, float Deg = 0.f, FVector2f RotAbout = FVector2f::ZeroVector, bool bPar = true)
	{
		const float W = (float)Sh->GetMeasuredWidth() / GeoScale / (S * GK), H = (float)Sh->GetMaxTextHeight() / GeoScale / (S * GK);
		const FVector2f Pv = Deg != 0.f ? FVector2f((RotAbout.X - X) / FMath::Max(W, 1.f), (RotAbout.Y - Y) / FMath::Max(H, 1.f)) : FVector2f(0.5f, 0.5f);
		FSlateDrawElement::MakeShapedText(Out, LayerId + L, Geo(X, Y, W, H, Deg, 1.f, Pv, bPar), Sh, ESlateDrawEffect::None, Col, FLinearColor::Transparent);
	};
	auto Text = [&](const FString& Str, bool bAmiri, float Px, bool bRtl, float X, float Y, float BoxW, float AlignX, int32 L, const FLinearColor& Col, bool bShadow = false, float ShadowPx = 2.f)
	{
		const FShapedGlyphSequenceRef Sh = Shape(Str, bAmiri, Px, bRtl);
		const float W = Width(Sh), Tx = X + (BoxW - W) * AlignX;
		if (bShadow) DrawShaped(Sh, Tx + ShadowPx, Y + ShadowPx, L, FLinearColor(0.f, 0.f, 0.f, Col.A));
		DrawShaped(Sh, Tx, Y, L + 1, Col);
		return W;
	};

	const FLinearColor Paper = Hex(0xe6d5b5), Ink = Hex(0x2a1a0e);
	const bool bDead = Wp->bDead;
	const float PanelK = FMath::Clamp(Wp->HudPanelScale, 0.4f, 1.5f);
	const float WheelK = FMath::Clamp(Wp->HudWheelScale, 0.4f, 1.5f);

	// ---------------------------------------------------------------- chapter 1 (AAAMissionSubsystem): cinematics
	// While a cinematic runs (intro cards, briefing, outro) only black / cinema bars / cards / subtitles are drawn.
	const UAAAMissionSubsystem* Ms = Wp->GetWorld() ? Wp->GetWorld()->GetSubsystem<UAAAMissionSubsystem>() : nullptr;
	const float Ks = View.Y / 1080.f;   // mission texts scale with the screen height
	auto MFont = [&](float Px) -> FSlateFontInfo
	{
		TSharedPtr<FCompositeFont> F = A.Amiri;
		return F.IsValid() ? FSlateFontInfo(F, Px * Ks * 0.75f) : FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(1, FMath::RoundToInt(Px * Ks * 0.75f)));
	};
	auto MShape = [&](const FString& Str, float Px) { return FontCache->ShapeBidirectionalText(Str, MFont(Px), GeoScale, TextBiDi::ETextDirection::RightToLeft, ETextShapingMethod::Auto); };
	auto MSize = [&](const FShapedGlyphSequenceRef& Sh) { return FVector2f((float)Sh->GetMeasuredWidth() / GeoScale, (float)Sh->GetMaxTextHeight() / GeoScale); };
	// text at (X anchor, Y top): AlignX 0 = X is the left edge, .5 = centre, 1 = right edge
	auto MText = [&](const FString& Str, float Px, float X, float Y, float AlignX, int32 L, const FLinearColor& Col, bool bShadow) -> FVector2f
	{
		if (Str.IsEmpty()) return FVector2f::ZeroVector;
		const FShapedGlyphSequenceRef Sh = MShape(Str, Px);
		const FVector2f Sz = MSize(Sh);
		const float Tx = X - Sz.X * AlignX;
		if (bShadow) FSlateDrawElement::MakeShapedText(Out, LayerId + L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(FVector2f(Tx + 2.f, Y + 2.f))), Sh, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Col.A * 0.85f), FLinearColor::Transparent);
		FSlateDrawElement::MakeShapedText(Out, LayerId + L + 1, G.ToPaintGeometry(Sz, FSlateLayoutTransform(FVector2f(Tx, Y))), Sh, ESlateDrawEffect::None, Col, FLinearColor::Transparent);
		return Sz;
	};
	auto MBox = [&](float X, float Y, float W, float H, int32 L, const FLinearColor& Col)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + L, G.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &White, ESlateDrawEffect::None, Col);
	};
	auto Typed = [](const FString& Str, float T, float Per) { return Str.Left(FMath::Clamp(FMath::FloorToInt(T / Per) + 1, 0, Str.Len())); };
	auto MBars = [&](int32 L)
	{
		if (!Ms) return;
		if (Ms->Black > 0.001f) MBox(0.f, 0.f, View.X, View.Y, L, FLinearColor(0.f, 0.f, 0.f, Ms->Black));
		const float Bar = View.Y * 0.11f * Ms->Letterbox;
		if (Bar > 0.5f) { MBox(0.f, 0.f, View.X, Bar, L + 1, FLinearColor::Black); MBox(0.f, View.Y - Bar, View.X, Bar, L + 1, FLinearColor::Black); }
	};
	if (Ms && Ms->IsCinematic())
	{
		MBars(0);
		if (Ms->CardT >= 0.f && Ms->CardAlpha > 0.01f)
		{
			const float Al = Ms->CardAlpha;
			if (Ms->bCardTitle)
			{
				// chapter title: big gold letters, a thin rule, the chapter name fading in under it
				const float Y0 = View.Y * 0.34f;
				const FVector2f S1 = MText(Ms->CardLine1, 96.f, View.X * 0.5f, Y0, 0.5f, 3, Hex(0xc5a059, Al), true);
				MBox(View.X * 0.5f - 190.f * Ks, Y0 + S1.Y + 6.f * Ks, 380.f * Ks, 2.f * Ks, 3, Hex(0x8a5a2a, Al));
				const float A2 = Al * FMath::Clamp((Ms->CardT - 0.8f) / 0.8f, 0.f, 1.f);
				MText(Ms->CardLine2, 34.f, View.X * 0.5f, Y0 + S1.Y + 26.f * Ks, 0.5f, 3, Paper.CopyWithNewOpacity(A2), true);
			}
			else
			{
				// a card: the first line typed (45 ms per letter), the second fading in after it
				const float Y0 = View.Y * 0.42f;
				const FVector2f Full = MSize(MShape(Ms->CardLine1, 46.f));
				const FString L1 = Typed(Ms->CardLine1, Ms->CardT, 0.045f);
				MText(L1, 46.f, View.X * 0.5f + Full.X * 0.5f, Y0, 1.f, 3, Paper.CopyWithNewOpacity(Al), true);   // right-aligned: grows to the left
				const float Done1 = Ms->CardLine1.Len() * 0.045f;
				if (!Ms->CardLine2.IsEmpty() && Ms->CardT > Done1 + 0.3f)
				{
					const float A2 = Al * FMath::Clamp((Ms->CardT - Done1 - 0.3f) / 0.7f, 0.f, 1.f);
					MText(Ms->CardLine2, 30.f, View.X * 0.5f, Y0 + Full.Y + 18.f * Ks, 0.5f, 3, Hex(0xb8a888, A2), true);
				}
			}
		}
		if (Ms->LineT >= 0.f && !Ms->Line.IsEmpty())
		{
			// subtitle above the lower bar: the speaker (his colour) on the right, the line typed to the left of it
			const float Al = FMath::Clamp(Ms->LineT / 0.25f, 0.f, 1.f);
			const FString Name = Ms->Speaker + TEXT(":");
			const FVector2f Sn = MSize(MShape(Name, 32.f)), Sl = MSize(MShape(Ms->Line, 32.f));
			const float Gap = 12.f * Ks, Total = Sn.X + Gap + Sl.X;
			const float Rx = View.X * 0.5f + Total * 0.5f;
			const float Y = View.Y - View.Y * 0.11f * Ms->Letterbox - Sl.Y - 30.f * Ks;
			MBox(Rx - Total - 26.f * Ks, Y - 8.f * Ks, Total + 52.f * Ks, Sl.Y + 16.f * Ks, 2, FLinearColor(0.f, 0.f, 0.f, 0.5f * Al));
			MText(Name, 32.f, Rx, Y, 1.f, 3, Ms->SpeakerColor.CopyWithNewOpacity(Al), true);
			MText(Typed(Ms->Line, Ms->LineT, 0.028f), 32.f, Rx - Sn.X - Gap, Y, 1.f, 3, Paper.CopyWithNewOpacity(Al), true);
		}
		// how to skip (inside the lower bar)
		if (Ms->Phase != EAAAMissionPhase::Boot && !Ms->bDeathScene)
			MText(TEXT("Enter / مسافة — التالي   ·   Backspace — تخطّي المشهد"), 18.f, View.X - 34.f * Ks, View.Y - 40.f * Ks, 1.f, 3, Hex(0x8c7a5d, 0.75f), false);
		return LayerId + 20;
	}

	// ---------------------------------------------------------------- full-screen fx (outside the canvas)
	{
		// screen filter (vignette, rain streaks, film grain) REMOVED on the user's request: the render is shown clean.
		// Set bScreenFilter = true to bring it back. Hit flash and blood splats below stay (damage feedback).
		constexpr bool bScreenFilter = false;
		const FGhostTex* V = bScreenFilter ? A.Get(TEXT("fx_vignette")) : nullptr;
		if (V) FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(View, FSlateLayoutTransform()), &V->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.85f));
		// rain: .07, 60 px tile moving (-24, 60) every .35 s; grain: .28 * .12, jittered
		const FGhostTex* Rn = bScreenFilter ? A.Get(TEXT("fx_rain")) : nullptr;
		if (Rn)
		{
			const float Tile = 120.f, Ph = FMath::Fmod(RainT / 0.35f, 1.f);
			const FVector2f Off(-24.f * Ph * 2.f, 60.f * Ph * 2.f);
			for (float Y = -Tile; Y < View.Y + Tile; Y += Tile)
				for (float X = -Tile; X < View.X + Tile; X += Tile)
					FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(FVector2f(Tile, Tile), FSlateLayoutTransform(FVector2f(X, Y) + FVector2f(FMath::Fmod(Off.X, Tile), FMath::Fmod(Off.Y, Tile)))), &Rn->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.07f));
		}
		const FGhostTex* Gr = bScreenFilter ? A.Get(TEXT("fx_grain")) : nullptr;
		if (Gr)
		{
			const float Tile = 256.f;
			for (float Y = -Tile; Y < View.Y + Tile; Y += Tile)
				for (float X = -Tile; X < View.X + Tile; X += Tile)
					FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(FVector2f(Tile, Tile), FSlateLayoutTransform(FVector2f(X, Y) + GrainOff)), &Gr->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.034f));
		}
		// #hit + blood splats (dark red, fading in 2 s while growing x1.6 and sliding 40 px)
		const FGhostTex* Ht = A.Get(TEXT("fx_hit"));
		if (Ht && HitFlash > 0.001f) FSlateDrawElement::MakeBox(Out, LayerId + 1, G.ToPaintGeometry(View, FSlateLayoutTransform()), &Ht->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, HitFlash));
		const FGhostTex* Sp = A.Get(TEXT("fx_splat"));
		if (Sp)
			for (const FGhostSplat& Sk : Splats)
			{
				const float E = Bezier(0.25f, 0.1f, 0.25f, 1.f, Sk.T / 2.f), Sz = Sk.Size * FMath::Lerp(1.f, 1.6f, E);
				const FVector2f P(Sk.P.X * View.X - Sz * 0.5f, Sk.P.Y * View.Y - Sz * 0.5f + 40.f * E);
				FSlateDrawElement::MakeBox(Out, LayerId + 1, G.ToPaintGeometry(FVector2f(Sz, Sz), FSlateLayoutTransform(P)), &Sp->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.9f * (1.f - E)));
			}
	}
	if (bDead) return LayerId + 2;

	// ---------------------------------------------------------------- top bar: compass tape, marker, eye, noise
	Group(842.5f, 18.f, PanelK);
	Img(TEXT("topbar"), 476.f, 18.f, 733.f, 0.f, 2);
	{
		// #tape: 537 x 39 at (574, 23); labels every 90 px (= 45 deg), ticks every 15 px, faded at both ends (14 %)
		const float TapeX = 574.f, TapeY = 23.f, TapeW = 537.f;
		const float Shift = 268.5f - (720.f + Heading / 360.f * 720.f);
		const TCHAR* const NS[8] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
		auto Fade = [&](float Lx) { const float U = Lx / TapeW; return FMath::Clamp(FMath::Min(U, 1.f - U) / 0.14f, 0.f, 1.f); };
		for (int32 K = 0; K < 24; ++K)
		{
			const float Lx = K * 90.f + Shift;
			if (Lx > -60.f && Lx < TapeW + 60.f)
			{
				const float Fd = Fade(Lx);
				if (Fd > 0.01f)
				{
					const FShapedGlyphSequenceRef Sh = Shape(NS[K % 8], false, 15.f, false);
					const float Wd = Width(Sh);
					DrawShaped(Sh, TapeX + Lx - Wd * 0.5f + 1.f, TapeY + 8.f + 1.f, 3, FLinearColor(0.f, 0.f, 0.f, 0.8f * Fd));   // text-shadow 0 0 6px #000
					DrawShaped(Sh, TapeX + Lx - Wd * 0.5f, TapeY + 8.f, 4, Paper.CopyWithNewOpacity(Fd));
				}
			}
			for (int32 J = 1; J < 6; ++J)
			{
				const float Tx = K * 90.f + J * 15.f + Shift;
				if (Tx < 0.f || Tx > TapeW) continue;
				Box(TapeX + Tx, TapeY + 39.f - 8.f, 1.f, 8.f, 3, Paper.CopyWithNewOpacity(0.6f * Fade(Tx)));
			}
		}
		// #mark (red triangle with a glow)
		Img(TEXT("mark_glow"), 830.f - 6.f, 19.f - 6.f, 28.f, 24.f, 4);
		Img(TEXT("mark"), 830.f, 19.f, 16.f, 12.f, 5);
		// #nz: 9 bars, 56 x 24 at (838, 72), gap 3, bottom aligned
		const FLinearColor NzCol = bDetected ? Hex(0xff4a3c) : Paper.CopyWithNewOpacity(0.75f);
		const float Bw = (56.f - 8.f * 3.f) / 9.f;
		for (int32 K = 0; K < 9; ++K) { const float Bh = 24.f * Noise[K]; Box(838.f + K * (Bw + 3.f), 72.f + 24.f - Bh, Bw, Bh, 4, NzCol); }
		// #eye + pupil (follows the view turning; red and bigger when detected)
		const FLinearColor EyeCol = bDetected ? Hex(0xff4a3c) : Paper;
		Img(TEXT("eye"), 902.f, 70.f, 46.f, 26.f, 4, EyeCol);
		const float Pr = bDetected ? 6.f : 4.f, Px = 902.f + 23.f + LookX * 8.f, Py = 70.f + 13.f + LookY * 5.f;
		Img(TEXT("dot"), Px - Pr, Py - Pr, Pr * 2.f, Pr * 2.f, 5, EyeCol);
	}

	// ---------------------------------------------------------------- dossier (typed, redacted, stamped)
	// Every line is placed on the page's own baselines (CSS line boxes of Amiri 19 / Courier 14), inside #doc
	// (44, 52, 345 x 150) turned -3.5 deg about its centre, so the writing sits on the paper like in the page.
	Group(19.f, 33.f, PanelK);
	Img(TEXT("dossier"), 19.f, 33.f, 408.f, 0.f, 2);
	{
		const float Deg = -3.5f;
		const FVector2f Ctr(44.f + 345.f * 0.5f, 52.f + 150.f * 0.5f);
		const float Left = 44.f, Right = 44.f + 345.f;
		const float AscA = Ascent(true, 19.f), AscC = Ascent(false, 14.f);
		// measured on the page itself (Chromium render of the HTML, in #doc before its rotation)
		const float HeadBase = 68.6f;      // baseline of "ملف العملية" / NOV-544
		const float RuleY = 77.6f;         // the 2 px rule
		const float LineH = 19.f * 1.55f;  // body line height
		const float FirstBase = 108.3f;    // baseline of the first body line
		// header: "ملف العملية" (right) / NOV-544 (left) on one baseline, 2 px rule under it
		const FShapedGlyphSequenceRef H1 = Shape(TEXT("ملف العملية"), true, 19.f, true);
		DrawShaped(H1, Right - Width(H1), HeadBase - AscA, 4, Ink, Deg, Ctr);
		const FShapedGlyphSequenceRef H2 = Shape(TEXT("NOV-544"), false, 14.f, false);
		DrawShaped(H2, Left, HeadBase - AscC, 4, Ink, Deg, Ctr);
		Box(Left, RuleY, 345.f, 2.f, 3, Hex(0x3a2a1a), Deg, FVector2f((Ctr.X - Left) / 345.f, (Ctr.Y - RuleY) / 2.f));

		// body: typed at 32 ms per character (60 ms inside the redactions) from 0.5 s, wrapped at 345 px, right to left
		struct FSeg { const TCHAR* Txt; bool bRed; bool bBreak; };
		static const FSeg Segs[] = {
			{ TEXT("الهدف: اعتراض القافلة في "), false, false }, { TEXT("وادي عبدي"), true, false }, { TEXT("."), false, true },
			{ TEXT("التوقيت: "), false, false }, { TEXT("الفجر"), true, false }, { TEXT("."), false, true },
			{ TEXT("تحذير: العدو يمتلك مدرعات ثقيلة."), false, false } };
		float Clock = 0.5f;
		float X = Right, Base = FirstBase;
		for (const FSeg& Sg : Segs)
		{
			const FString Full(Sg.Txt);
			const float Per = Sg.bRed ? 0.06f : 0.032f;
			const int32 Shown = FMath::Clamp(FMath::FloorToInt((GT - Clock) / Per) + 1, 0, Full.Len());
			Clock += Per * Full.Len();
			if (Shown <= 0) break;
			if (Sg.bRed)
			{
				// redaction: a black bar the width of the (hidden) words, padding 0 6px, wraps as one piece
				const float W = Width(Shape(Full.Left(Shown), true, 19.f, true)) + 12.f;
				if (X - W < Left) { X = Right; Base += LineH; }
				const float By = Base - 20.f;
				Box(X - W, By, W, 24.f, 4, Hex(0x0d0907), Deg, FVector2f((Ctr.X - (X - W)) / W, (Ctr.Y - By) / 24.f));
				X -= W;
			}
			else
			{
				TArray<FString> Words;
				Full.Left(Shown).ParseIntoArray(Words, TEXT(" "), false);
				for (int32 Wi = 0; Wi < Words.Num(); ++Wi)
				{
					const FString Word = Words[Wi] + ((Wi < Words.Num() - 1) ? TEXT(" ") : TEXT(""));
					if (Word.IsEmpty()) continue;
					const FShapedGlyphSequenceRef Sh = Shape(Word, true, 19.f, true);
					const float W = Width(Sh);
					if (X - W < Left && X < Right - 1.f) { X = Right; Base += LineH; }
					DrawShaped(Sh, X - W, Base - AscA, 4, Ink.CopyWithNewOpacity(0.92f), Deg, Ctr);
					X -= W;
				}
			}
			if (Sg.bBreak) { X = Right; Base += LineH; }
		}
		// #stamp: .4 s after the text, slams in (scale 2.4 -> 1, opacity -> .92, rotate -12)
		const float Ts = GT - (Clock + 0.4f);
		if (Ts > 0.f)
		{
			const float E = Bezier(0.5f, 0.f, 0.9f, 0.4f, Ts / 0.14f);
			const float Op = FMath::Clamp(Ts / 0.1f, 0.f, 1.f) * 0.92f;
			Img(TEXT("stamp"), 238.f, 136.f, 146.f, 0.f, 5, FLinearColor(1.f, 1.f, 1.f, Op), -12.f, FMath::Lerp(2.4f, 1.f, E));
		}
	}

	// ---------------------------------------------------------------- health / stamina / name
	Group(13.f, 761.f + 149.f, PanelK);
	Img(TEXT("health"), 13.f, 761.f, 451.f, 0.f, 2);
	{
		const float Hw = (100.f - FMath::Clamp(HpShown, 0.f, 100.f)) / 100.f * 237.f;
		const float Sw = (100.f - FMath::Clamp(StShown, 0.f, 100.f)) / 100.f * 230.f;
		if (Hw > 0.f) Box(320.f - Hw, 840.f, Hw, 22.f, 3, Hex(0x150d08));
		if (Sw > 0.f) Box(324.f - Sw, 873.f, Sw, 15.f, 3, Hex(0x150d08));
		const bool bCrit = Wp->Health / FMath::Max(Wp->MaxHealth, 1.f) < 0.3f;
		const float Shk = bCrit ? (FMath::Fmod(CritT, 0.4f) < 0.2f ? 1.f : -1.f) : 0.f;
		Text(TEXT("مصطفى بن بولعيد"), true, 25.f, true, 96.f + Shk, 796.f + Shk, 222.f, 1.f, 4, bCrit ? Hex(0xff6a5a) : Paper, true, 2.f);
	}
	// stance: crouched / behind cover / leaning (next to the health panel)
	{
		FString St;
		if (Wp->IsInCover()) St = TEXT("خلف ساتر");
		else if (Wp->IsCrouchedPose()) St = TEXT("منحنٍ");
		if (Wp->GetLean() > 0.5f) St += St.IsEmpty() ? TEXT("ميل يمين") : TEXT(" · ميل يمين");
		else if (Wp->GetLean() < -0.5f) St += St.IsEmpty() ? TEXT("ميل يسار") : TEXT(" · ميل يسار");
		if (!St.IsEmpty()) Text(St, true, 20.f, true, 470.f, 846.f, 180.f, 0.f, 4, Paper, true, 2.f);
	}

	// ---------------------------------------------------------------- the fire team: cards, callouts, world markers
	if (UAAAWeaponsSubsystem* Sq = Wp->GetWorld() ? Wp->GetWorld()->GetSubsystem<UAAAWeaponsSubsystem>() : nullptr)
	{
		const float NowT = (float)Wp->GetWorld()->GetTimeSeconds();
		const FLinearColor Sepia = Hex(0xc9b48a);
		// cards (above the health panel): colour strip, name, trade, what he does, health / ammo
		Group(13.f, 750.f, PanelK);
		{
			const float CX = 22.f, CW = 262.f, CH = 34.f, Gap = 5.f;
			const float Top = 750.f - 3.f * (CH + Gap) - 24.f;
			FString Head = FString::Printf(TEXT("الفريق · تشكيل: %s · النار: %s"), FAAASquadInfo::FormationName(Sq->GetFormation()), Sq->IsSquadHoldFire() ? TEXT("مقيّدة") : TEXT("حرّة"));
			if (Sq->GetSquadSelected() >= 0) Head += FString::Printf(TEXT(" · الأوامر إلى %s"), FAAASquadInfo::Name(Sq->GetSquadSelected()));
			Box(CX, Top, CW, 20.f, 3, FLinearColor(0.06f, 0.035f, 0.02f, 0.72f));
			Text(Head, true, 13.f, true, CX + 6.f, Top - 1.f, CW - 12.f, 1.f, 4, Sepia);
			for (int32 I = 0; I < 3; ++I)
			{
				const float Y = Top + 24.f + I * (CH + Gap);
				const AAAAPoliceOfficer* M = Sq->GetSquadMember(I);
				const bool bSel = Sq->GetSquadSelected() == I;
				const FLinearColor Col = FAAASquadInfo::Color(I);
				Box(CX, Y, CW, CH, 3, FLinearColor(0.06f, 0.035f, 0.02f, bSel ? 0.9f : 0.74f));
				if (bSel) { Box(CX - 2.f, Y - 2.f, CW + 4.f, 2.f, 4, Hex(0xff9a2e)); Box(CX - 2.f, Y + CH, CW + 4.f, 2.f, 4, Hex(0xff9a2e)); }
				Box(CX + CW - 5.f, Y, 5.f, CH, 4, Col);                                       // colour strip (right: RTL)
				FString Status;
				FLinearColor StCol = Sepia;
				float Hp = 0.f, Am = 0.f;
				if (!M || M->IsDead())
				{
					const float R = Sq->GetReinforceT(I);
					Status = R > 0.f ? FString::Printf(TEXT("استشهد · تعزيز بعد %d ث"), FMath::CeilToInt(R)) : FString(TEXT("استشهد"));
					StCol = Hex(0x8c7a5d);
				}
				else if (M->IsDowned())
				{
					Status = FString::Printf(TEXT("مصاب! ينزف %d ث"), FMath::CeilToInt(M->GetBleedT()));
					const float Pulse = 0.6f + 0.4f * FMath::Sin(GT * 8.f);
					StCol = FLinearColor(1.f, 0.3f, 0.22f, Pulse);
				}
				else
				{
					Hp = M->GetHealth01(); Am = M->GetAmmo01();
					switch (M->GetTask())
					{
					case AAAAPoliceOfficer::ETask::Fight: Status = TEXT("يشتبك"); break;
					case AAAAPoliceOfficer::ETask::Reposition: Status = TEXT("يغيّر موقعه"); break;
					case AAAAPoliceOfficer::ETask::Bound: Status = TEXT("يتقدّم تحت التغطية"); break;
					case AAAAPoliceOfficer::ETask::Flank: Status = TEXT("يلتفّ"); break;
					case AAAAPoliceOfficer::ETask::CoverFire: Status = TEXT("نيران تغطية"); break;
					case AAAAPoliceOfficer::ETask::Grenade: Status = TEXT("قنبلة!"); break;
					case AAAAPoliceOfficer::ETask::Retreat: Status = TEXT("يتراجع"); break;
					case AAAAPoliceOfficer::ETask::Reload: Status = TEXT("يعيد التلقيم"); break;
					case AAAAPoliceOfficer::ETask::Revive: Status = TEXT("يسعف"); break;
					default: Status = FAAASquadInfo::OrderName(M->GetOrder()); break;
					}
					if (M->GetSuppression() > 0.6f) Status += TEXT(" · تحت النار");
					if (M->GetMags() <= 1) Status += TEXT(" · ذخيرة قليلة");
				}
				static const TCHAR* const Trades[3] = { TEXT("رامي"), TEXT("قنّاص"), TEXT("رشّاش ومسعف") };
				Text(FString::Printf(TEXT("%d  %s"), I + 1, FAAASquadInfo::Name(I)), true, 16.f, true, CX + 8.f, Y - 1.f, CW - 20.f, 1.f, 4, Paper);
				Text(Trades[I], true, 12.f, true, CX + 6.f, Y + 1.f, 90.f, 0.f, 4, Hex(0x8c7a5d));
				Text(Status, true, 13.f, true, CX + 8.f, Y + 16.f, CW - 20.f, 1.f, 4, StCol);
				// health (red .. green) and ammo (brass) hairlines along the bottom
				if (Hp > 0.f) Box(CX + CW - 8.f - (CW - 16.f) * Hp, Y + CH - 4.f, (CW - 16.f) * Hp, 2.f, 4, FMath::Lerp(Hex(0x8a0b0b), Hex(0x6fae4a), Hp));
				if (Am > 0.f) Box(CX + CW - 8.f - (CW - 16.f) * Am * 0.5f, Y + CH - 1.5f, (CW - 16.f) * Am * 0.5f, 1.5f, 4, Hex(0xc5a059));
			}
		}
		// the radio: the last callouts (newest at the bottom), under the dossier
		Group(19.f, 250.f, PanelK);
		{
			float Y = 252.f;
			const int32 N0 = FMath::Max(0, Sq->Callouts.Num() - 4);
			for (int32 K = N0; K < Sq->Callouts.Num(); ++K)
			{
				const UAAAWeaponsSubsystem::FCallout& Co = Sq->Callouts[K];
				const float Al = FMath::Clamp((7.f - Co.T) / 1.2f, 0.f, 1.f) * FMath::Clamp(Co.T / 0.15f, 0.f, 1.f);
				if (Al <= 0.01f) continue;
				const FShapedGlyphSequenceRef ShN = Shape(Co.Who + TEXT(":"), true, 15.f, true);
				const FShapedGlyphSequenceRef ShT = Shape(Co.Text, true, 15.f, true);
				const float Wn = Width(ShN), Wt = Width(ShT), RightX = 400.f;
				Box(RightX - Wn - Wt - 18.f, Y + 2.f, Wn + Wt + 18.f, 22.f, 3, FLinearColor(0.04f, 0.025f, 0.015f, 0.62f * Al));
				DrawShaped(ShN, RightX - Wn - 4.f, Y, 4, Co.Color.CopyWithNewOpacity(Al));
				DrawShaped(ShT, RightX - Wn - Wt - 10.f, Y, 4, Paper.CopyWithNewOpacity(Al));
				Y += 25.f;
			}
		}
		Group(0.f, 0.f, 1.f);
		// markers in the world (screen space): the men, the enemies they reported, the last order's point
		if (APlayerController* PC = Wp->ControllingPC())
		{
			auto ToScreen = [&](const FVector& Wl, FVector2f& OutP) -> bool
			{
				FVector2D Sp;
				if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Wl, Sp, false)) return false;
				OutP = FVector2f((float)Sp.X, (float)Sp.Y);
				return OutP.X > -50.f && OutP.Y > -50.f && OutP.X < View.X + 50.f && OutP.Y < View.Y + 50.f;
			};
			auto Diamond = [&](const FVector2f& C, float R, int32 L, const FLinearColor& Col)
			{
				FSlateDrawElement::MakeBox(Out, LayerId + L, G.ToPaintGeometry(FVector2f(R, R), FSlateLayoutTransform(C - FVector2f(R, R) * 0.5f), RT(45.f, 1.f), FVector2f(0.5f, 0.5f)), &White, ESlateDrawEffect::None, Col);
			};
			auto Label = [&](const FString& T, const FVector2f& C, float Px, const FLinearColor& Col, int32 L)
			{
				TSharedPtr<FCompositeFont> F = A.Amiri;
				FSlateFontInfo Fi = F.IsValid() ? FSlateFontInfo(F, Px * 0.75f) : FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(Px * 0.75f));
				const FShapedGlyphSequenceRef Sh = FontCache->ShapeBidirectionalText(T, Fi, GeoScale, TextBiDi::ETextDirection::RightToLeft, ETextShapingMethod::Auto);
				const float Tw = (float)Sh->GetMeasuredWidth() / GeoScale, Th = (float)Sh->GetMaxTextHeight() / GeoScale;
				FSlateDrawElement::MakeShapedText(Out, LayerId + L, G.ToPaintGeometry(FVector2f(Tw, Th), FSlateLayoutTransform(C - FVector2f(Tw * 0.5f - 1.f, -1.f))), Sh, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Col.A * 0.8f), FLinearColor::Transparent);
				FSlateDrawElement::MakeShapedText(Out, LayerId + L + 1, G.ToPaintGeometry(FVector2f(Tw, Th), FSlateLayoutTransform(C - FVector2f(Tw * 0.5f, 0.f))), Sh, ESlateDrawEffect::None, Col, FLinearColor::Transparent);
			};
			const ACharacter* MeC = Wp->Char();
			const FVector MeLoc = MeC ? MeC->GetActorLocation() : FVector::ZeroVector;
			// enemy contacts the men know of (merged)
			TArray<FVector> CPos; TArray<float> CFr;
			for (int32 I = 0; I < 3; ++I) if (const AAAAPoliceOfficer* M = Sq->GetSquadMember(I)) if (!M->IsOut()) M->GetContacts(CPos, CFr);
			TArray<FVector> Drawn;
			const FGhostTex* Rooster = A.Get(TEXT("rooster"));
			for (int32 K = 0; K < CPos.Num(); ++K)
			{
				bool bDup = false;
				for (const FVector& Dv : Drawn) if (FVector::Dist(Dv, CPos[K]) < 350.f) { bDup = true; break; }
				if (bDup) continue;
				Drawn.Add(CPos[K]);
				FVector2f Sp;
				if (!ToScreen(CPos[K] + FVector(0.0, 0.0, 120.0), Sp)) continue;
				const float Fr = CFr[K];
				const int32 Mtr = FMath::RoundToInt((float)FVector::Dist(MeLoc, CPos[K]) / 100.f);
				float LabelY = 8.f;
				if (Rooster)
				{
					// the enemy mark: the rooster in its sight ring - a fresh contact is full size and opaque,
					// an old one shrinks a little and fades; a soft dark copy under it keeps it readable on bright walls
					const float Rs = FMath::Lerp(22.f, 30.f, Fr);
					const float Al = 0.4f + 0.6f * Fr;
					const FVector2f Ts(Rs, Rs);
					FSlateDrawElement::MakeBox(Out, LayerId + 5, G.ToPaintGeometry(Ts, FSlateLayoutTransform(Sp - Ts * 0.5f + FVector2f(1.2f, 1.6f))), &Rooster->Brush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.45f * Al));
					FSlateDrawElement::MakeBox(Out, LayerId + 6, G.ToPaintGeometry(Ts, FSlateLayoutTransform(Sp - Ts * 0.5f)), &Rooster->Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Al));
					LabelY = Rs * 0.5f + 1.f;
				}
				else
				{
					Diamond(Sp, 11.f, 5, FLinearColor(0.f, 0.f, 0.f, 0.5f * Fr));
					Diamond(Sp, 8.f, 6, FLinearColor(0.95f, 0.18f, 0.12f, 0.35f + 0.6f * Fr));
				}
				Label(FString::Printf(TEXT("%d م"), Mtr), Sp + FVector2f(0.f, LabelY), 11.f, FLinearColor(1.f, 0.55f, 0.45f, 0.4f + 0.5f * Fr), 6);
			}
			// the men (their name labels are stacked when they would overlap: the men walk close together)
			TArray<FVector2f> LabelAt;
			auto FreeSpot = [&LabelAt](FVector2f Lp)
			{
				for (int32 Pass = 0; Pass < 4; ++Pass)
				{
					bool bHit = false;
					for (const FVector2f& Q : LabelAt) if (FMath::Abs(Q.X - Lp.X) < 120.f && FMath::Abs(Q.Y - Lp.Y) < 17.f) { bHit = true; break; }
					if (!bHit) break;
					Lp.Y += 17.f;
				}
				LabelAt.Add(Lp);
				return Lp;
			};
			for (int32 I = 0; I < 3; ++I)
			{
				const AAAAPoliceOfficer* M = Sq->GetSquadMember(I);
				if (!M || M->IsDead()) continue;
				FVector2f Sp;
				if (!ToScreen(M->GetActorLocation() + FVector(0.0, 0.0, M->IsDowned() ? 40.0 : 125.0), Sp)) continue;
				const FLinearColor Col = FAAASquadInfo::Color(I);
				const float Dm = (float)FVector::Dist(MeLoc, M->GetActorLocation());
				const bool bSel = Sq->GetSquadSelected() == I;
				if (M->IsDowned())
				{
					const float Pulse = 0.55f + 0.45f * FMath::Sin(GT * 7.f);
					FSlateDrawElement::MakeBox(Out, LayerId + 6, G.ToPaintGeometry(FVector2f(18.f, 6.f), FSlateLayoutTransform(Sp - FVector2f(9.f, 3.f))), &White, ESlateDrawEffect::None, FLinearColor(1.f, 0.2f, 0.15f, Pulse));
					FSlateDrawElement::MakeBox(Out, LayerId + 6, G.ToPaintGeometry(FVector2f(6.f, 18.f), FSlateLayoutTransform(Sp - FVector2f(3.f, 9.f))), &White, ESlateDrawEffect::None, FLinearColor(1.f, 0.2f, 0.15f, Pulse));
					Label(FString::Printf(TEXT("%s مصاب · %d م"), FAAASquadInfo::Name(I), FMath::RoundToInt(Dm / 100.f)), FreeSpot(Sp + FVector2f(0.f, 11.f)), 13.f, FLinearColor(1.f, 0.6f, 0.5f, 0.95f), 6);
					continue;
				}
				// a small chevron in his colour; the name when far or selected
				Diamond(Sp, bSel ? 12.f : 9.f, 5, FLinearColor(0.f, 0.f, 0.f, 0.55f));
				Diamond(Sp, bSel ? 9.f : 6.5f, 6, Col.CopyWithNewOpacity(0.95f));
				if (Dm > 900.f || bSel) Label(FString::Printf(TEXT("%s · %d م"), FAAASquadInfo::Name(I), FMath::RoundToInt(Dm / 100.f)), FreeSpot(Sp + FVector2f(0.f, 8.f)), 12.f, Col.CopyWithNewOpacity(0.9f), 6);
			}
			// the last order's point (5 s)
			const float Ot = NowT - Sq->LastOrderT;
			if (Ot < 5.f && Sq->LastOrder != EAAASquadOrder::Follow && Sq->LastOrder != EAAASquadOrder::FallBack)
			{
				FVector2f Sp;
				if (ToScreen(Sq->LastOrderPos + FVector(0.0, 0.0, 30.0), Sp))
				{
					const float Ao = FMath::Clamp(5.f - Ot, 0.f, 1.f);
					const float Rr = 14.f + 6.f * FMath::Sin(Ot * 6.f);
					Diamond(Sp, Rr + 4.f, 5, FLinearColor(0.f, 0.f, 0.f, 0.4f * Ao));
					Diamond(Sp, Rr, 6, FLinearColor(1.f, 0.62f, 0.2f, 0.75f * Ao));
					Diamond(Sp, Rr * 0.55f, 7, FLinearColor(0.08f, 0.05f, 0.03f, 0.85f * Ao));
					Label(FAAASquadInfo::OrderName(Sq->LastOrder), Sp + FVector2f(0.f, Rr * 0.8f + 4.f), 14.f, FLinearColor(1.f, 0.78f, 0.45f, Ao), 7);
				}
			}
		}
		// first aid by the leader
		if (const AAAAPoliceOfficer* Pat = Wp->GetRevivePatient())
		{
			TSharedPtr<FCompositeFont> F = A.Amiri;
			FSlateFontInfo Fi = F.IsValid() ? FSlateFontInfo(F, 20.f * 0.75f) : FCoreStyle::GetDefaultFontStyle("Bold", 15);
			const FString Msg = Wp->GetReviveHoldT() > 0.f ? FString::Printf(TEXT("تسعف %s..."), FAAASquadInfo::Name(Pat->SquadIndex)) : FString::Printf(TEXT("اضغط F مطولاً لإسعاف %s"), FAAASquadInfo::Name(Pat->SquadIndex));
			const FShapedGlyphSequenceRef Sh = FontCache->ShapeBidirectionalText(Msg, Fi, GeoScale, TextBiDi::ETextDirection::RightToLeft, ETextShapingMethod::Auto);
			const float Tw = (float)Sh->GetMeasuredWidth() / GeoScale, Th = (float)Sh->GetMaxTextHeight() / GeoScale;
			const FVector2f C0((View.X - Tw) * 0.5f, View.Y * 0.62f);
			FSlateDrawElement::MakeBox(Out, LayerId + 11, G.ToPaintGeometry(FVector2f(Tw + 30.f, Th + 22.f), FSlateLayoutTransform(C0 - FVector2f(15.f, 6.f))), &White, ESlateDrawEffect::None, FLinearColor(0.06f, 0.035f, 0.02f, 0.82f));
			FSlateDrawElement::MakeShapedText(Out, LayerId + 12, G.ToPaintGeometry(FVector2f(Tw, Th), FSlateLayoutTransform(C0)), Sh, ESlateDrawEffect::None, Paper, FLinearColor::Transparent);
			const float Pr = FMath::Clamp(Pat->GetReviveProgress(), 0.f, 1.f);
			FSlateDrawElement::MakeBox(Out, LayerId + 12, G.ToPaintGeometry(FVector2f((Tw + 20.f) * Pr, 4.f), FSlateLayoutTransform(C0 + FVector2f(-10.f, Th + 8.f))), &White, ESlateDrawEffect::None, Hex(0x6fae4a));
		}
	}

	// ---------------------------------------------------------------- weapon panel
	Group(1237.f + 418.f, 770.f + 152.f, PanelK);
	Img(TEXT("weapon"), 1237.f, 770.f, 418.f, 0.f, 2);
	{
		const EAAAWeaponType Ty = Wp->CurrentWeaponType;
		const TCHAR* Icon = Ty == EAAAWeaponType::Kar98 ? TEXT("kar98") : (Ty == EAAAWeaponType::Pistol ? TEXT("s24") : (Ty == EAAAWeaponType::LMG ? TEXT("s20") : TEXT("s17")));
		const TCHAR* WName = Ty == EAAAWeaponType::Kar98 ? TEXT("Kar98k") : (Ty == EAAAWeaponType::Pistol ? TEXT("P08") : (Ty == EAAAWeaponType::LMG ? TEXT("MG 42") : TEXT("STG 44")));
		const float Holster = Wp->bArmed ? 1.f : 0.55f;
		// equip: translateX(50px) + fade in over .32s ease-out; fire: translateX(6px) rotate(1.5deg) for 70 ms
		const float Ee = Bezier(0.f, 0.f, 0.58f, 1.f, EquipAnimT / 0.32f);
		const bool bKick = KickAnimT < 0.07f;
		ImgFit(Icon, 1424.f, 773.f, 196.f, 68.f, 1.f, 4, FLinearColor(1.f, 1.f, 1.f, Ee * Holster), bKick ? 1.5f : 0.f, (1.f - Ee) * 50.f + (bKick ? 6.f : 0.f));
		// #wname: Courier 15, ink, letter-spacing 1px, centred in 134 px
		{
			const FString N(WName);
			float Tw = 0.f;
			TArray<FShapedGlyphSequenceRef> Glyphs;
			for (int32 K = 0; K < N.Len(); ++K) { Glyphs.Add(Shape(N.Mid(K, 1), false, 15.f, false)); Tw += Width(Glyphs.Last()) + 1.f; }
			float Gx = 1298.f + (134.f - Tw) * 0.5f;
			for (const FShapedGlyphSequenceRef& Gs : Glyphs) { DrawShaped(Gs, Gx, 795.f, 4, Ink); Gx += Width(Gs) + 1.f; }
		}
		// #ammo (Amiri 50, glow), #belt (9 rounds), #mags (3), MAGS
		const int32 Cap = FMath::Max(Wp->MagazineSize, 1);
		const FString AmmoStr = FString::Printf(TEXT("%02d"), FMath::Max(Wp->Ammo, 0));
		{
			const FShapedGlyphSequenceRef Sh = Shape(AmmoStr, true, 50.f, false);
			for (int32 K = 0; K < 8; ++K)
			{
				const float An = K * UE_PI / 4.f;
				DrawShaped(Sh, 1366.f + FMath::Cos(An) * 3.f, 838.f + FMath::Sin(An) * 3.f, 3, FLinearColor(0.f, 0.f, 0.f, 0.18f));
			}
			DrawShaped(Sh, 1366.f, 838.f, 4, Paper);
		}
		const int32 FullN = FMath::CeilToInt((float)FMath::Max(Wp->Ammo, 0) / Cap * 9.f);
		for (int32 K = 0; K < 9; ++K) Img(TEXT("bullet"), 1443.f + K * 9.f, 861.f + 4.f, 6.f, 22.f, 4, FLinearColor(1.f, 1.f, 1.f, K < FullN ? 1.f : 0.15f));
		const int32 Mags = Wp->bInfiniteReserve ? 3 : FMath::Clamp(Wp->ReserveAmmo / Cap, 0, 3);
		for (int32 K = 0; K < 3; ++K) Box(1538.f + K * 12.f, 866.f, 6.f, 20.f, 4, K < Mags ? Hex(0xd8d3c4) : Hex(0x6c6a62));
		Text(TEXT("MAGS"), false, 12.f, false, 1584.f, 868.f, 60.f, 0.f, 4, Hex(0xc9c2b0));
	}

	// ---------------------------------------------------------------- the wheel (held TAB / Q) and its sub wheels
	const float RingA = FMath::Clamp(Wp->RadialMenuAlpha, 0.f, 1.f);
	if (RingA > 0.01f)
	{
		Group(RingC.X, RingC.Y, WheelK);
		const int32 Kind = ShownKind;
		int32 SubNum = 0;
		const FAAARadialSubItem* SubItems = Kind != 0 ? UAAAWeaponComponent::GetRadialSubItems(Kind, SubNum) : nullptr;
		const int32 NumFrames = Kind != 0 ? SubNum : 8;
		const int32 Sel = Kind != 0 ? FMath::Clamp(Wp->RadialSubIndex, 0, FMath::Max(SubNum - 1, 0)) : FMath::Clamp(Wp->RadialSelectedIndex, 0, 7);
		const bool bClosing = KindCloseT >= 0.f;
		const float Op = RingA * (bClosing ? FMath::Clamp(1.f - KindCloseT / 0.3f, 0.f, 1.f) : 1.f);
		// the equipped weapon in the weapons ring
		int32 Equipped = -1;
		if (Kind == 1) Equipped = !Wp->bArmed ? 3 : (Wp->CurrentWeaponType == EAAAWeaponType::Kar98 ? 4 : Wp->CurrentWeaponType == EAAAWeaponType::LMG ? 1 : (Wp->CurrentWeaponType == EAAAWeaponType::Pistol ? 2 : 0));

		// #dialImg: rotate(-200deg) scale(.2) -> none over .9s cubic-bezier(.2,1.1,.3,1); going back while switching rings
		{
			const float E = bClosing ? 1.f - Bezier(0.2f, 1.1f, 0.3f, 1.f, KindCloseT / 0.9f) : Bezier(0.2f, 1.1f, 0.3f, 1.f, RingT / 0.9f);
			const float Fo = bClosing ? Op : Op * FMath::Clamp(RingT / 0.35f, 0.f, 1.f);
			const float Dw = 300.f, Dh = 300.f * 618.f / 640.f;
			Img(TEXT("dial"), RingC.X - 150.f, RingC.Y - 146.f, Dw, Dh, 6, FLinearColor(1.f, 1.f, 1.f, Fo), FMath::Lerp(-200.f, 0.f, E), FMath::Lerp(0.2f, 1.f, E), FVector2f(150.f / Dw, 146.f / Dh));
		}
		// frames: from rotate(a - 80deg) translateY(-20px) scale(.15) to rotate(a) translateY(-R) scale(1),
		// .65s cubic-bezier(.2,1.25,.4,1) each, 45 ms apart; the picture inside stays upright
		for (int32 K = 0; K < NumFrames; ++K)
		{
			float E, Fo;
			if (bClosing) { E = 1.f - Bezier(0.2f, 1.25f, 0.4f, 1.f, KindCloseT / 0.65f); Fo = Op; }
			else
			{
				E = Bezier(0.2f, 1.25f, 0.4f, 1.f, (RingT - K * 0.045f) / 0.65f);
				Fo = FMath::Clamp((RingT - K * 0.045f) / 0.3f, 0.f, 1.f) * Op;
			}
			if (Fo <= 0.f) continue;
			const float Fs = Kind == 0 ? 1.05f : 1.f;   // .fi scale (.sub .fi = 1)
			const float Ang = K * 45.f - 80.f * (1.f - E), Rad = FMath::Lerp(20.f, RingR, E), Sc = FMath::Lerp(0.15f, 1.f, E) * Fs;
			const float Ar = FMath::DegreesToRadians(Ang);
			const FVector2f Up(FMath::Sin(Ar), -FMath::Cos(Ar));
			const FVector2f P = RingC + Up * Rad;
			const bool bSel = K == Sel;
			if (bSel)
			{
				const float Gw = 224.f * 540.f / 460.f, Gh = 165.f * 418.f / 338.f;
				Img(TEXT("frame_glow"), P.X - Gw * 0.5f, P.Y - Gh * 0.5f, Gw, Gh, 7, FLinearColor(1.f, 0.75f, 0.4f, 0.9f * Fo), Ang, Sc);
			}
			const float Br = bSel ? 1.f : (K == Equipped ? 0.97f : 0.9f);
			Img(TEXT("frame"), P.X - 112.f, P.Y - 82.5f, 224.f, 165.f, 8, FLinearColor(Br, Br, Br, Fo), Ang, Sc);
			if (Kind == 0)
			{
				// main ring: the illustrated sector (icon carries its own Arabic caption), max 176 x 120
				const FString Ic = FString::Printf(TEXT("m%d"), K);
				const FGhostTex* T = A.Get(*Ic);
				if (T)
				{
					const float Kf = FMath::Min(176.f / T->Size.X, 120.f / T->Size.Y) * Sc, Iw = T->Size.X * Kf, Ih = T->Size.Y * Kf;
					Img(*Ic, P.X - Iw * 0.5f, P.Y - Ih * 0.5f, Iw, Ih, 9, FLinearColor(1.f, 1.f, 1.f, Fo));
				}
			}
			else if (SubItems)
			{
				// sub ring (.wc at 47 %): the item picture (max 150 x 62) over its name (Courier 14 / Amiri), upright
				const FAAARadialSubItem& It = SubItems[K];
				const FVector2f Cc = RingC + Up * (Rad + 4.9f * Sc);
				const FGhostTex* T = A.Get(It.Icon);
				const FString Lbl(It.Label);
				const bool bAr = HasArabic(Lbl);
				const FShapedGlyphSequenceRef Sh = Shape(Lbl, bAr, 14.f * Sc, bAr);
				const float Lw = Width(Sh), Lh = 15.7f * Sc;
				float Iw = 0.f, Ih = 0.f;
				if (T) { const float Kf = FMath::Min(150.f / T->Size.X, 62.f / T->Size.Y) * Sc; Iw = T->Size.X * Kf; Ih = T->Size.Y * Kf; }
				const float Tot = Ih + 2.f * Sc + Lh, Top = Cc.Y - Tot * 0.5f;
				if (T)
				{
					Img(It.Icon, Cc.X - Iw * 0.5f + 2.f * Sc, Top + 3.f * Sc, Iw, Ih, 9, FLinearColor(0.f, 0.f, 0.f, 0.45f * Fo));   // drop-shadow
					Img(It.Icon, Cc.X - Iw * 0.5f, Top, Iw, Ih, 10, FLinearColor(1.f, 1.f, 1.f, Fo));
				}
				const float Ly = Top + Ih + 2.f * Sc - (bAr ? 3.f * Sc : 0.f);
				DrawShaped(Sh, Cc.X - Lw * 0.5f, Ly + 1.f, 10, FLinearColor(1.f, 0.92f, 0.75f, 0.5f * Fo));   // text-shadow 0 1px rgba(255,235,190,.5)
				DrawShaped(Sh, Cc.X - Lw * 0.5f, Ly, 11, Ink.CopyWithNewOpacity(Fo));
			}
		}
		// #ptr (orange triangle, follows the selection with a springy turn) - hidden in the sub rings
		if (Kind == 0)
		{
			const float Ar = FMath::DegreesToRadians(PtrRot);
			const FVector2f P = RingC + FVector2f(FMath::Sin(Ar), -FMath::Cos(Ar)) * PtrR;
			Img(TEXT("ptr_glow"), P.X - 24.f, P.Y - 22.f, 48.f, 44.f, 9, FLinearColor(1.f, 1.f, 1.f, Op), PtrRot);
			Img(TEXT("ptr"), P.X - 14.f, P.Y - 12.f, 28.f, 24.f, 10, FLinearColor(1.f, 1.f, 1.f, Op), PtrRot);
		}
		// #dc: main ring = the name of the selection + the compass rose (turns against the heading);
		//      sub ring = "المجهّز" / "المعدات", the picture of the current item and the pager ‹ 1/1 › ×
		{
			const float Dc = (bClosing ? 1.f : FMath::Clamp((RingT - 0.35f) / 0.4f, 0.f, 1.f)) * Op;
			if (Dc > 0.f)
			{
				if (Kind == 0)
				{
					// the dial's own centre (its crosshair hub is at 320 x 308 of the 640 x 618 art): needle on the hub,
					// the name in the paper band above the inner circle so the needle never covers it
					const FVector2f Hub(RingC.X, RingC.Y - 146.f + 308.f * (290.f / 618.f));
					Text(WheelNames[Sel], true, 19.f, true, RingC.X - 88.f, Hub.Y - 98.f, 176.f, 0.5f, 10, Ink.CopyWithNewOpacity(Dc));
					const float Nl = 128.f;   // needle art: 128 design px across (tips at 57 px from the hub)
					Img(TEXT("needle_shadow"), Hub.X - Nl * 0.5f + 2.5f, Hub.Y - Nl * 0.5f + 3.5f, Nl, Nl, 10, FLinearColor(1.f, 1.f, 1.f, Dc), NeedleRot);
					Img(TEXT("needle"), Hub.X - Nl * 0.5f, Hub.Y - Nl * 0.5f, Nl, Nl, 11, FLinearColor(1.f, 1.f, 1.f, Dc), NeedleRot);
				}
				else if (SubItems)
				{
					const int32 Shown = (Kind == 1 && Equipped >= 0) ? Equipped : Sel;
					const FGhostTex* T = A.Get(SubItems[Shown].Icon);
					float Iw = 0.f, Ih = 0.f;
					if (T) { const float Kf = FMath::Min(150.f / T->Size.X, 66.f / T->Size.Y); Iw = T->Size.X * Kf; Ih = T->Size.Y * Kf; }
					const float Tot = 19.f + 3.f + Ih + 3.f + 22.f;
					const float Top = RingC.Y - Tot * 0.5f;
					Text(Kind == 1 ? TEXT("المجهّز") : (Kind == 3 ? TEXT("الأوامر") : TEXT("المعدات")), true, 19.f, true, RingC.X - 88.f, Top - 6.f, 176.f, 0.5f, 10, Ink.CopyWithNewOpacity(Dc));
					if (T)
					{
						Img(SubItems[Shown].Icon, RingC.X - Iw * 0.5f + 2.f, Top + 22.f + 3.f, Iw, Ih, 10, FLinearColor(0.f, 0.f, 0.f, 0.4f * Dc));
						Img(SubItems[Shown].Icon, RingC.X - Iw * 0.5f, Top + 22.f, Iw, Ih, 11, FLinearColor(1.f, 1.f, 1.f, Dc));
					}
					// #dp: ‹  1/1  ›  ×   (22 px brass-ringed buttons, gap 10)
					static FSlateRoundedBoxBrush Btn(Hex(0x3a2411), 11.f, Hex(0xc5a059), 1.f);
					const FShapedGlyphSequenceRef Pg = Shape(FString::Printf(TEXT("%d/%d"), 1, FMath::Max(1, (SubNum + 7) / 8)), false, 14.f, false);
					const float PgW = Width(Pg), RowW = 22.f * 3.f + PgW + 30.f;
					float Bx = RingC.X - RowW * 0.5f;
					const float By = Top + 22.f + Ih + 3.f;
					const TCHAR* const BtnTxt[3] = { TEXT("‹"), TEXT("›"), TEXT("×") };
					for (int32 Bi = 0; Bi < 4; ++Bi)
					{
						if (Bi == 1)
						{
							DrawShaped(Pg, Bx, By + 2.f, 11, Ink.CopyWithNewOpacity(Dc));
							Bx += PgW + 10.f;
							continue;
						}
						const int32 Ti = Bi == 0 ? 0 : (Bi == 2 ? 1 : 2);
						FSlateDrawElement::MakeBox(Out, LayerId + 10, Geo(Bx, By, 22.f, 22.f), &Btn, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Dc));
						const FShapedGlyphSequenceRef Bt = Shape(BtnTxt[Ti], false, 14.f, false);
						DrawShaped(Bt, Bx + (22.f - Width(Bt)) * 0.5f, By + 2.f, 11, Paper.CopyWithNewOpacity(Dc));
						Bx += 22.f + 10.f;
					}
				}
			}
		}
		Group(0.f, 0.f, 1.f);
	}

	// ---------------------------------------------------------------- toast + key help (outside the canvas: CSS px)
	if (ToastT >= 0.f)
	{
		const float In = FMath::Clamp(ToastT / 0.3f, 0.f, 1.f), Outk = FMath::Clamp((2.1f - ToastT) / 0.3f, 0.f, 1.f), Ta = FMath::Min(In, Outk);
		TSharedPtr<FCompositeFont> F = A.Amiri;
		FSlateFontInfo Fi = F.IsValid() ? FSlateFontInfo(F, 22.f * 0.75f) : FCoreStyle::GetDefaultFontStyle("Bold", 16);
		const FShapedGlyphSequenceRef Sh = FontCache->ShapeBidirectionalText(ToastText, Fi, GeoScale, TextBiDi::ETextDirection::RightToLeft, ETextShapingMethod::Auto);
		const float Tw = (float)Sh->GetMeasuredWidth() / GeoScale, Th = (float)Sh->GetMaxTextHeight() / GeoScale;
		const float Bw = Tw + 44.f, Bh = Th + 16.f, Bx = (View.X - Bw) * 0.5f, By = View.Y - View.Y * 0.04f - Bh + (1.f - In) * 20.f;
		FSlateDrawElement::MakeBox(Out, LayerId + 11, G.ToPaintGeometry(FVector2f(Bw, Bh), FSlateLayoutTransform(FVector2f(Bx, By))), &White, ESlateDrawEffect::None, FLinearColor(20.f / 255.f, 12.f / 255.f, 8.f / 255.f, 0.9f * Ta));
		const FLinearColor Bd = Hex(0x8a5a2a, Ta);
		FSlateDrawElement::MakeBox(Out, LayerId + 12, G.ToPaintGeometry(FVector2f(Bw, 1.f), FSlateLayoutTransform(FVector2f(Bx, By))), &White, ESlateDrawEffect::None, Bd);
		FSlateDrawElement::MakeBox(Out, LayerId + 12, G.ToPaintGeometry(FVector2f(Bw, 1.f), FSlateLayoutTransform(FVector2f(Bx, By + Bh - 1.f))), &White, ESlateDrawEffect::None, Bd);
		FSlateDrawElement::MakeBox(Out, LayerId + 12, G.ToPaintGeometry(FVector2f(1.f, Bh), FSlateLayoutTransform(FVector2f(Bx, By))), &White, ESlateDrawEffect::None, Bd);
		FSlateDrawElement::MakeBox(Out, LayerId + 12, G.ToPaintGeometry(FVector2f(1.f, Bh), FSlateLayoutTransform(FVector2f(Bx + Bw - 1.f, By))), &White, ESlateDrawEffect::None, Bd);
		FSlateDrawElement::MakeShapedText(Out, LayerId + 13, G.ToPaintGeometry(FVector2f(Tw, Th), FSlateLayoutTransform(FVector2f(Bx + 22.f, By + 8.f))), Sh, ESlateDrawEffect::None, Paper.CopyWithNewOpacity(Ta), FLinearColor::Transparent);
	}
	// key help: with the wheel open, else only for the first 25 s of play (then it fades: it ran under the panels)
	const float HelpT = (Ms && Ms->Phase == EAAAMissionPhase::Play) ? Ms->PlayTime : GT;
	const float HelpA = Wp->bRadialMenuOpen ? 0.8f : 0.8f * FMath::Clamp((25.f - HelpT) / 2.f, 0.f, 1.f);
	if (HelpA > 0.01f)
	{
		TSharedPtr<FCompositeFont> F = A.Courier;
		FSlateFontInfo Fi = F.IsValid() ? FSlateFontInfo(F, 12.f * 0.75f) : FCoreStyle::GetDefaultFontStyle("Regular", 9);
		const TCHAR* Help = Wp->bRadialMenuOpen
			? (Wp->RadialKind != 0 ? TEXT("mouse / 1-4 choose · LMB pick · RMB back · release TAB / Q = use") : TEXT("mouse / 1-8 choose · LMB on weapons / equipment = open · release TAB / Q = use"))
			: TEXT("E take · LMB fire · R reload · TAB / Q wheel · MMB order · V follow · Y hold · U suppress · 1-4 select · F1-F3 weapons · N formation · K fire · F first aid");
		const FShapedGlyphSequenceRef Sh = FontCache->ShapeBidirectionalText(Help, Fi, GeoScale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
		const float Tw = (float)Sh->GetMeasuredWidth() / GeoScale, Th = (float)Sh->GetMaxTextHeight() / GeoScale;
		FSlateDrawElement::MakeShapedText(Out, LayerId + 11, G.ToPaintGeometry(FVector2f(Tw, Th), FSlateLayoutTransform(FVector2f((View.X - Tw) * 0.5f, View.Y - 6.f - Th))), Sh, ESlateDrawEffect::None, Hex(0x8c7a5d, HelpA), FLinearColor::Transparent);
	}
	// ---------------------------------------------------------------- chapter 1 (AAAMissionSubsystem): objectives,
	// banner, the marker of where to go, the "E" prompt, and the fade back from the cinematic
	if (Ms && Ms->Phase != EAAAMissionPhase::Off)
	{
		MBars(15);
		// objectives, top right under the compass
		if (!Ms->Objectives.IsEmpty())
		{
			const float Rx = View.X - 30.f * Ks;
			float Y = View.Y * 0.15f;
			TArray<FString> Lines;
			for (const UAAAMissionSubsystem::FObjective& Ob : Ms->Objectives)
				Lines.Add(Ob.Need > 0 ? FString::Printf(TEXT("%s  (%d/%d)"), *Ob.Text, Ob.Have, Ob.Need) : Ob.Text);
			float Wm = MSize(MShape(Ms->ChapterName, 19.f)).X;
			for (const FString& Ln : Lines) Wm = FMath::Max(Wm, MSize(MShape(Ln, 22.f)).X + 22.f * Ks);
			const float H = 34.f * Ks + Lines.Num() * 32.f * Ks + 10.f * Ks;
			MBox(Rx - Wm - 22.f * Ks, Y - 10.f * Ks, Wm + 40.f * Ks, H, 15, FLinearColor(0.04f, 0.025f, 0.015f, 0.62f));
			MBox(Rx + 16.f * Ks, Y - 10.f * Ks, 2.f * Ks, H, 16, Hex(0x8a5a2a, 0.9f));
			MText(Ms->ChapterName, 19.f, Rx, Y, 1.f, 16, Hex(0xc5a059), false);
			Y += 34.f * Ks;
			for (int32 K = 0; K < Lines.Num(); ++K)
			{
				const UAAAMissionSubsystem::FObjective& Ob = Ms->Objectives[K];
				const float Flash = FMath::Clamp(1.f - Ob.T / 0.8f, 0.f, 1.f);
				const FLinearColor Col = Ob.bDone ? Hex(0x6fae4a, 0.8f) : FMath::Lerp(Paper, Hex(0xffe2a0), Flash);
				// a small square (filled when done), then the text right-aligned to its left
				MBox(Rx - 10.f * Ks, Y + 11.f * Ks, 9.f * Ks, 9.f * Ks, 16, Ob.bDone ? Hex(0x6fae4a) : Hex(0xc5a059, 0.9f));
				const FVector2f Sz = MText(Lines[K], 22.f, Rx - 20.f * Ks, Y, 1.f, 16, Col, true);
				if (Ob.bDone) MBox(Rx - 20.f * Ks - Sz.X, Y + Sz.Y * 0.55f, Sz.X, 1.5f * Ks, 18, Hex(0x6fae4a, 0.8f));
				Y += 32.f * Ks;
			}
		}
		// banner in the upper middle
		if (Ms->BannerT >= 0.f)
		{
			const float Al = FMath::Min(FMath::Clamp(Ms->BannerT / 0.35f, 0.f, 1.f), FMath::Clamp((4.5f - Ms->BannerT) / 0.6f, 0.f, 1.f));
			const float Y0 = View.Y * 0.2f + (1.f - FMath::Clamp(Ms->BannerT / 0.35f, 0.f, 1.f)) * 14.f * Ks;
			const FVector2f S1 = MText(Ms->Banner, 48.f, View.X * 0.5f, Y0, 0.5f, 17, Hex(0xc5a059, Al), true);
			MBox(View.X * 0.5f - 140.f * Ks, Y0 + S1.Y + 4.f * Ks, 280.f * Ks, 1.5f * Ks, 17, Hex(0x8a5a2a, Al));
			MText(Ms->BannerSub, 26.f, View.X * 0.5f, Y0 + S1.Y + 14.f * Ks, 0.5f, 17, Paper.CopyWithNewOpacity(Al), true);
		}
		// marker: on screen a gold diamond with its label and distance; off screen the same at the edge, pointing to it
		APlayerController* MPC = Wp->ControllingPC();
		if (Ms->bHasMarker && MPC && MPC->PlayerCameraManager)
		{
			const FVector CamL = MPC->PlayerCameraManager->GetCameraLocation();
			const FRotator CamR = MPC->PlayerCameraManager->GetCameraRotation();
			const FVector Lc = CamR.UnrotateVector(Ms->Marker - CamL);
			FVector2f P;
			FVector2D Sp;
			bool bOn = Lc.X > 10.0 && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(MPC, Ms->Marker, Sp, false);
			// allowed area: an ellipse round the screen centre (36 % of the width, 34 % of the height), so the marker never
			// sits on the corner panels (dossier, team list, health, weapon, objectives); outside it -> on its rim
			const FVector2f Ctr(View.X * 0.5f, View.Y * 0.5f);
			const float Ex = View.X * 0.36f, Ey = View.Y * 0.34f;
			if (bOn)
			{
				P = FVector2f((float)Sp.X, (float)Sp.Y);
				const FVector2f Q = P - Ctr;
				bOn = FMath::Square(Q.X / Ex) + FMath::Square(Q.Y / Ey) <= 1.f;
				if (!bOn) { const float K = 1.f / FMath::Sqrt(FMath::Square(Q.X / Ex) + FMath::Square(Q.Y / Ey)); P = Ctr + Q * K; }
			}
			else
			{
				FVector2f D((float)Lc.Y, (float)-Lc.Z);
				if (D.SizeSquared() < 1.f) D = FVector2f(0.f, 1.f);   // straight behind: at the bottom of the ellipse
				D.Normalize();
				const float K = 1.f / FMath::Sqrt(FMath::Square(D.X / Ex) + FMath::Square(D.Y / Ey));
				P = Ctr + D * K;
			}
			const float R = 16.f * Ks;
			const float Pulse = 0.75f + 0.25f * FMath::Sin((float)Wp->GetWorld()->GetTimeSeconds() * 4.f);
			FSlateDrawElement::MakeBox(Out, LayerId + 16, G.ToPaintGeometry(FVector2f(R, R), FSlateLayoutTransform(P - FVector2f(R, R) * 0.5f), RT(45.f, 1.f), FVector2f(0.5f, 0.5f)), &White, ESlateDrawEffect::None, Hex(0xc5a059, Pulse));
			FSlateDrawElement::MakeBox(Out, LayerId + 17, G.ToPaintGeometry(FVector2f(R * 0.45f, R * 0.45f), FSlateLayoutTransform(P - FVector2f(R, R) * 0.225f), RT(45.f, 1.f), FVector2f(0.5f, 0.5f)), &White, ESlateDrawEffect::None, Hex(0x140c08, 0.9f));
			const APawn* Me = MPC->GetPawn();
			const int32 Meters = Me ? FMath::RoundToInt((float)FVector::Dist(Me->GetActorLocation(), Ms->Marker) / 100.f) : 0;
			MText(FString::Printf(TEXT("%s  %d م"), *Ms->MarkerLabel, Meters), 20.f, P.X, P.Y + R * 0.9f, 0.5f, 16, Paper, true);
		}
		// "E" prompt under the crosshair
		if (!Ms->Prompt.IsEmpty())
		{
			const FVector2f Sz = MSize(MShape(Ms->Prompt, 26.f));
			const float Y = View.Y * 0.62f;
			MBox(View.X * 0.5f - Sz.X * 0.5f - 22.f * Ks, Y - 8.f * Ks, Sz.X + 44.f * Ks, Sz.Y + 16.f * Ks, 16, FLinearColor(20.f / 255.f, 12.f / 255.f, 8.f / 255.f, 0.88f));
			MBox(View.X * 0.5f - Sz.X * 0.5f - 22.f * Ks, Y - 8.f * Ks, Sz.X + 44.f * Ks, 1.f, 17, Hex(0x8a5a2a));
			MText(Ms->Prompt, 26.f, View.X * 0.5f, Y, 0.5f, 17, Paper, false);
		}
	}
	return LayerId + 20;
}
