#include "AAAWeaponsSubsystem.h"
#include "AAAWeaponComponent.h"
#include "AAAPoliceOfficer.h"
#include "TrafficAIManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATrafficAIManager* UAAAWeaponsSubsystem::Traffic()
{
	if (!TrafficMgr.IsValid() && SearchT <= 0.f && GetWorld())
	{
		SearchT = 2.f;
		for (TActorIterator<ATrafficAIManager> It(GetWorld()); It; ++It) { TrafficMgr = *It; break; }
	}
	return TrafficMgr.Get();
}

int32 UAAAWeaponsSubsystem::WantedStars()
{
	ATrafficAIManager* T = Traffic();
	return T ? T->GetWantedStars() : 0;
}

uint8 UAAAWeaponsSubsystem::TeamOf(const AActor* A) const
{
	if (!A) return 255;
	if (const AAAAPoliceOfficer* O = Cast<AAAAPoliceOfficer>(A)) return O->Team;
	if (A == Equipped.Get()) return AAATeam::Player;
	if (const APawn* P = Cast<APawn>(A)) if (P->IsPlayerControlled()) return AAATeam::Player;
	return 255;
}

void UAAAWeaponsSubsystem::GetFoes(const AAAAPoliceOfficer* Me, TArray<ACharacter*>& Out)
{
	Out.Reset();
	if (!Me) return;
	const int32 Stars = WantedStars();
	ACharacter* L = Equipped.Get();
	if (L && !L->IsHidden() && AAAAPoliceOfficer::Hostile(Me->Team, AAATeam::Player, Stars))
	{
		const UAAAWeaponComponent* W = L->FindComponentByClass<UAAAWeaponComponent>();
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if ((!W || !W->IsDead()) && PC && PC->GetPawn() == L) Out.Add(L);   // not while he sits in a car
	}
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X == Me || X->IsOut()) continue;
		if (AAAAPoliceOfficer::Hostile(Me->Team, X->Team, Stars)) Out.Add(X);
	}
}

void UAAAWeaponsSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W) return;
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	const float Now = (float)W->GetTimeSeconds();
	SearchT -= Dt; BackupT -= Dt;
	Officers.RemoveAll([](const TWeakObjectPtr<AAAAPoliceOfficer>& O) { return !O.IsValid(); });

	APlayerController* PC = W->GetFirstPlayerController();
	// the character the player walks around with (not the Tesla / other pawns)
	ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (C && C != Equipped.Get())
	{
		if (!C->FindComponentByClass<UAAAWeaponComponent>())
		{
			UAAAWeaponComponent* Comp = NewObject<UAAAWeaponComponent>(C, TEXT("AAAWeapon"));
			Comp->RegisterComponent();
			C->AddInstanceComponent(Comp);
		}
		Equipped = C;
	}

	// the radio: reports reach the mates after a short delay (and the callouts fade from the HUD)
	for (int32 I = Pending.Num() - 1; I >= 0; --I)
	{
		FPendingContact& Pc = Pending[I];
		Pc.T -= Dt;
		if (Pc.T > 0.f) continue;
		for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
		{
			AAAAPoliceOfficer* X = O.Get();
			if (!X || X->IsOut() || X->Team != Pc.Team || X == Pc.From.Get()) continue;
			// the further from the one who saw it, the vaguer the report
			const float D = Pc.From.IsValid() ? (float)FVector::Dist(Pc.From->GetActorLocation(), X->GetActorLocation()) : 0.f;
			const FVector Err = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * FMath::Min(D * 0.03f, 250.f);
			X->RadioContact(Pc.Who.Get(), Pc.Where + Err);
		}
		Pending.RemoveAtSwap(I);
	}
	for (FCallout& Co : Callouts) Co.T += Dt;
	Callouts.RemoveAll([](const FCallout& Co) { return Co.T > 7.f; });
	Medics.RemoveAll([Now](const FMedic& M) { return !M.Patient.IsValid() || !M.Medic.IsValid() || !M.Patient->IsDowned() || M.Medic->IsOut() || Now - M.T > 2.f; });

	// the player's fire team and the army patrols
	ACharacter* L = Equipped.Get();
	if (L)
	{
		TickSquad(L, Dt);
		TickEnemies(L, Dt);
	}

	ATrafficAIManager* T = Traffic();
	const int32 Stars = T ? T->GetWantedStars() : 0;
	// wanted cleared (or busted / wasted): the police stand down and leave when nobody looks
	if (Stars == 0)
	{
		CalmT += Dt;
		for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
		{
			if (!O.IsValid() || O->Team != AAATeam::Police) continue;
			O->StandDown();
			if (CalmT > 3.f && (O->CanDespawn() || CalmT > 25.f)) O->Destroy();
		}
		ExtraCars = 0;
		return;
	}
	CalmT = 0.f;
	if (C && (DismountT -= Dt) <= 0.f) { DismountT = 0.5f; SpawnOfficers(C); }
	if (C && (RoleT -= Dt) <= 0.f) { RoleT = FMath::FRandRange(6.f, 9.f); AssignRoles(C); }
}

/* ============================================================================================ spawning */

AAAAPoliceOfficer* UAAAWeaponsSubsystem::SpawnSoldier(uint8 Team, const FVector& Where, float Yaw, int32 Index)
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	const FTransform Xf(FRotator(0.0, Yaw, 0.0), Where);
	AAAAPoliceOfficer* O = W->SpawnActorDeferred<AAAAPoliceOfficer>(AAAAPoliceOfficer::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!O) return nullptr;
	O->Team = Team;
	O->SquadIndex = Index;
	UGameplayStatics::FinishSpawningActor(O, Xf);
	return O;
}

bool UAAAWeaponsSubsystem::FindHiddenSpot(const FVector& Around, float MinR, float MaxR, FVector& Out) const
{
	// on the ground, out of the camera's view (so nobody pops into sight)
	UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const FVector Cam = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetCameraLocation() : Around;
	const FVector CamF = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetCameraRotation().Vector() : FVector::ForwardVector;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAAHiddenSpot), false);
	if (Equipped.IsValid()) Q.AddIgnoredActor(Equipped.Get());
	for (int32 K = 0; K < 24; ++K)
	{
		const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
		const FVector P = Around + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(MinR, MaxR);
		FHitResult H;
		if (!W->LineTraceSingleByChannel(H, P + FVector(0.0, 0.0, 1500.0), P - FVector(0.0, 0.0, 1500.0), ECC_Visibility, Q)) continue;
		if (H.ImpactNormal.Z < 0.8) continue;
		const FVector G = H.ImpactPoint + FVector(0.0, 0.0, 95.0);
		if (FMath::Abs(G.Z - Around.Z) > 600.0) continue;   // not on a roof
		if (W->OverlapAnyTestByChannel(G, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(36.f, 85.f), Q)) continue;
		const FVector D = G - Cam;
		const bool bInView = FVector::DotProduct(D.GetSafeNormal(), CamF) > 0.35;
		FHitResult V;
		const bool bSeen = bInView && !W->LineTraceSingleByChannel(V, Cam, G + FVector(0.0, 0.0, 40.0), ECC_Visibility, Q);
		if (bSeen) continue;
		Out = G;
		return true;
	}
	return false;
}

FVector UAAAWeaponsSubsystem::FormationSlot(int32 Idx) const
{
	const ACharacter* L = Equipped.Get();
	if (!L) return FVector::ZeroVector;
	// the slot turns with where the leader goes (or looks, standing)
	const FVector V = L->GetVelocity();
	const float Yaw = V.Size2D() > 120.0 ? (float)V.Rotation().Yaw : (float)L->GetActorRotation().Yaw;
	const FVector2D S = FAAASquadInfo::Slot(Formation, Idx) * 100.0;
	const FVector P = L->GetActorLocation() + FRotator(0.0, Yaw, 0.0).RotateVector(FVector(S.X, S.Y, 0.0));
	// on the ground there (a slot inside a wall: closer to the leader)
	UWorld* W = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AAASlot), false, L);
	FHitResult H;
	if (W && W->LineTraceSingleByChannel(H, L->GetActorLocation(), P, ECC_Visibility, Q)) return L->GetActorLocation() + (P - L->GetActorLocation()) * FMath::Max(0.f, H.Time - 0.15f);
	return P;
}

void UAAAWeaponsSubsystem::TickSquad(ACharacter* L, float Dt)
{
	if (!bSquadEnabled) return;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const bool bOnFoot = PC && PC->GetPawn() == L && !L->IsHidden();
	if (SquadMen.Num() < 3) SquadMen.SetNum(3);
	SquadT -= Dt;

	// the first three join a little after the start, behind the leader
	if (!bSquadSpawned)
	{
		if (!bOnFoot || W->GetTimeSeconds() < 2.0) return;
		bSquadSpawned = true;
		for (int32 I = 0; I < 3; ++I)
		{
			FVector P = FormationSlot(I);
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AAASquadSpawn), false, L);
			if (W->LineTraceSingleByChannel(H, P + FVector(0.0, 0.0, 300.0), P - FVector(0.0, 0.0, 600.0), ECC_Visibility, Q)) P = H.ImpactPoint + FVector(0.0, 0.0, 95.0);
			SquadMen[I] = SpawnSoldier(AAATeam::Player, P, (float)L->GetActorRotation().Yaw, I);
		}
		SayLeader(TEXT("الفريق جاهز. أنتم معي."));
		return;
	}

	for (int32 I = 0; I < 3; ++I)
	{
		AAAAPoliceOfficer* M = SquadMen[I].Get();
		// a fallen man is replaced after a while (reinforcement out of sight near the leader)
		if (!M || M->IsDead())
		{
			if (ReinforceT[I] <= 0.f) ReinforceT[I] = 60.f;
			ReinforceT[I] -= Dt;
			if (ReinforceT[I] <= 0.f && bOnFoot)
			{
				FVector P;
				if (FindHiddenSpot(L->GetActorLocation(), 1500.f, 3000.f, P))
				{
					SquadMen[I] = SpawnSoldier(AAATeam::Player, P, (float)L->GetActorRotation().Yaw, I);
					ReinforceT[I] = 0.f;
					if (AAAAPoliceOfficer* N = SquadMen[I].Get()) Say(N, TEXT("تعزيزات! انضممت إليكم."));
				}
				else ReinforceT[I] = 2.f;
			}
			continue;
		}
		ReinforceT[I] = 0.f;
		if (M->IsOut()) continue;
		// left far behind (the leader drove off, climbed, ran): catch up out of sight when he is on foot again
		if (bOnFoot && SquadT <= 0.f)
		{
			const float D = (float)FVector::Dist(M->GetActorLocation(), L->GetActorLocation());
			if (D > 6000.f && (M->GetOrder() == EAAASquadOrder::Follow) && !M->InCombat())
			{
				FVector P;
				if (M->CanDespawn() && FindHiddenSpot(L->GetActorLocation(), 900.f, 2000.f, P)) M->CatchUp(P);
			}
		}
	}
	if (SquadT <= 0.f) SquadT = 1.f;
}

void UAAAWeaponsSubsystem::TickEnemies(ACharacter* L, float Dt)
{
	if (!bEnemyPatrols) return;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const bool bOnFoot = PC && PC->GetPawn() == L && !L->IsHidden();
	EnemyT -= Dt;
	if (EnemyT > 0.f) return;
	EnemyT = 12.f;
	int32 Alive = 0;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X->Team != AAATeam::Enemy || X->IsDead()) continue;
		// far away and forgotten: gone
		if (FVector::Dist(X->GetActorLocation(), L->GetActorLocation()) > 25000.f && X->CanDespawn() && !X->InCombat()) { X->Destroy(); continue; }
		++Alive;
	}
	if (!bOnFoot || Alive + 3 > MaxEnemies) return;
	// a new patrol of three somewhere 80..150 m away, walking a beat around their spot
	FVector Base;
	if (!FindHiddenSpot(L->GetActorLocation(), 8000.f, 15000.f, Base)) return;
	TArray<FVector> Beat;
	for (int32 K = 0; K < 4; ++K)
	{
		FVector P;
		if (!FindHiddenSpot(Base, 600.f, 3000.f, P)) P = Base + FVector(FMath::FRandRange(-2000.f, 2000.f), FMath::FRandRange(-2000.f, 2000.f), 0.f);
		Beat.Add(P);
	}
	for (int32 I = 0; I < 3; ++I)
	{
		const FVector P = Base + FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 0.f) + FVector(I * 160.0, 0.0, 0.0);
		if (AAAAPoliceOfficer* E = SpawnSoldier(AAATeam::Enemy, P, FMath::FRandRange(-180.f, 180.f), I))
		{
			TArray<FVector> Mine = Beat;
			for (FVector& B : Mine) B += FVector(I * 150.0, (I - 1) * 150.0, 0.0);
			E->SetPatrol(Mine);
		}
	}
}

void UAAAWeaponsSubsystem::SpawnOfficers(ACharacter* C)
{
	ATrafficAIManager* T = Traffic();
	if (!T || C->IsHidden()) return;
	int32 Alive = 0;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers) if (O.IsValid() && O->Team == AAATeam::Police && !O->IsDead()) ++Alive;
	const int32 Room = MaxOfficers - Alive;
	if (Room < 2) return;
	TArray<ATrafficAIManager::FCopDismount> Out;
	T->TakeDismounts(FMath::Min(Room / 2, 1 + ExtraCars), Out);
	for (const ATrafficAIManager::FCopDismount& D : Out)
	{
		FVector P = D.Pos;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AAADismount), false);
		if (GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.0, 0.0, 60.0), P - FVector(0.0, 0.0, 600.0), ECC_Visibility, Q)) P = H.ImpactPoint + FVector(0.0, 0.0, 95.0);   // from door height: a trace from higher up landed officers on tree crowns / awnings
		if (AAAAPoliceOfficer* O = SpawnSoldier(AAATeam::Police, P, (float)D.Rot.Yaw, -1))
		{
			O->RadioIntel(C->GetActorLocation());   // they know roughly where he is: the car was chasing him
			O->BeginExit(D.Out);                    // step out of the door drawing the rifle, then straight into the fight
		}
	}
	if (Out.Num()) ExtraCars = FMath::Max(0, ExtraCars - 1);
}

void UAAAWeaponsSubsystem::AssignRoles(ACharacter* C)
{
	// the officer closest to the player keeps him busy, every second one goes round the side
	TArray<AAAAPoliceOfficer*> Live;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers) if (O.IsValid() && O->Team == AAATeam::Police && !O->IsDead() && O->IsEngaged()) Live.Add(O.Get());
	const FVector P = C->GetActorLocation();
	Live.Sort([P](const AAAAPoliceOfficer& A, const AAAAPoliceOfficer& B) { return FVector::DistSquared(A.GetActorLocation(), P) < FVector::DistSquared(B.GetActorLocation(), P); });
	for (int32 I = 0; I < Live.Num(); ++I) Live[I]->SetRole(I % 2 == 1 ? AAAAPoliceOfficer::RoleFlank : AAAAPoliceOfficer::RoleSuppress);
}

/* ============================================================================================ the radio */

void UAAAWeaponsSubsystem::Register(AAAAPoliceOfficer* O) { Officers.AddUnique(O); }

void UAAAWeaponsSubsystem::ShareIntel(AAAAPoliceOfficer* From, const FVector& Where)
{
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
		if (O.IsValid() && O.Get() != From && O->Team == From->Team && !O->IsOut() && FVector::Dist(O->GetActorLocation(), From->GetActorLocation()) < 20000.f) O->RadioIntel(Where);
}

void UAAAWeaponsSubsystem::ShareContact(const AAAAPoliceOfficer* From, uint8 Team, ACharacter* Who, const FVector& Where)
{
	// one report per enemy in flight at a time; the radio takes a moment (0.4..1.1 s)
	for (FPendingContact& Pc : Pending) if (Pc.Team == Team && Pc.Who.Get() == Who && Who) { Pc.Where = Where; return; }
	FPendingContact N;
	N.Team = Team; N.Who = Who; N.Where = Where; N.From = From;
	N.T = FMath::FRandRange(0.4f, 1.1f);
	Pending.Add(N);
}

void UAAAWeaponsSubsystem::Say(const AAAAPoliceOfficer* From, const FString& Text)
{
	if (!From || From->Team != AAATeam::Player || Text.IsEmpty()) return;
	const FString Who = FAAASquadInfo::Name(From->SquadIndex);
	// the same words from another man a moment ago: one is enough ("contact!" x3)
	for (const FCallout& Co : Callouts) if (Co.Text == Text && Co.T < 3.f) return;
	FCallout C;
	C.Who = Who; C.Text = Text; C.Color = FAAASquadInfo::Color(From->SquadIndex); C.T = 0.f;
	Callouts.Add(C);
	if (Callouts.Num() > 5) Callouts.RemoveAt(0);
	++CalloutSerial;
}

void UAAAWeaponsSubsystem::SayLeader(const FString& Text)
{
	FCallout C;
	C.Who = TEXT("القائد"); C.Text = Text; C.Color = FLinearColor(0.9f, 0.83f, 0.7f); C.T = 0.f;
	Callouts.Add(C);
	if (Callouts.Num() > 5) Callouts.RemoveAt(0);
	++CalloutSerial;
}

void UAAAWeaponsSubsystem::RequestCoverFire(const AAAAPoliceOfficer* Mover, const FVector& Where)
{
	if (!Mover) return;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X == Mover || X->IsOut() || X->Team != Mover->Team || X->IsMovingToCover()) continue;
		if (FVector::Dist(X->GetActorLocation(), Mover->GetActorLocation()) > 4500.f) continue;
		X->RequestCoverFire(Where, 2.8f);
	}
	if (Mover->Team == AAATeam::Player) Say(Mover, TEXT("غطّوني!"));
}

int32 UAAAWeaponsSubsystem::TargetClaims(const AAAAPoliceOfficer* Me, const ACharacter* Who) const
{
	if (!Me || !Who) return 0;
	int32 N = 0;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		const AAAAPoliceOfficer* X = O.Get();
		if (X && X != Me && X->Team == Me->Team && !X->IsOut() && X->Foe() == Who) ++N;
	}
	return N;
}

void UAAAWeaponsSubsystem::GetMates(const AAAAPoliceOfficer* Me, TArray<AAAAPoliceOfficer*>& Out) const
{
	Out.Reset();
	if (!Me) return;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (X && X != Me && X->Team == Me->Team && !X->IsOut() && FVector::Dist(X->GetActorLocation(), Me->GetActorLocation()) < 6000.f) Out.Add(X);
	}
}

bool UAAAWeaponsSubsystem::IsReviveTaken(const AAAAPoliceOfficer* Patient, const AAAAPoliceOfficer* Me) const
{
	for (const FMedic& M : Medics) if (M.Patient.Get() == Patient && M.Medic.Get() != Me && M.Medic.IsValid()) return true;
	return false;
}

void UAAAWeaponsSubsystem::ClaimRevive(const AAAAPoliceOfficer* Medic, const AAAAPoliceOfficer* Patient)
{
	const float Now = GetWorld() ? (float)GetWorld()->GetTimeSeconds() : 0.f;
	for (FMedic& M : Medics) if (M.Patient.Get() == Patient) { M.Medic = Medic; M.T = Now; return; }
	FMedic N;
	N.Patient = Patient; N.Medic = Medic; N.T = Now;
	Medics.Add(N);
}

AAAAPoliceOfficer* UAAAWeaponsSubsystem::NeedsMedic(const AAAAPoliceOfficer* Me) const
{
	if (!Me) return nullptr;
	AAAAPoliceOfficer* Best = nullptr;
	float BestD = 1e12f;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X == Me || X->Team != Me->Team || !X->IsDowned() || IsReviveTaken(X, Me)) continue;
		const float D = (float)FVector::DistSquared(X->GetActorLocation(), Me->GetActorLocation());
		if (D < BestD) { BestD = D; Best = X; }
	}
	return Best;
}

int32 UAAAWeaponsSubsystem::ResupplySquad(const FVector& Where)
{
	int32 N = 0;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : SquadMen)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (X && !X->IsOut() && FVector::Dist(X->GetActorLocation(), Where) < 1000.f) { X->Resupply(); ++N; }
	}
	return N;
}

AAAAPoliceOfficer* UAAAWeaponsSubsystem::DownedNear(const FVector& Where, float Radius) const
{
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : SquadMen)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (X && X->IsDowned() && FVector::Dist2D(X->GetActorLocation(), Where) < Radius) return X;
	}
	return nullptr;
}

bool UAAAWeaponsSubsystem::CanMove(const AAAAPoliceOfficer* Me) const
{
	// bounding overwatch: at most half the squad (at least one) runs between covers at a time
	int32 Live = 0, Moving = 0;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		if (!O.IsValid() || O->IsOut() || O->Team != Me->Team) continue;
		++Live;
		if (O.Get() != Me && O->IsMovingToCover()) ++Moving;
	}
	return Moving < FMath::Max(1, Live / 2);
}

bool UAAAWeaponsSubsystem::CoverTaken(const AAAAPoliceOfficer* Me, const FVector& Where) const
{
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
		if (O.IsValid() && O.Get() != Me && !O->IsOut() && FVector::Dist2D(O->GetCoverPoint(), Where) < 250.f) return true;
	if (const ACharacter* L = Equipped.Get()) if (Me && Me->Team == AAATeam::Player && FVector::Dist2D(L->GetActorLocation(), Where) < 200.f) return true;
	return false;
}

void UAAAWeaponsSubsystem::CallBackup()
{
	if (BackupT > 0.f) return;
	BackupT = 10.f;
	ExtraCars = FMath::Min(ExtraCars + 1, 2);
	if (ATrafficAIManager* T = Traffic()) if (T->GetWantedStars() < 3) T->SetPlayerWanted(T->GetWantedStars() + 1);
}

void UAAAWeaponsSubsystem::OfficerDown(AAAAPoliceOfficer* O)
{
	// his side hears it on the radio: heads down; the police send more units; the leader's men call it out
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& X : Officers)
		if (X.IsValid() && X.Get() != O && !X->IsOut() && X->Team == O->Team) X->Suppress(0.4f);
	if (O->Team == AAATeam::Police) ExtraCars = FMath::Min(ExtraCars + 1, 2);
	if (O->Team == AAATeam::Player)
	{
		// a mate says it (not the man himself)
		for (const TWeakObjectPtr<AAAAPoliceOfficer>& X : SquadMen)
			if (X.IsValid() && X.Get() != O && !X->IsOut()) { Say(X.Get(), FString::Printf(TEXT("%s سقط!"), FAAASquadInfo::Name(O->SquadIndex))); break; }
	}
}

/* ============================================================================================ the leader's orders */

void UAAAWeaponsSubsystem::SquadOrder(EAAASquadOrder O, const FVector& Where, ACharacter* Target)
{
	LastOrder = O;
	LastOrderPos = Where;
	LastOrderT = GetWorld() ? (float)GetWorld()->GetTimeSeconds() : 0.f;
	static const TCHAR* const Said[7] = { TEXT("اتبعوني!"), TEXT("تقدّموا إلى هناك!"), TEXT("اثبتوا مكانكم!"), TEXT("اهجموا!"), TEXT("احتموا!"), TEXT("نيران تغطية على هذه النقطة!"), TEXT("انسحبوا!") };
	FString Line = Said[FMath::Clamp((int32)O, 0, 6)];
	if (SquadSel >= 0) Line = FString::Printf(TEXT("%s، %s"), FAAASquadInfo::Name(SquadSel), *Line);
	SayLeader(Line);
	bool bAnswered = false;
	for (int32 I = 0; I < SquadMen.Num(); ++I)
	{
		if (SquadSel >= 0 && I != SquadSel) continue;
		AAAAPoliceOfficer* M = SquadMen[I].Get();
		if (!M || M->IsOut()) continue;
		// the leader marked an enemy: everybody knows of him at once
		if (Target) M->RadioContact(Target, Target->GetActorLocation());
		M->SetOrder(O, Where, Target);
		bAnswered = true;
	}
	if (!bAnswered) SayLeader(TEXT("(لا أحد يجيب)"));
}

void UAAAWeaponsSubsystem::SquadToggleFire()
{
	bHoldFire = !bHoldFire;
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : SquadMen) if (O.IsValid()) O->SetHoldFire(bHoldFire);
	SayLeader(bHoldFire ? TEXT("لا تطلقوا النار إلا للدفاع!") : TEXT("أطلقوا النار بحرية!"));
}

void UAAAWeaponsSubsystem::SquadCycleFormation()
{
	Formation = Formation == EAAAFormation::Wedge ? EAAAFormation::Line : (Formation == EAAAFormation::Line ? EAAAFormation::Column : EAAAFormation::Wedge);
	SayLeader(FString::Printf(TEXT("تشكيل: %s"), FAAASquadInfo::FormationName(Formation)));
}

/* ============================================================================================ noise */

void UAAAWeaponsSubsystem::OnPlayerGunshot(const FVector& Where)
{
	// the leader's own men don't take his shots for an enemy; the others hear a shooter
	const int32 Stars = WantedStars();
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
		if (O.IsValid() && O->Team != AAATeam::Player && (O->Team == AAATeam::Enemy || Stars > 0)) O->HearShot(Where, 1.f);
}

void UAAAWeaponsSubsystem::OnGunshot(const FVector& Where, uint8 ShooterTeam)
{
	// soldiers of a side hostile to the shooter hear "a shooter over there"; police only fight while WANTED
	const int32 Stars = WantedStars();
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X->IsOut() || !AAAAPoliceOfficer::Hostile(X->Team, ShooterTeam, Stars)) continue;
		X->HearShot(Where, 0.8f);
	}
}

void UAAAWeaponsSubsystem::OnBulletPathTeam(const FVector& A, const FVector& B, uint8 ShooterTeam, float Amount)
{
	// a bullet passing within ~2.5 m of a soldier of a hostile side: suppression (head down)
	const int32 Stars = FMath::Max(1, WantedStars());
	for (const TWeakObjectPtr<AAAAPoliceOfficer>& O : Officers)
	{
		AAAAPoliceOfficer* X = O.Get();
		if (!X || X->IsOut() || !AAAAPoliceOfficer::Hostile(X->Team, ShooterTeam, Stars)) continue;
		const FVector Chest = X->GetActorLocation() + FVector(0.0, 0.0, 30.0);
		if (FMath::PointDistToSegment(Chest, A, B) < 250.f)
		{
			X->Suppress(Amount);
			// the leader is shooting at someone: his men see where his rounds go
			if (ShooterTeam == AAATeam::Player)
				for (const TWeakObjectPtr<AAAAPoliceOfficer>& M : SquadMen)
					if (M.IsValid() && !M->IsOut()) M->RadioContact(X, X->GetActorLocation());
		}
	}
}
