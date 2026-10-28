// AAA City Weapons - chapter 1 game loop (see AAAMissionSubsystem.h)
#include "AAAMissionSubsystem.h"
#include "AAAWeaponsSubsystem.h"
#include "AAAWeaponComponent.h"
#include "AAAPoliceOfficer.h"
#include "AAAFireTeamTypes.h"
#include "AAAShapes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "AAAMissionAudio.h"
#include "AAARifleAnimInstance.h"
#include "Engine/Engine.h"
#include "Misc/App.h"

static TAutoConsoleVariable<int32> CVarAAAMission(TEXT("aaa.Mission"), 1, TEXT("1 = chapter 1 (intro, briefing, mission) at the start of play, 0 = free roam"));

namespace
{
	const TCHAR* const LeaderName = TEXT("بن بولعيد");
	const FLinearColor LeaderColor(0.77f, 0.63f, 0.35f);

	FString WhoName(int32 Who)
	{
		if (Who < 0) return LeaderName;
		if (Who <= 2) return FAAASquadInfo::Name(Who);
		return TEXT("الجميع");
	}
	FLinearColor WhoColor(int32 Who)
	{
		if (Who < 0) return LeaderColor;
		if (Who <= 2) return FAAASquadInfo::Color(Who);
		return FLinearColor(0.92f, 0.88f, 0.80f);
	}
}

/* ============================================================================================ pickup */

AAAAMissionPickup::AAAAMissionPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* R = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	R->SetMobility(EComponentMobility::Movable);
	SetRootComponent(R);
	Spin = CreateDefaultSubobject<USceneComponent>(TEXT("Spin"));
	Spin->SetupAttachment(R);
	Spin->SetMobility(EComponentMobility::Movable);
}

void AAAAMissionPickup::Build(uint8 InKind)
{
	Kind = InKind;
	auto Add = [&](USceneComponent* Parent, bool bCyl, const FVector& Size, const FVector& Loc, const FRotator& Rot, UMaterialInterface* M, bool bShadow = true)
	{
		UStaticMeshComponent* C = AAAShapes::Make(this, bCyl, Size, M, false, bShadow);
		C->SetupAttachment(Parent);
		C->SetRelativeLocationAndRotation(Loc, Rot);
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	UMaterialInterface* Wood = AAAShapes::Mat(this, FLinearColor(0.20f, 0.10f, 0.045f));
	UMaterialInterface* Steel = AAAShapes::Mat(this, FLinearColor(0.035f, 0.035f, 0.04f));
	UMaterialInterface* Olive = AAAShapes::Mat(this, FLinearColor(0.10f, 0.11f, 0.05f));
	UMaterialInterface* Glow = AAAShapes::Mat(this, FLinearColor(1.f, 0.72f, 0.28f) * 3.f, true);
	USceneComponent* Root = GetRootComponent();
	if (Kind == 0)
	{
		// a rifle lying on the ground (stock, receiver, barrel) and its ammo box
		Add(Root, false, FVector(32.f, 5.f, 11.f), FVector(-34.f, 0.f, 4.f), FRotator(0.f, 0.f, 90.f), Wood);
		Add(Root, false, FVector(46.f, 5.f, 7.f), FVector(4.f, 0.f, 3.5f), FRotator(0.f, 0.f, 90.f), Steel);
		Add(Root, false, FVector(34.f, 4.f, 4.f), FVector(10.f, 0.f, 2.5f), FRotator(0.f, 0.f, 90.f), Wood);
		Add(Root, true, FVector(2.4f, 2.4f, 40.f), FVector(46.f, 0.f, 3.f), FRotator(90.f, 0.f, 0.f), Steel);
		Add(Root, false, FVector(24.f, 13.f, 14.f), FVector(0.f, 28.f, 7.f), FRotator(0.f, 25.f, 0.f), Olive);
		Add(Root, true, FVector(110.f, 110.f, 0.6f), FVector(0.f, 8.f, 0.6f), FRotator::ZeroRotator, Glow, false)->SetRenderCustomDepth(false);
	}
	else
	{
		// the patrol's ammo crate: a wooden box, its lid boards, two ammo boxes on top
		Add(Root, false, FVector(96.f, 62.f, 52.f), FVector(0.f, 0.f, 26.f), FRotator::ZeroRotator, Wood);
		Add(Root, false, FVector(98.f, 8.f, 4.f), FVector(0.f, -20.f, 53.f), FRotator::ZeroRotator, Steel);
		Add(Root, false, FVector(98.f, 8.f, 4.f), FVector(0.f, 20.f, 53.f), FRotator::ZeroRotator, Steel);
		Add(Root, false, FVector(26.f, 14.f, 16.f), FVector(-20.f, 0.f, 63.f), FRotator(0.f, 10.f, 0.f), Olive);
		Add(Root, false, FVector(26.f, 14.f, 16.f), FVector(16.f, 4.f, 63.f), FRotator(0.f, -15.f, 0.f), Olive);
		Add(Root, true, FVector(190.f, 190.f, 0.6f), FVector(0.f, 0.f, 0.6f), FRotator::ZeroRotator, Glow, false);
	}
	// a small glowing diamond turning above it (seen from afar)
	Spin->SetRelativeLocation(FVector(0.f, 0.f, Kind == 0 ? 70.f : 120.f));
	Add(Spin, false, FVector(12.f, 12.f, 12.f), FVector::ZeroVector, FRotator(45.f, 0.f, 45.f), Glow, false);
}

void AAAAMissionPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	T += DeltaSeconds;
	if (Spin) Spin->SetRelativeLocationAndRotation(FVector(0.f, 0.f, (Kind == 0 ? 70.f : 120.f) + 6.f * FMath::Sin(T * 2.4f)), FRotator(0.f, T * 90.f, 0.f));
}

void AAAAMissionPickup::Take()
{
	bTaken = true;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetLifeSpan(0.5f);
}

/* ============================================================================================ the loop */

void UAAAMissionSubsystem::Deinitialize()
{
	if (bDebugMsgsSaved && GEngine) GEngine->bEnableOnScreenDebugMessages = bDebugMsgsWere;
	StopMusic(0.f);
	if (Cam.IsValid()) Cam->Destroy();
	Super::Deinitialize();
}

bool UAAAMissionSubsystem::Ground(const FVector& Around, FVector& Out, double StartUp) const
{
	UWorld* W = GetWorld();
	if (!W) return false;
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAMissionGround), false);
	if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& E : Enemies) if (E.IsValid()) Q.AddIgnoredActor(E.Get());
	if (!W->LineTraceSingleByChannel(H, Around + FVector(0.0, 0.0, StartUp), Around - FVector(0.0, 0.0, 1500.0), ECC_Visibility, Q)) return false;
	if (H.ImpactNormal.Z < 0.7) return false;
	Out = H.ImpactPoint;
	return true;
}

bool UAAAMissionSubsystem::StreetSpot(const FVector& Want, double RefZ, FVector& Out) const
{
	UWorld* W = GetWorld();
	if (!W) return false;
	// look from just above street level (never from above the roofs), the floor must be the street's (+-2.2 m)
	FVector G;
	if (!Ground(FVector(Want.X, Want.Y, RefZ), G, 150.0)) return false;
	if (FMath::Abs(G.Z - RefZ) > 220.0) return false;
	const FVector C = G + FVector(0.0, 0.0, 95.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAMissionSpot), false);
	if (W->OverlapAnyTestByChannel(C, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(36.f, 85.f), Q)) return false;   // inside a wall / a car
	Out = C;
	return true;
}

AActor* UAAAMissionSubsystem::Actor(int32 Who) const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (Who < 0) return PC ? PC->GetPawn() : nullptr;
	if (Who <= 2) if (UAAAWeaponsSubsystem* Ws = W ? W->GetSubsystem<UAAAWeaponsSubsystem>() : nullptr) return Ws->GetSquadMember(Who);
	return nullptr;
}

void UAAAMissionSubsystem::LockInput(APlayerController* PC, bool bLock)
{
	if (!PC || bLock == bInputLocked) return;
	bInputLocked = bLock;
	PC->SetIgnoreMoveInput(bLock);   // (the weapon component also stops reading its keys: bInputOk)
	PC->SetIgnoreLookInput(bLock);
}

void UAAAMissionSubsystem::ViewCinematic(APlayerController* PC)
{
	UWorld* W = GetWorld();
	if (!PC || !W) return;
	if (!Cam.IsValid())
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Sp);
		if (Cam.IsValid())
		{
			Cam->GetCameraComponent()->SetFieldOfView(42.f);
			Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
	}
	ShotWho = -99;
	if (Cam.IsValid()) PC->SetViewTargetWithBlend(Cam.Get(), 0.f);
}

void UAAAMissionSubsystem::ViewGame(APlayerController* PC, float Blend)
{
	if (PC && PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), Blend, VTBlend_EaseInOut, 2.f);
}

void UAAAMissionSubsystem::TickCamera(float Dt, APlayerController* PC, bool bWide)
{
	ACameraActor* C = Cam.Get();
	AActor* Lead = Actor(-1);
	if (!C || !Lead) return;
	// who is speaking now (a close shot on him), else the whole group
	int32 Who = 3;
	if (!bWide && Beats.IsValidIndex(BeatIdx) && Beats[BeatIdx].Type == 2) Who = Beats[BeatIdx].Who;
	const bool bAerial = Beats.IsValidIndex(BeatIdx) && Beats[BeatIdx].Type == 3;
	AActor* Subject = (Who >= -1 && Who <= 2) ? Actor(Who) : nullptr;
	if (!Subject) Who = 3;
	if (bAerial) Who = 4;
	const bool bCut = Who != ShotWho;
	if (bCut) { ShotWho = Who; ShotT = 0.f; }
	ShotT += Dt;

	const FVector Up(0.0, 0.0, 1.0);
	FVector Pos, Look;
	if (Who == 4)
	{
		// establishing shot: from high over the street, descending and closing on the men (the beat's length)
		FVector Ctr = Lead->GetActorLocation();
		int32 N = 1;
		for (int32 I = 0; I < 3; ++I) if (AActor* M = Actor(I)) { Ctr += M->GetActorLocation(); ++N; }
		Ctr /= (double)N;
		const float K = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(BeatT / FMath::Max(Beats[BeatIdx].Dur, 0.1f), 0.f, 1.f));
		const FRotator Yaw(0.0, Lead->GetActorRotation().Yaw + 18.0 + K * 14.0, 0.0);
		Pos = Ctr + Yaw.RotateVector(FVector(FMath::Lerp(2600.0, 900.0, (double)K), 300.0, 0.0)) + Up * FMath::Lerp(1300.0, 160.0, (double)K);
		Look = Ctr + Up * FMath::Lerp(0.0, 35.0, (double)K);
	}
	else if (Who == 3)
	{
		// wide: the four men, a slow push in and a slight orbit
		FVector Ctr = Lead->GetActorLocation();
		int32 N = 1;
		for (int32 I = 0; I < 3; ++I) if (AActor* M = Actor(I)) { Ctr += M->GetActorLocation(); ++N; }
		Ctr /= (double)N;
		const FRotator Yaw(0.0, Lead->GetActorRotation().Yaw + 18.0 + ShotT * 1.6, 0.0);
		const double Dist = 760.0 - 110.0 * FMath::Min(ShotT / 9.f, 1.f);
		Pos = Ctr + Yaw.RotateVector(FVector(Dist, 120.0, 0.0)) + Up * 120.0;
		Look = Ctr + Up * 35.0;
	}
	else
	{
		// close: the speaker's face, from the front, a little to one side, pushing in while he talks
		const FVector Head = Subject->GetActorLocation() + Up * 68.0;
		const FRotator Yaw(0.0, Subject->GetActorRotation().Yaw, 0.0);
		const double Side = (Who % 2 == 0) ? 1.0 : -1.0;
		const double Dist = 250.0 - 40.0 * FMath::Min(ShotT / 4.f, 1.f);
		Pos = Head + Yaw.RotateVector(FVector(Dist, 70.0 * Side, -6.0));
		Look = Head + Yaw.RotateVector(FVector(0.0, 12.0 * Side, -4.0));
	}
	// never inside a wall: pull the camera in front of what is between it and the subject
	if (UWorld* W = GetWorld())
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAMissionCam), false);
		Q.AddIgnoredActor(Lead);
		for (int32 I = 0; I < 3; ++I) if (AActor* M = Actor(I)) Q.AddIgnoredActor(M);
		if (W->SweepSingleByChannel(H, Look, Pos, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q)) Pos = H.Location;
	}
	CamLook(C, Pos, Look, Dt, bCut || Who == 4, Who == 4 ? 0.15f : 0.35f);
}

void UAAAMissionSubsystem::CamLook(ACameraActor* C, const FVector& Pos, const FVector& Look, float Dt, bool bCut, float NoiseDeg)
{
	// hand-held: a slow, small drift of the aim (Perlin), and the focus on the subject (shallow depth of field)
	CamNoiseT += Dt;
	FRotator Rot = (Look - Pos).Rotation();
	Rot.Pitch += NoiseDeg * FMath::PerlinNoise1D(CamNoiseT * 0.45f);
	Rot.Yaw += NoiseDeg * FMath::PerlinNoise1D(CamNoiseT * 0.37f + 19.f);
	if (bCut) C->SetActorLocationAndRotation(Pos, Rot);
	else C->SetActorLocationAndRotation(FMath::VInterpTo(C->GetActorLocation(), Pos, Dt, 5.f), FMath::RInterpTo(C->GetActorRotation(), Rot, Dt, 6.f));
	if (UCameraComponent* Cc = C->GetCameraComponent())
	{
		FPostProcessSettings& P = Cc->PostProcessSettings;
		Cc->PostProcessBlendWeight = 1.f;
		P.bOverride_DepthOfFieldFocalDistance = true;
		P.DepthOfFieldFocalDistance = (float)FVector::Dist(C->GetActorLocation(), Look);
		P.bOverride_DepthOfFieldFstop = true;
		P.DepthOfFieldFstop = 2.2f;
		P.bOverride_DepthOfFieldSensorWidth = true;
		P.DepthOfFieldSensorWidth = 36.f;
	}
}

void UAAAMissionSubsystem::StartBeats(const TArray<FBeat>& InBeats)
{
	Beats = InBeats;
	BeatIdx = 0;
	BeatT = 0.f;
}

bool UAAAMissionSubsystem::TickBeats(float Dt, APlayerController* PC)
{
	auto Clear = [this]() { CardT = -1.f; CardAlpha = 0.f; LineT = -1.f; };
	if (!Beats.IsValidIndex(BeatIdx)) { Clear(); return false; }
	BeatT += Dt;
	bool bNext = BeatT >= Beats[BeatIdx].Dur;
	if (PC)
	{
		// Enter / Space / click: next line; Backspace: skip the whole scene
		if (BeatT > 0.5f && (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))) bNext = true;
		if (PC->WasInputKeyJustPressed(EKeys::BackSpace)) { BeatIdx = Beats.Num(); Clear(); return false; }
	}
	if (bNext) { ++BeatIdx; BeatT = 0.f; }
	if (!Beats.IsValidIndex(BeatIdx)) { Clear(); return false; }
	const FBeat& B = Beats[BeatIdx];
	if (B.Type == 3) { CardT = -1.f; CardAlpha = 0.f; LineT = -1.f; return true; }   // establishing shot: no words
	if (B.Type <= 1)
	{
		CardLine1 = B.A; CardLine2 = B.B; bCardTitle = B.Type == 1;
		CardT = BeatT;
		CardAlpha = FMath::Clamp(FMath::Min(BeatT / 0.7f, (B.Dur - BeatT) / 0.6f), 0.f, 1.f);
		LineT = -1.f;
	}
	else
	{
		Speaker = WhoName(B.Who); SpeakerColor = WhoColor(B.Who); Line = B.A;
		LineT = BeatT;
		CardT = -1.f; CardAlpha = 0.f;
	}
	return true;
}

void UAAAMissionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W) return;
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	PhaseT += Dt;
	if (BannerT >= 0.f) { BannerT += Dt; if (BannerT > 4.5f) BannerT = -1.f; }
	for (FObjective& O : Objectives) O.T += Dt;

	APlayerController* PC = W->GetFirstPlayerController();
	ACharacter* L = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UAAAWeaponsSubsystem* Ws = W->GetSubsystem<UAAAWeaponsSubsystem>();
	UAAAWeaponComponent* Wp = L ? L->FindComponentByClass<UAAAWeaponComponent>() : nullptr;

	TickPolish(Dt);

	// the hero's death (whatever the phase): the mission waits while the scene plays
	if ((Wp && Wp->IsDead()) || bDeathScene)
	{
		TickDeathScene(Dt, PC, L, Wp);
		if (bDeathScene) { Prompt.Reset(); bHasMarker = false; return; }
	}
	if (Phase == EAAAMissionPhase::Off || Phase == EAAAMissionPhase::Done)
	{
		Black = FMath::Max(0.f, Black - Dt); Letterbox = FMath::Max(0.f, Letterbox - Dt / 0.8f);
		return;
	}
	auto Card = [](const TCHAR* A, const TCHAR* B, float Dur) { FBeat X; X.Type = 0; X.A = A; X.B = B; X.Dur = Dur; return X; };
	auto Title = [](const TCHAR* A, const TCHAR* B, float Dur) { FBeat X; X.Type = 1; X.A = A; X.B = B; X.Dur = Dur; return X; };
	auto Say = [](int32 Who, const TCHAR* A, float Dur) { FBeat X; X.Type = 2; X.Who = Who; X.A = A; X.Dur = Dur; return X; };

	switch (Phase)
	{
	case EAAAMissionPhase::Boot:
		if (CVarAAAMission.GetValueOnGameThread() == 0) { Phase = EAAAMissionPhase::Off; Black = 0.f; Letterbox = 0.f; return; }
		Black = 1.f; Letterbox = 1.f;
		LockInput(PC, true);
		if (Ws) Ws->bEnemyPatrols = false;   // no random patrol during the intro: the mission brings its own
		if (PhaseT > 0.4f && PC)
		{
			Phase = EAAAMissionPhase::Intro; PhaseT = 0.f;
			StartMusic(0.55f);
			ChapterName = TEXT("الفصل الأول — السلاح من يد العدو");
			StartBeats({
				Card(TEXT("الجزائر — ليلة الفاتح من نوفمبر 1954"), TEXT(""), 4.2f),
				Card(TEXT("مئة وأربعة وعشرون عامًا تحت الاستعمار الفرنسي."), TEXT("أرضٌ مسلوبة، وشعبٌ بلا صوت."), 5.0f),
				Card(TEXT("في هذه الليلة، قرّر أبناء الشعب أن يحملوا السلاح."), TEXT("لكنّ السلاح قليل… وهو عند العدو."), 5.2f),
				Title(TEXT("أشباح نوفمبر"), TEXT("الفصل الأول — السلاح من يد العدو"), 5.0f) });
			if (Wp) { Wp->SwitchWeapon(EAAAWeaponType::Pistol); }
		}
		break;

	case EAAAMissionPhase::Intro:
		Black = 1.f; Letterbox = 1.f;
		LockInput(PC, true);
		if (!TickBeats(Dt, PC))
		{
			// the briefing needs the men (they join about 2 s after the start)
			const bool bMen = Ws && Ws->GetSquadMember(0) && Ws->GetSquadMember(1) && Ws->GetSquadMember(2);
			if (L && (bMen || PhaseT > 60.f))
			{
				Phase = EAAAMissionPhase::Briefing; PhaseT = 0.f;
				ViewCinematic(PC);
				FBeat Est; Est.Type = 3; Est.Dur = 5.f;   // the establishing shot first (the music carries it)
				StartBeats({
					Est,
					Say(-1, TEXT("يا رجال، الليلة تبدأ الثورة. لكنّ سلاحنا لا يكفي."), 4.2f),
					Say(0, TEXT("دورية فرنسية تحرس الساحة القريبة. ستة جنود."), 4.0f),
					Say(1, TEXT("معهم بنادق، وصندوق ذخيرة في موقعهم."), 3.8f),
					Say(2, TEXT("نضربهم بسرعة، ونأخذ كلّ ما يحملون."), 3.8f),
					Say(-1, TEXT("اتبعوني، ولا تطلقوا قبل أن أطلق. السلاح الذي نحتاجه بين أيديهم."), 4.8f),
					Say(3, TEXT("تحيا الجزائر!"), 2.6f) });
			}
		}
		break;

	case EAAAMissionPhase::Briefing:
		Black = FMath::Max(0.f, Black - Dt / 1.4f); Letterbox = 1.f;
		LockInput(PC, true);
		TickCamera(Dt, PC, false);
		if (!TickBeats(Dt, PC))
		{
			Phase = EAAAMissionPhase::Play; PhaseT = 0.f;
			ViewGame(PC, 1.2f);
			StopMusic(4.f);
			for (int32 Wk = -1; Wk <= 2; ++Wk) SetTalk(Wk, -1.f);
			LockInput(PC, false);
			if (L) BeginPlayPhase(L, Wp, Ws);
		}
		break;

	case EAAAMissionPhase::Play:
		Black = FMath::Max(0.f, Black - Dt); Letterbox = FMath::Max(0.f, Letterbox - Dt / 0.8f);
		if (L) TickPlay(Dt, L, Wp, Ws, PC);
		break;

	case EAAAMissionPhase::Outro:
	{
		Letterbox = FMath::Min(1.f, Letterbox + Dt / 0.8f);
		LockInput(PC, true);
		const bool bCardNow = Beats.IsValidIndex(BeatIdx) && Beats[BeatIdx].Type <= 1;
		Black = FMath::FInterpTo(Black, bCardNow ? 1.f : 0.f, Dt, 3.f);
		TickCamera(Dt, PC, false);
		if (!TickBeats(Dt, PC))
		{
			EndMission(Wp, Ws);
			Phase = EAAAMissionPhase::Done; PhaseT = 0.f;
			StopMusic(3.f);
			for (int32 Wk = -1; Wk <= 2; ++Wk) SetTalk(Wk, -1.f);
			Black = 0.f; Letterbox = 0.f;
			ViewGame(PC, 0.f);
			LockInput(PC, false);
			SetBanner(TEXT("اللعب الحر"), TEXT("المدينة مفتوحة أمامك"));
		}
		break;
	}
	default: break;
	}
	if (Phase != EAAAMissionPhase::Play) { Prompt.Reset(); bHasMarker = false; }
}

void UAAAMissionSubsystem::BeginPlayPhase(ACharacter* L, UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws)
{
	UWorld* W = GetWorld();
	Rally = L->GetActorLocation();
	// the mission's own patrol replaces the random ones
	if (Ws) Ws->bEnemyPatrols = false;
	for (TActorIterator<AAAAPoliceOfficer> It(W); It; ++It) if (It->Team == AAATeam::Enemy && !It->IsDead()) It->Destroy();

	// their post: 55..85 m away, out of sight, ON THE STREET (the same level as the rally point: never on a roof)
	FVector RallyG = Rally - FVector(0.0, 0.0, 95.0);
	Ground(Rally, RallyG, 50.0);
	const double StreetZ = RallyG.Z;
	bool bCamp = false;
	for (int32 Try = 0; Try < 12 && Ws && !bCamp; ++Try)
	{
		FVector C;
		if (Ws->FindHiddenSpot(Rally, 5500.f, 8500.f, C) && StreetSpot(C, StreetZ, C)) { Camp = C; bCamp = true; }
	}
	for (int32 Try = 0; Try < 16 && !bCamp; ++Try)
	{
		const FVector Dir = FRotator(0.0, L->GetActorRotation().Yaw + Try * 22.5, 0.0).Vector();
		FVector C;
		if (StreetSpot(Rally + Dir * 6000.0, StreetZ, C)) { Camp = C; bCamp = true; }
	}
	if (!bCamp) Camp = Rally + L->GetActorForwardVector() * 3000.0;
	// three guards around the crate, three men walking a beat round the post
	Enemies.Reset(); Counted.Reset(); Pickups.Reset();
	Kills = 0; RiflesTaken = 0; AmmoTaken = 0; PlayTime = 0.f; bCrateTaken = false; bReinforced = false; bLowAmmoSaid = false;
	for (int32 I = 0; I < 6 && Ws; ++I)
	{
		const int32 Grp = I / 3, Ix = I % 3;
		const float A = (Ix * 120.f + Grp * 60.f) * UE_PI / 180.f;
		const double R = Grp == 0 ? 320.0 : 1500.0;
		// a free street spot near the wanted one (turned / pulled in until one fits)
		FVector P = Camp;
		bool bOk = false;
		for (int32 Try = 0; Try < 12 && !bOk; ++Try)
		{
			const float At = A + (Try % 2 ? 1.f : -1.f) * (Try / 2) * 0.35f;
			const double Rt = R * (1.0 - 0.12 * (Try / 4));
			bOk = StreetSpot(Camp + FVector(FMath::Cos(At) * Rt, FMath::Sin(At) * Rt, 0.0), Camp.Z - 95.0, P);
		}
		if (!bOk) P = Camp + FVector(FMath::Cos(A) * 150.0, FMath::Sin(A) * 150.0, 0.0);
		AAAAPoliceOfficer* E = Ws->SpawnSoldier(AAATeam::Enemy, P, FMath::RadiansToDegrees(A), Ix);
		if (!E) continue;
		TArray<FVector> Beat;
		const double Br = Grp == 0 ? 450.0 : 2200.0;
		for (int32 K = 0; K < 4; ++K)
		{
			const float Ak = A + K * UE_HALF_PI;
			FVector Bp;
			if (StreetSpot(Camp + FVector(FMath::Cos(Ak) * Br, FMath::Sin(Ak) * Br, 0.0), Camp.Z - 95.0, Bp)) Beat.Add(Bp);   // (points on roofs / in walls left out)
		}
		if (Beat.Num() < 2)
		{
			Beat.Reset();
			Beat.Add(Camp); Beat.Add(P);
		}
		E->SetPatrol(Beat);
		Enemies.Add(E);
	}
	TotalEnemies = FMath::Max(Enemies.Num(), 1);
	// the ammo crate at the post
	{
		FVector G = Camp - FVector(0.0, 0.0, 95.0);
		Ground(Camp, G, 60.0);
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AAAAMissionPickup* Cr = W->SpawnActor<AAAAMissionPickup>(AAAAMissionPickup::StaticClass(), FTransform(FRotator(0.0, FMath::FRandRange(0.f, 90.f), 0.0), G), Sp))
		{
			Cr->Build(1);
			Pickups.Add(Cr);
		}
	}
	Objectives.Reset();
	{ FObjective O; O.Text = TEXT("اقضِ على الدورية الفرنسية"); O.Need = TotalEnemies; Objectives.Add(O); }
	{ FObjective O; O.Text = TEXT("اغنم البنادق وصندوق الذخيرة"); O.Need = TotalEnemies + 1; Objectives.Add(O); }
	SetBanner(TEXT("هدف جديد"), TEXT("اقضِ على الدورية واغنم سلاحها"));
	// a resistance fighter: a pistol and two magazines - the rest has to be taken from the enemy
	if (Wp)
	{
		bSavedInfinite = Wp->bInfiniteReserve;
		Wp->bInfiniteReserve = false;
		Wp->ReserveAmmo = 16;
		Wp->SwitchWeapon(EAAAWeaponType::Pistol);
		Wp->SetArmed(true);
	}
	if (Ws) Ws->SayLeader(TEXT("الدورية أمامنا. اتبعوني بهدوء."));
}

void UAAAMissionSubsystem::TickPlay(float Dt, ACharacter* L, UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws, APlayerController* PC)
{
	UWorld* W = GetWorld();
	PlayTime += Dt;
	SayT -= Dt;

	// the leader fell: a short card, the weapon component brings him back at the start; the mission goes on
	const bool bDead = Wp && Wp->IsDead();   // (his death itself is the shahada scene: TickDeathScene)
	bWasDead = bDead;

	// enemies down: count them, each leaves his rifle and ammo where he fell
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& E : Enemies)
	{
		AAAAPoliceOfficer* X = E.Get();
		const bool bDown = !X || X->IsOut();
		if (!bDown || Counted.Contains(E)) continue;
		Counted.Add(E);
		++Kills;
		if (X)
		{
			FVector Body = X->GetActorLocation();
			if (const USkeletalMeshComponent* Sk = X->GetMesh()) Body = Sk->GetSocketLocation(TEXT("pelvis"));   // (ragdoll: where the body really lies)
			FVector G = Body - FVector(0.0, 0.0, 90.0);
			if (!Ground(Body, G, 40.0)) { FVector G2; if (Ground(Body, G2, 150.0)) G = G2; }
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AAAAMissionPickup* P = W->SpawnActor<AAAAMissionPickup>(AAAAMissionPickup::StaticClass(), FTransform(FRotator(0.0, FMath::FRandRange(0.f, 360.f), 0.0), G + FVector(60.0, 0.0, 0.0)), Sp))
			{
				P->Build(0);
				Pickups.Add(P);
			}
		}
		if (Ws && SayT <= 0.f)
		{
			SayT = 6.f;
			static const TCHAR* const Lines[3] = { TEXT("سقط واحد! خذ سلاحه."), TEXT("أصبناه! بندقيته لنا."), TEXT("واحد آخر! اجمعوا الذخيرة.") };
			for (int32 I = 0; I < 3; ++I)
				if (AAAAPoliceOfficer* M = Ws->GetSquadMember((Kills + I) % 3)) if (!M->IsOut()) { Ws->Say(M, Lines[Kills % 3]); break; }
		}
	}
	// reinforcements: when four of the patrol are down, two more come running from 30..50 m (they heard it on the radio)
	if (!bReinforced && Kills >= 4 && Ws)
	{
		bReinforced = true;
		for (int32 I = 0; I < 2; ++I)
		{
			FVector P;
			bool bOk = false;
			for (int32 Try = 0; Try < 10 && !bOk; ++Try) bOk = Ws->FindHiddenSpot(Camp, 3000.f, 5000.f, P) && StreetSpot(P, Camp.Z - 95.0, P);
			if (!bOk) continue;
			if (AAAAPoliceOfficer* E = Ws->SpawnSoldier(AAATeam::Enemy, P, (float)(L->GetActorLocation() - P).Rotation().Yaw, I))
			{
				E->RadioIntel(L->GetActorLocation());
				Enemies.Add(E);
			}
		}
		const int32 Added = Enemies.Num() - TotalEnemies;
		if (Added > 0)
		{
			TotalEnemies += Added;
			if (Objectives.Num() >= 2) { Objectives[0].Need += Added; Objectives[1].Need += Added; Objectives[0].T = 0.f; }
			SetBanner(TEXT("تعزيزات العدو!"), TEXT("جنديان آخران قادمان"));
			for (int32 I = 0; I < 3; ++I) if (AAAAPoliceOfficer* M = Ws->GetSquadMember(I)) if (!M->IsOut()) { Ws->Say(M, TEXT("تعزيزات فرنسية! انتبهوا!")); break; }
		}
	}
	// little ammo left: a hint once (again after he has more)
	if (Wp)
	{
		const int32 Left = Wp->Ammo + Wp->ReserveAmmo;
		if (!bLowAmmoSaid && Left <= 8) { bLowAmmoSaid = true; Wp->RadialAction(TEXT("الذخيرة قليلة — اغنمها من العدو")); }
		else if (Left > 20) bLowAmmoSaid = false;
	}
	if (Objectives.Num() >= 2)
	{
		FObjective& O0 = Objectives[0];
		O0.Have = FMath::Min(Kills, O0.Need);
		if (!O0.bDone && O0.Have >= O0.Need)
		{
			O0.bDone = true; O0.T = 0.f;
			SetBanner(TEXT("تمّ القضاء على الدورية"), TEXT("اجمع السلاح والذخيرة"));
			if (Ws) Ws->SayLeader(TEXT("المكان لنا! خذوا كلّ شيء."));
		}
	}

	// pickups: E near one
	AAAAMissionPickup* Near = nullptr;
	double NearD = 190.0;
	for (const TWeakObjectPtr<AAAAMissionPickup>& P : Pickups)
	{
		if (!P.IsValid() || P->bTaken) continue;
		const double D = FVector::Dist2D(P->GetActorLocation(), L->GetActorLocation());
		if (D < NearD && FMath::Abs(P->GetActorLocation().Z - L->GetActorLocation().Z) < 200.0) { NearD = D; Near = P.Get(); }
	}
	if (Near && !bDead)
	{
		Prompt = Near->Kind == 0 ? TEXT("E — خذ البندقية والذخيرة") : TEXT("E — افتح صندوق الذخيرة");
		if (PC && PC->WasInputKeyJustPressed(EKeys::E))
		{
			if (Near->Kind == 0)
			{
				++RiflesTaken; AmmoTaken += 30;
				if (Wp)
				{
					Wp->ReserveAmmo += 30;
					if (RiflesTaken == 1) { Wp->SwitchWeapon(EAAAWeaponType::Rifle); Wp->RadialAction(TEXT("غنمت بندقية! +30 طلقة")); }
					else Wp->RadialAction(TEXT("بندقية أخرى: +30 طلقة"));
				}
			}
			else
			{
				bCrateTaken = true; AmmoTaken += 120;
				if (Wp) { Wp->ReserveAmmo += 120; Wp->RadialAction(TEXT("صندوق الذخيرة: +120 طلقة، والرجال تزوّدوا")); }
				if (Ws) Ws->ResupplySquad(L->GetActorLocation());
			}
			Near->Take();
			Sfx((uint8)AAAMissionAudio::ESnd::Pickup, 0.9f, Near->Kind == 0 ? 1.f : 0.8f);
			Prompt.Reset();
		}
	}
	else Prompt.Reset();

	// weapons: only what has been taken from the enemy (the pistol always; the rifle after the first one is picked
	// up; the machine gun from the crate). Switching to a weapon not taken yet falls back to the pistol.
	if (Wp && Wp->bArmed)
	{
		const EAAAWeaponType Ty = Wp->CurrentWeaponType;
		const bool bOk = Ty == EAAAWeaponType::Pistol || (Ty == EAAAWeaponType::Rifle && RiflesTaken > 0) || (Ty == EAAAWeaponType::LMG && bCrateTaken);
		if (!bOk) { Wp->SwitchWeapon(RiflesTaken > 0 ? EAAAWeaponType::Rifle : EAAAWeaponType::Pistol); Wp->RadialAction(TEXT("لم تغنم هذا السلاح بعد")); }
	}

	int32 Open = 0;
	for (const TWeakObjectPtr<AAAAMissionPickup>& P : Pickups) if (P.IsValid() && !P->bTaken) ++Open;
	const int32 Taken = RiflesTaken + (bCrateTaken ? 1 : 0);
	if (Objectives.Num() >= 2)
	{
		FObjective& O1 = Objectives[1];
		O1.Have = FMath::Min(Taken, O1.Need);
		if (!O1.bDone && Objectives[0].bDone && Open == 0)
		{
			O1.bDone = true; O1.T = 0.f;
			FObjective O; O.Text = TEXT("عُد إلى نقطة التجمّع"); O.Need = 0; Objectives.Add(O);
			SetBanner(TEXT("هدف جديد"), TEXT("عُد بالسلاح إلى نقطة التجمّع"));
			if (Ws) Ws->SayLeader(TEXT("يكفي. نعود إلى نقطة التجمّع قبل وصول تعزيزاتهم."));
		}
	}

	// the marker: the post while the patrol stands, then the nearest rifle left, then the rally point
	bHasMarker = true;
	if (Objectives.Num() >= 3)
	{
		Marker = Rally; MarkerLabel = TEXT("نقطة التجمّع");
		if (FVector::Dist2D(L->GetActorLocation(), Rally) < 450.0 && !bDead)
		{
			Objectives[2].bDone = true; Objectives[2].T = 0.f;
			Phase = EAAAMissionPhase::Outro; PhaseT = 0.f;
			Sfx((uint8)AAAMissionAudio::ESnd::Complete, 0.8f);
			StartMusic(0.4f);
			bHasMarker = false;
			ViewCinematic(PC);
			const int32 Mins = FMath::FloorToInt(PlayTime / 60.f), Secs = FMath::FloorToInt(PlayTime) % 60;
			const FString Stats = FString::Printf(TEXT("العدو: %d  ·  بنادق: %d  ·  طلقات: %d  ·  الزمن: %d:%02d"), Kills, RiflesTaken, AmmoTaken, Mins, Secs);
			FBeat A; A.Type = 2; A.Who = 0; A.A = TEXT("غنمنا السلاح والذخيرة. الآن نستطيع أن نقاتل."); A.Dur = 4.2f;
			FBeat B; B.Type = 2; B.Who = -1; B.A = TEXT("هذه أول خطوة فقط. الطريق إلى الحرية طويل… لكنّنا بدأناه."); B.Dur = 5.0f;
			FBeat C; C.Type = 0; C.A = TEXT("تمّت المهمة"); C.B = Stats; C.Dur = 5.5f;
			FBeat D; D.Type = 1; D.A = TEXT("نهاية الفصل الأول"); D.B = TEXT("يتبع…"); D.Dur = 5.0f;
			StartBeats({ A, B, C, D });
			return;
		}
	}
	else if (!Objectives.IsEmpty() && !Objectives[0].bDone)
	{
		Marker = Camp; MarkerLabel = TEXT("الدورية");
	}
	else
	{
		double Best = 1e12;
		bHasMarker = false;
		for (const TWeakObjectPtr<AAAAMissionPickup>& P : Pickups)
		{
			if (!P.IsValid() || P->bTaken) continue;
			const double D = FVector::DistSquared(P->GetActorLocation(), L->GetActorLocation());
			if (D < Best) { Best = D; Marker = P->GetActorLocation() + FVector(0.0, 0.0, 60.0); MarkerLabel = P->Kind == 0 ? TEXT("سلاح") : TEXT("ذخيرة"); bHasMarker = true; }
		}
	}
}

void UAAAMissionSubsystem::EndMission(UAAAWeaponComponent* Wp, UAAAWeaponsSubsystem* Ws)
{
	// free roam: the random army patrols come back, the ammo is no longer counted
	if (Ws) Ws->bEnemyPatrols = true;
	if (Wp) Wp->bInfiniteReserve = bSavedInfinite;
	for (const TWeakObjectPtr<AAAAMissionPickup>& P : Pickups) if (P.IsValid()) P->Destroy();
	Pickups.Reset();
	Objectives.Reset();
	if (Cam.IsValid()) Cam->Destroy();
}

/* ============================================================================================ the hero's death */

void UAAAMissionSubsystem::TickDeathScene(float Dt, APlayerController* PC, ACharacter* L, UAAAWeaponComponent* Wp)
{
	if (Wp && Wp->IsDead())
	{
		if (!bDeathScene) { bDeathScene = true; bDeathShot = false; Black = 0.f; LineT = -1.f; CardT = -1.f; CardAlpha = 0.f; }
		// game seconds since he was hit (the fall plays in slow motion: AAAWeaponComponent), S = since he lies still
		const float Lf = (Wp->DeathIdx != INDEX_NONE && !Wp->bRagdolled) ? Wp->DeathAnimLen : 1.0f;
		const float S = Wp->DeadT - Lf;
		Letterbox = FMath::Min(1.f, Letterbox + Dt / 0.5f);
		// just before he reaches the ground: a close shot from above his head, on the side of the raised hand
		if (S > -0.15f && !bDeathShot) { bDeathShot = true; ViewCinematic(PC); ShotWho = 100; }
		if (bDeathShot) TickDeathCamera(Dt, L, S);
		// the testimony, in two lines, while the finger is up (raised 0..1.4 s, held to 7.4 s)
		if (S >= 1.2f && S < 4.4f) { Speaker = TEXT("بن بولعيد"); SpeakerColor = FLinearColor(0.77f, 0.63f, 0.35f); Line = TEXT("أشهدُ أن لا إله إلا الله"); LineT = S - 1.2f; }
		else if (S >= 4.4f && S < 7.6f) { Speaker = TEXT("بن بولعيد"); SpeakerColor = FLinearColor(0.77f, 0.63f, 0.35f); Line = TEXT("وأشهدُ أنّ محمدًا رسولُ الله"); LineT = S - 4.4f; }
		else LineT = -1.f;
		// the arm sinks, a last card, then black until he is back
		if (S >= 8.0f)
		{
			CardLine1 = TEXT("استُشهد البطل"); CardLine2 = TEXT("والثورة لا تموت.");
			bCardTitle = false;
			CardT = S - 8.0f;
			CardAlpha = FMath::Clamp(FMath::Min((S - 8.0f) / 0.7f, (10.6f - S) / 0.4f), 0.f, 1.f);
		}
		else { CardT = -1.f; CardAlpha = 0.f; }
		Black = FMath::Clamp((S - 9.0f) / 1.0f, 0.f, 1.f);
		return;
	}
	if (bDeathScene)
	{
		// back on his feet (respawned): the game camera again, the black fades, a word of courage
		bDeathScene = false; bDeathShot = false;
		LineT = -1.f; CardT = -1.f; CardAlpha = 0.f;
		ViewGame(PC, 0.f);
		Black = Phase == EAAAMissionPhase::Off ? 0.f : 1.f;
		Letterbox = 0.f;
		if (Phase == EAAAMissionPhase::Play)
		{
			SetBanner(TEXT("الثورة لا تموت"), TEXT("عُد إلى القتال"));
			// checkpoint: he comes back at the rally point, not at the far player start
			if (L && !Rally.IsZero() && FVector::Dist2D(L->GetActorLocation(), Rally) > 1500.0)
				L->TeleportTo(Rally, FRotator(0.0, (Camp - Rally).Rotation().Yaw, 0.0), false, true);
		}
	}
}

void UAAAMissionSubsystem::TickDeathCamera(float Dt, ACharacter* L, float S)
{
	ACameraActor* C = Cam.Get();
	if (!C || !L) return;
	const USkeletalMeshComponent* M = L->GetMesh();
	const FVector Up(0.0, 0.0, 1.0);
	const FVector Head = M ? M->GetSocketLocation(TEXT("head")) : L->GetActorLocation();
	const FVector Pel = M ? M->GetSocketLocation(TEXT("pelvis")) : Head - FVector(60.0, 0.0, 0.0);
	const FVector Hand = M ? M->GetSocketLocation(TEXT("hand_r")) : Head;
	FVector Along = Head - Pel; Along.Z = 0.0;
	if (Along.SizeSquared() < 1.0) Along = L->GetActorForwardVector();
	Along.Normalize();
	FVector Side = FVector::CrossProduct(Up, Along);
	if (FVector::DotProduct(Hand - Head, Side) < 0.0) Side = -Side;   // the side of the raised hand
	const float Push = FMath::Clamp(S / 9.f, 0.f, 1.f);                 // a slow push in over the whole scene
	const float Up01 = FMath::SmoothStep(0.f, 1.4f, S) * (1.f - FMath::SmoothStep(7.4f, 9.0f, S));
	const FVector Look = FMath::Lerp(Head, Head * 0.55 + Hand * 0.45, (double)Up01) + Up * 8.0;
	FVector Pos = Head + Along * (120.0 - 35.0 * Push) + Side * (95.0 - 25.0 * Push) + Up * (150.0 - 35.0 * Push);
	if (UWorld* W = GetWorld())
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AAADeathCam), false, L);
		if (W->SweepSingleByChannel(H, Look, Pos, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q)) Pos = H.Location;
	}
	const bool bCutNow = ShotWho == 100;   // the cut
	if (bCutNow) ShotWho = 101;
	CamLook(C, Pos, Look, Dt, bCutNow, 0.25f);
}

/* ============================================================================================ polish */

void UAAAMissionSubsystem::Sfx(uint8 Snd, float Vol, float Pitch)
{
	AAAMissionAudio::Play(GetWorld(), (AAAMissionAudio::ESnd)Snd, Vol, Pitch);
}

void UAAAMissionSubsystem::StartMusic(float Vol)
{
	StopMusic(0.5f);
	Music = AAAMissionAudio::Play(GetWorld(), AAAMissionAudio::ESnd::Music, Vol);
}

void UAAAMissionSubsystem::StopMusic(float FadeS)
{
	if (UAudioComponent* A = Music.Get())
	{
		if (FadeS > 0.f) A->FadeOut(FadeS, 0.f);
		else A->Stop();
	}
	Music.Reset();
}

void UAAAMissionSubsystem::SetTalk(int32 Who, float T)
{
	const ACharacter* Ch = Cast<ACharacter>(Actor(Who));
	if (!Ch || !Ch->GetMesh()) return;
	if (UAAARifleAnimInstance* A = Cast<UAAARifleAnimInstance>(Ch->GetMesh()->GetAnimInstance())) A->Inputs.TalkT = T;
}

void UAAAMissionSubsystem::TickPolish(float Dt)
{
	const float RealDt = (float)FMath::Min(FApp::GetDeltaTime(), 0.1);   // (unaffected by the death slow motion)
	// the engine's debug lines ("Police: ...", "WANTED") are hidden while a cinematic plays
	if (GEngine)
	{
		const bool bCine = IsCinematic();
		if (bCine && !bDebugMsgsSaved) { bDebugMsgsSaved = true; bDebugMsgsWere = GEngine->bEnableOnScreenDebugMessages; GEngine->bEnableOnScreenDebugMessages = false; }
		else if (!bCine && bDebugMsgsSaved) { bDebugMsgsSaved = false; GEngine->bEnableOnScreenDebugMessages = bDebugMsgsWere; }
	}
	// typewriter ticks for the cards and the subtitles (one per new letter, not for spaces, at most every 40 ms)
	TickGap -= RealDt;
	auto Typed = [](const FString& Str, float T, float Per) { return FMath::Clamp(FMath::FloorToInt(T / Per) + 1, 0, Str.Len()); };
	const int32 NowCard = (CardT >= 0.f && !bCardTitle) ? Typed(CardLine1, CardT, 0.045f) : 0;
	const int32 NowLine = (LineT >= 0.f) ? Typed(Line, LineT, 0.028f) : 0;
	const bool bNewCard = NowCard > TypedCard && CardLine1.IsValidIndex(NowCard - 1) && CardLine1[NowCard - 1] != TEXT(' ');
	const bool bNewLine = NowLine > TypedLine && Line.IsValidIndex(NowLine - 1) && Line[NowLine - 1] != TEXT(' ');
	if ((bNewCard || bNewLine) && TickGap <= 0.f)
	{
		TickGap = 0.04f;
		Sfx((uint8)AAAMissionAudio::ESnd::Tick, bNewCard ? 0.35f : 0.18f, FMath::FRandRange(0.92f, 1.08f));
	}
	TypedCard = NowCard; TypedLine = NowLine;
	// a chime with every banner (new objective, objective done, reinforcements, ...)
	static float LastBannerT = -1.f;
	if (BannerT >= 0.f && (LastBannerT < 0.f || BannerT < LastBannerT)) Sfx((uint8)AAAMissionAudio::ESnd::Chime, 0.6f);
	LastBannerT = BannerT;
	// talking heads: the speaker nods and breathes while his line is on screen (everybody for "الجميع")
	const bool bTalkScene = Phase == EAAAMissionPhase::Briefing || Phase == EAAAMissionPhase::Outro;
	if (bTalkScene && !bDeathScene)
	{
		const int32 Who = (Beats.IsValidIndex(BeatIdx) && Beats[BeatIdx].Type == 2) ? Beats[BeatIdx].Who : -99;
		for (int32 Wk = -1; Wk <= 2; ++Wk) SetTalk(Wk, (Who == Wk || Who == 3) && LineT >= 0.f ? LineT : -1.f);
	}
	// the death scene: a heartbeat that slows down and stops as he goes
	if (bDeathScene)
	{
		UWorld* W = GetWorld();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		const ACharacter* L = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		const UAAAWeaponComponent* Wp = L ? L->FindComponentByClass<UAAAWeaponComponent>() : nullptr;
		const float S = Wp ? Wp->DeadT - ((Wp->DeathIdx != INDEX_NONE && !Wp->bRagdolled) ? Wp->DeathAnimLen : 1.f) : 99.f;
		HeartT -= RealDt;
		if (S < 8.6f && HeartT <= 0.f)
		{
			const float Gap = FMath::Lerp(0.85f, 1.9f, FMath::Clamp((S + 2.f) / 10.f, 0.f, 1.f));
			HeartT = Gap;
			Sfx((uint8)AAAMissionAudio::ESnd::Heart, FMath::Lerp(0.9f, 0.35f, FMath::Clamp(S / 8.6f, 0.f, 1.f)));
		}
	}
}
