// AAA City Weapons - Tactical Squad Manager Component Implementation
#include "AAASquadManagerComponent.h"
#include "AAASquadMemberCharacter.h"
#include "AAAPoliceOfficer.h"
#include "AAATrainingTarget.h"

#include "Camera/PlayerCameraManager.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UAAASquadManagerComponent::UAAASquadManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAAASquadManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HudHandle.IsValid())
	{
		HudHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UAAASquadManagerComponent::DrawSquadHud));
	}

	if (bAutoSpawnSquad)
	{
		SpawnSquad();
	}
}

void UAAASquadManagerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HudHandle.IsValid())
	{
		UDebugDrawService::Unregister(HudHandle);
		HudHandle.Reset();
	}
	Super::EndPlay(Reason);
}

ACharacter* UAAASquadManagerComponent::PlayerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

APlayerController* UAAASquadManagerComponent::PlayerController() const
{
	if (const ACharacter* C = PlayerCharacter())
	{
		return Cast<APlayerController>(C->GetController());
	}
	return UGameplayStatics::GetPlayerController(this, 0);
}

void UAAASquadManagerComponent::SpawnSquad()
{
	ACharacter* Leader = PlayerCharacter();
	UWorld* W = GetWorld();
	if (!Leader || !W || SquadMembers.Num() > 0) return;

	const FVector LeaderLoc = Leader->GetActorLocation();
	const FRotator LeaderRot = Leader->GetActorRotation();

	const EAAASquadRole Roles[3] = { EAAASquadRole::Assault, EAAASquadRole::Support, EAAASquadRole::Marksman };

	for (int32 i = 0; i < 3; ++i)
	{
		const FVector Offset = CalculateFormationOffset(i, CurrentFormation, FormationSpacing);
		const FVector SpawnLoc = LeaderLoc + LeaderRot.RotateVector(Offset);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AAAASquadMemberCharacter* Member = W->SpawnActor<AAAASquadMemberCharacter>(SpawnLoc, LeaderRot, SpawnParams);
		if (Member)
		{
			Member->SquadRole = Roles[i];
			Member->SetSquadManager(this);
			Member->OrderFollowFormation(Offset);
			SquadMembers.Add(Member);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor(80, 220, 120), TEXT("[SQUAD LEADER] Bravo Team deployed! [V / Middle Click]: Move/Attack | [C]: Suppress | [H]: Hold | [Z]: Regroup | [F1-F4]: Formations & RoE"));
	}
}

void UAAASquadManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	CommandKeyCooldown -= Dt;
	if (bHasActiveWaypoint)
	{
		WaypointTimer -= Dt;
		if (WaypointTimer <= 0.f)
		{
			bHasActiveWaypoint = false;
		}
	}

	APlayerController* PC = PlayerController();
	if (PC && CommandKeyCooldown <= 0.f)
	{
		// Contextual command via Middle Mouse Button or V key (G is reserved for Grenades, B for Binoculars)
		if (PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton) || PC->WasInputKeyJustPressed(EKeys::V))
		{
			CommandKeyCooldown = 0.25f;
			IssueContextualCommand();
		}
		// C key: Order heavy suppressive fire at crosshair location
		else if (PC->WasInputKeyJustPressed(EKeys::C))
		{
			CommandKeyCooldown = 0.25f;
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			FHitResult Hit;
			FCollisionQueryParams Q(TEXT("SquadSuppressTrace"), true, PlayerCharacter());
			if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * 15000.f, ECC_Visibility, Q))
			{
				CurrentWaypoint = Hit.Location;
				bHasActiveWaypoint = true;
				WaypointTimer = 5.0f;
				IssueSuppressCommand(Hit.Location);
			}
		}
		// H key: Hold Position
		else if (PC->WasInputKeyJustPressed(EKeys::H))
		{
			CommandKeyCooldown = 0.25f;
			IssueHoldCommand();
		}
		// Z key: Regroup on Leader
		else if (PC->WasInputKeyJustPressed(EKeys::Z))
		{
			CommandKeyCooldown = 0.25f;
			IssueRegroupCommand();
		}

		// Formation Hotkeys
		if (PC->WasInputKeyJustPressed(EKeys::F1)) SetSquadFormation(EAAASquadFormation::Wedge);
		else if (PC->WasInputKeyJustPressed(EKeys::F2)) SetSquadFormation(EAAASquadFormation::Column);
		else if (PC->WasInputKeyJustPressed(EKeys::F3)) SetSquadFormation(EAAASquadFormation::Line);
		else if (PC->WasInputKeyJustPressed(EKeys::F4))
		{
			SetSquadRoE(CurrentRoE == EAAASquadRoE::FireAtWill ? EAAASquadRoE::HoldFire : EAAASquadRoE::FireAtWill);
		}
	}

	UpdateFormationPositions(Dt);
}

void UAAASquadManagerComponent::UpdateFormationPositions(float /*Dt*/)
{
	for (int32 i = 0; i < SquadMembers.Num(); ++i)
	{
		if (SquadMembers[i] && SquadMembers[i]->CurrentState == EAAASquadState::Following)
		{
			const FVector Offset = CalculateFormationOffset(i, CurrentFormation, FormationSpacing);
			SquadMembers[i]->OrderFollowFormation(Offset);
		}
	}
}

FVector UAAASquadManagerComponent::CalculateFormationOffset(int32 MemberIndex, EAAASquadFormation Formation, float Spacing) const
{
	switch (Formation)
	{
	case EAAASquadFormation::Wedge:
		// Tactical V-Formation
		if (MemberIndex == 0) return FVector(Spacing * 0.9f, Spacing * 0.9f, 0.f);    // Front-Right (Assault)
		if (MemberIndex == 1) return FVector(-Spacing * 0.8f, -Spacing * 1.1f, 0.f);  // Back-Left (Support)
		return FVector(-Spacing * 1.5f, Spacing * 0.4f, 0.f);                          // Rear-Right (Marksman)

	case EAAASquadFormation::Column:
		// Single file behind leader
		return FVector(-Spacing * (float)(MemberIndex + 1) * 0.9f, 0.f, 0.f);

	case EAAASquadFormation::Line:
		// Broad frontal line
		if (MemberIndex == 0) return FVector(0.f, Spacing * 1.2f, 0.f);
		if (MemberIndex == 1) return FVector(0.f, -Spacing * 1.2f, 0.f);
		return FVector(0.f, -Spacing * 2.4f, 0.f);

	case EAAASquadFormation::Dispersed:
	default:
		if (MemberIndex == 0) return FVector(Spacing * 1.2f, Spacing * 1.4f, 0.f);
		if (MemberIndex == 1) return FVector(-Spacing * 1.2f, -Spacing * 1.6f, 0.f);
		return FVector(-Spacing * 2.0f, Spacing * 1.2f, 0.f);
	}
}

/* ============================================================================================ Commands */

void UAAASquadManagerComponent::IssueContextualCommand()
{
	APlayerController* PC = PlayerController();
	UWorld* W = GetWorld();
	if (!PC || !W) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector TraceEnd = CamLoc + CamRot.Vector() * 18000.f;
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("SquadPing"), true, PlayerCharacter());

	if (W->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Q))
	{
		AActor* HitActor = Hit.GetActor();

		// 1. Check if hit actor is a downed squad member -> Revive order
		if (AAAASquadMemberCharacter* DownedAlly = Cast<AAAASquadMemberCharacter>(HitActor))
		{
			if (DownedAlly->IsDowned())
			{
				// Assign medic / nearest active member to revive
				for (AAAASquadMemberCharacter* Member : SquadMembers)
				{
					if (Member && Member != DownedAlly && !Member->IsDowned())
					{
						Member->OrderRevive(DownedAlly);
						break;
					}
				}
				OnSquadCommandIssued.Broadcast(EAAASquadCommand::ReviveTeammate, DownedAlly->GetActorLocation(), DownedAlly);
				return;
			}
		}

		// 2. Check if hit actor is a hostile officer, training target, or enemy character
		if (AAAAPoliceOfficer* Officer = Cast<AAAAPoliceOfficer>(HitActor))
		{
			if (!Officer->IsDead())
			{
				IssueAttackCommand(Officer);
				return;
			}
		}
		else if (AAAATrainingTarget* TrainTarget = Cast<AAAATrainingTarget>(HitActor))
		{
			IssueAttackCommand(TrainTarget);
			return;
		}
		else if (ACharacter* Enemy = Cast<ACharacter>(HitActor))
		{
			if (Enemy != PlayerCharacter() && !Enemy->IsA(AAAASquadMemberCharacter::StaticClass()))
			{
				IssueAttackCommand(Enemy);
				return;
			}
		}

		// 3. Otherwise, ground/cover Move order
		CurrentWaypoint = Hit.Location;
		bHasActiveWaypoint = true;
		WaypointTimer = 8.5f;
		IssueMoveCommand(Hit.Location);
	}
}

void UAAASquadManagerComponent::IssueMoveCommand(const FVector& Location)
{
	OnSquadCommandIssued.Broadcast(EAAASquadCommand::MoveToLocation, Location, nullptr);

	// Distribute squad in a tactical perimeter around the waypoint
	const FVector Offsets[3] = {
		FVector(90.f, 70.f, 0.f),
		FVector(-80.f, -90.f, 0.f),
		FVector(-120.f, 80.f, 0.f)
	};

	for (int32 i = 0; i < SquadMembers.Num(); ++i)
	{
		if (SquadMembers[i] && !SquadMembers[i]->IsDowned())
		{
			SquadMembers[i]->OrderMoveTo(Location + Offsets[i % 3], true);
		}
	}
}

void UAAASquadManagerComponent::IssueAttackCommand(AActor* TargetActor)
{
	if (!TargetActor) return;
	OnSquadCommandIssued.Broadcast(EAAASquadCommand::AttackTarget, TargetActor->GetActorLocation(), TargetActor);

	for (AAAASquadMemberCharacter* Member : SquadMembers)
	{
		if (Member && !Member->IsDowned())
		{
			if (Member->SquadRole == EAAASquadRole::Support)
			{
				Member->OrderSuppress(TargetActor->GetActorLocation());
			}
			else
			{
				Member->OrderAttack(TargetActor);
			}
		}
	}
}

void UAAASquadManagerComponent::IssueSuppressCommand(const FVector& Area)
{
	OnSquadCommandIssued.Broadcast(EAAASquadCommand::SuppressArea, Area, nullptr);
	for (AAAASquadMemberCharacter* Member : SquadMembers)
	{
		if (Member && !Member->IsDowned() && Member->SquadRole == EAAASquadRole::Support)
		{
			Member->OrderSuppress(Area);
		}
	}
}

void UAAASquadManagerComponent::IssueHoldCommand()
{
	bHasActiveWaypoint = false;
	OnSquadCommandIssued.Broadcast(EAAASquadCommand::HoldPosition, FVector::ZeroVector, nullptr);
	for (AAAASquadMemberCharacter* Member : SquadMembers)
	{
		if (Member && !Member->IsDowned())
		{
			Member->OrderHoldPosition();
		}
	}
}

void UAAASquadManagerComponent::IssueRegroupCommand()
{
	bHasActiveWaypoint = false;
	OnSquadCommandIssued.Broadcast(EAAASquadCommand::Regroup, FVector::ZeroVector, nullptr);
	for (AAAASquadMemberCharacter* Member : SquadMembers)
	{
		if (Member && !Member->IsDowned())
		{
			Member->OrderRegroup();
		}
	}
}

void UAAASquadManagerComponent::SetSquadFormation(EAAASquadFormation NewFormation)
{
	CurrentFormation = NewFormation;
	OnSquadFormationChanged.Broadcast(NewFormation);

	const TCHAR* FormNames[] = { TEXT("Tactical Wedge"), TEXT("Urban Column"), TEXT("Frontal Line"), TEXT("Dispersed Cover") };
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor(255, 215, 80), FString::Printf(TEXT("[SQUAD FORMATION] %s"), FormNames[(uint8)NewFormation]));
	}
}

void UAAASquadManagerComponent::SetSquadRoE(EAAASquadRoE NewRoE)
{
	CurrentRoE = NewRoE;
	const TCHAR* RoENames[] = { TEXT("FIRE AT WILL (Aggressive)"), TEXT("RETURN FIRE ONLY (Defensive)"), TEXT("HOLD FIRE (Stealth)") };
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor(255, 120, 100), FString::Printf(TEXT("[SQUAD RoE] %s"), RoENames[(uint8)NewRoE]));
	}
}

int32 UAAASquadManagerComponent::GetAliveMemberCount() const
{
	int32 Count = 0;
	for (const AAAASquadMemberCharacter* M : SquadMembers)
	{
		if (M && !M->IsDowned()) ++Count;
	}
	return Count;
}

TArray<FAAASquadMemberData> UAAASquadManagerComponent::GetSquadStatus() const
{
	TArray<FAAASquadMemberData> OutData;
	for (AAAASquadMemberCharacter* M : SquadMembers)
	{
		if (!M) continue;
		FAAASquadMemberData Data;
		Data.Character = M;
		Data.Role = M->SquadRole;
		Data.Callsign = M->Callsign;
		Data.State = M->CurrentState;
		Data.Health = M->Health;
		Data.MaxHealth = M->MaxHealth;
		Data.Ammo = M->CurrentAmmo;
		Data.MaxAmmo = M->MagazineSize;
		Data.bIsDowned = M->IsDowned();
		Data.DesignatedLocation = M->GetCurrentDestination();
		OutData.Add(Data);
	}
	return OutData;
}

/* ============================================================================================ Tactical HUD */

void UAAASquadManagerComponent::DrawSquadHud(UCanvas* Canvas, APlayerController* PC)
{
	if (!Canvas || !PC || SquadMembers.Num() == 0 || !GEngine) return;

	const float Wd = (float)Canvas->SizeX;
	const float Ht = (float)Canvas->SizeY;
	const float Scale = Ht / 1080.f;

	UFont* SmallFont = GEngine->GetSmallFont();
	UFont* LargeFont = GEngine->GetLargeFont();

	auto Box = [&](float X, float Y, float W, float H, const FLinearColor& C)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};

	auto Text = [&](UFont* F, const FString& S, float X, float Y, float Sc, const FLinearColor& C)
	{
		Canvas->SetDrawColor(C.ToFColor(true));
		Canvas->DrawText(F, S, X, Y, Sc, Sc);
	};

	// 1. Draw 3D World Holographic Waypoint Marker
	if (bHasActiveWaypoint)
	{
		FVector2D ScreenPos;
		if (PC->ProjectWorldLocationToScreen(CurrentWaypoint, ScreenPos))
		{
			const float DistM = FVector::Dist(PlayerCharacter() ? PlayerCharacter()->GetActorLocation() : FVector::ZeroVector, CurrentWaypoint) / 100.f;
			const float Pulse = 0.8f + 0.2f * FMath::Sin((float)GetWorld()->GetTimeSeconds() * 8.f);
			const FLinearColor WpCol(0.2f, 0.85f, 1.0f, Pulse);

			// Diamond / Chevron
			Box(ScreenPos.X - 10.f * Scale, ScreenPos.Y - 10.f * Scale, 20.f * Scale, 20.f * Scale, WpCol);
			Box(ScreenPos.X - 6.f * Scale, ScreenPos.Y - 6.f * Scale, 12.f * Scale, 12.f * Scale, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			Text(SmallFont, FString::Printf(TEXT("[ SQUAD MOVE: %.1fm ]"), DistM), ScreenPos.X - 60.f * Scale, ScreenPos.Y + 16.f * Scale, 1.1f * Scale, WpCol);
		}
	}

	// 2. Draw Squad Status Cards at Bottom-Left
	const float CardW = 210.f * Scale;
	const float CardH = 48.f * Scale;
	const float StartX = 25.f * Scale;
	float StartY = Ht - 220.f * Scale;

	// Squad Header Banner
	Box(StartX, StartY - 26.f * Scale, CardW, 22.f * Scale, FLinearColor(0.04f, 0.05f, 0.07f, 0.88f));
	const TCHAR* FormStr = (CurrentFormation == EAAASquadFormation::Wedge) ? TEXT("WEDGE") : ((CurrentFormation == EAAASquadFormation::Column) ? TEXT("COLUMN") : TEXT("LINE"));
	Text(SmallFont, FString::Printf(TEXT("BRAVO TEAM  |  %s  |  %d/3 ACTIVE"), FormStr, GetAliveMemberCount()), StartX + 8.f * Scale, StartY - 22.f * Scale, 0.95f * Scale, FLinearColor(0.3f, 0.85f, 1.0f));

	for (int32 i = 0; i < SquadMembers.Num(); ++i)
	{
		AAAASquadMemberCharacter* M = SquadMembers[i];
		if (!M) continue;

		const bool bDown = M->IsDowned();
		const float HpFrac = FMath::Clamp(M->Health / FMath::Max(M->MaxHealth, 1.f), 0.f, 1.f);

		// Background Card
		Box(StartX, StartY, CardW, CardH, FLinearColor(0.05f, 0.06f, 0.08f, 0.85f));

		// Role Indicator Left Bar
		FLinearColor RoleCol = (M->SquadRole == EAAASquadRole::Assault) ? FLinearColor(0.2f, 0.6f, 1.0f) : ((M->SquadRole == EAAASquadRole::Support) ? FLinearColor(1.0f, 0.65f, 0.1f) : FLinearColor(0.3f, 1.0f, 0.4f));
		if (bDown) RoleCol = FLinearColor(1.f, 0.15f, 0.15f);
		Box(StartX, StartY, 4.f * Scale, CardH, RoleCol);

		// Callsign & Status Text
		const FString RoleTag = (M->SquadRole == EAAASquadRole::Assault) ? TEXT("ASSAULT") : ((M->SquadRole == EAAASquadRole::Support) ? TEXT("LMG") : TEXT("MARK"));
		Text(SmallFont, FString::Printf(TEXT("[%s] %s"), *RoleTag, *M->Callsign), StartX + 10.f * Scale, StartY + 6.f * Scale, 1.0f * Scale, bDown ? FLinearColor(1.f, 0.3f, 0.3f) : FLinearColor::White);

		// State Badge Text
		FString StateStr = TEXT("FOLLOWING");
		FLinearColor StateCol = FLinearColor(0.7f, 0.7f, 0.7f);
		if (bDown) { StateStr = TEXT("DOWNED!"); StateCol = FLinearColor(1.f, 0.1f, 0.1f); }
		else if (M->CurrentState == EAAASquadState::InCover) { StateStr = TEXT("IN COVER"); StateCol = FLinearColor(0.2f, 0.9f, 0.4f); }
		else if (M->CurrentState == EAAASquadState::Engaging) { StateStr = TEXT("ENGAGING"); StateCol = FLinearColor(1.f, 0.35f, 0.1f); }
		else if (M->CurrentState == EAAASquadState::Suppressing) { StateStr = TEXT("SUPPRESSING"); StateCol = FLinearColor(1.f, 0.8f, 0.1f); }
		else if (M->CurrentState == EAAASquadState::Holding) { StateStr = TEXT("HOLDING"); StateCol = FLinearColor(0.8f, 0.8f, 0.3f); }
		else if (M->CurrentState == EAAASquadState::MovingToPoint) { StateStr = TEXT("MOVING"); StateCol = FLinearColor(0.2f, 0.7f, 1.f); }
		Text(SmallFont, StateStr, StartX + CardW - 85.f * Scale, StartY + 6.f * Scale, 0.9f * Scale, StateCol);

		// Health Bar Background & Fill
		const float BarX = StartX + 10.f * Scale;
		const float BarY = StartY + 28.f * Scale;
		const float BarW = CardW - 75.f * Scale;
		const float BarH = 6.f * Scale;
		Box(BarX, BarY, BarW, BarH, FLinearColor(0.1f, 0.1f, 0.1f, 0.9f));

		const FLinearColor HpCol = bDown ? FLinearColor(1.f, 0.1f, 0.1f) : (HpFrac > 0.5f ? FLinearColor(0.15f, 0.85f, 0.3f) : FLinearColor(0.95f, 0.65f, 0.1f));
		Box(BarX, BarY, BarW * HpFrac, BarH, HpCol);

		// Ammo count
		Text(SmallFont, FString::Printf(TEXT("%d / %d"), M->CurrentAmmo, M->MagazineSize), StartX + CardW - 55.f * Scale, StartY + 24.f * Scale, 0.95f * Scale, FLinearColor(0.85f, 0.85f, 0.85f));

		StartY += CardH + 6.f * Scale;
	}
}
