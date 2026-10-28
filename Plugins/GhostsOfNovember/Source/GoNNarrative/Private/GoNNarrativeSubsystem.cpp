#include "GoNNarrativeSubsystem.h"

#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GoNDeveloperSettings.h"
#include "GoNNarrativeLog.h"
#include "GoNSaveGame.h"
#include "GoNStringConv.h"
#include "GoNTimelineValidator.h"
#include "GoNUnrealFileSystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Logic/GoNCampaignState.h"
#include "Logic/GoNMissionRunner.h"
#include "Logic/GoNNarrativeLoader.h"

namespace GoNNarrativeSubsystemDetail
{
	TAutoConsoleVariable<int32> CVarNarrativeDebug(
		TEXT("gon.narrative.debug"),
		0,
		TEXT("GoN Narrative: 1 = show the current mission/beat/consequences on screen."));

	void LogIssue(const GoN::Narrative::FIssue& Issue, bool bIncludeWarnings)
	{
		const FString Text = GoN::ToFString(Issue.ToString());
		if (Issue.Severity == GoN::Narrative::ESeverity::Error)
		{
			UE_LOG(LogGoNNarrative, Error, TEXT("GoN Narrative: %s"), *Text);
		}
		else if (Issue.Severity == GoN::Narrative::ESeverity::Warning && bIncludeWarnings)
		{
			UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: %s"), *Text);
		}
	}
}

// Owns the pure-core objects and receives the runner's callbacks.
class FGoNNarrativeRuntime final : public GoN::Narrative::IMissionListener
{
public:
	explicit FGoNNarrativeRuntime(UGoNNarrativeSubsystem& InOwner) : Owner(InOwner) {}

	UGoNNarrativeSubsystem& Owner;
	TUniquePtr<GoN::Narrative::FCampaign> Campaign;
	TUniquePtr<GoN::Narrative::FCampaignState> State;
	TUniquePtr<GoN::Narrative::FMissionRunner> Runner;
	GoN::Narrative::FIssueList Issues;
	FString NarrativeDirectory;

	// Blueprint handlers may call back into the subsystem from a broadcast; such calls are
	// queued and run once the runner has returned.
	int32 DispatchDepth = 0;
	TArray<FName> PendingEvents;
	FName PendingStart;

	struct FDispatchScope
	{
		explicit FDispatchScope(int32& InDepth) : Depth(InDepth) { ++Depth; }
		~FDispatchScope() { --Depth; }
		int32& Depth;
	};

	virtual void OnBeatEntered(const GoN::Narrative::FMission& Mission, const GoN::Narrative::FBeat& Beat) override
	{
		const FString BeatType = ANSI_TO_TCHAR(GoN::Narrative::BeatTypeName(Beat.Type));
		if (GoN::Narrative::IsEventBeat(Beat.Type))
		{
			UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: [%s] -> %s (%s), waiting for event '%s'"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Beat.Id), *BeatType, *GoN::ToFString(GoN::Narrative::FMissionRunner::CompletionEvent(Beat)));
		}
		else
		{
			UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: [%s] -> %s (%s)"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Beat.Id), *BeatType);
		}
		Owner.OnBeatEntered.Broadcast(GoN::ToFName(Mission.Id), GoN::ToFName(Beat.Id), BeatType);
	}

	virtual void OnBeatCompleted(const GoN::Narrative::FMission& Mission, const GoN::Narrative::FBeat& Beat) override
	{
		UE_LOG(LogGoNNarrative, Verbose, TEXT("GoN Narrative: [%s] done %s"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Beat.Id));
	}

	virtual void OnCheckpoint(const GoN::Narrative::FMission& Mission, const GoN::Narrative::FBeat& Beat) override
	{
		State->Progress.MissionId = Mission.Id;
		State->Progress.CheckpointBeatId = Beat.Id;
		UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: [%s] checkpoint %s"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Beat.Id));
		if (UGoNDeveloperSettings::Get()->bSaveAtCheckpoints)
		{
			Owner.SaveCheckpoint();
		}
		Owner.OnCheckpointReached.Broadcast(GoN::ToFName(Mission.Id), GoN::ToFName(Beat.Id));
	}

	virtual void OnMissionEnded(const GoN::Narrative::FMission& Mission, const std::string& Outcome) override
	{
		State->RecordMissionEnd(Mission.Id, Outcome);
		UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: [%s] ended: %s | %s"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Outcome), *GoN::ToFString(State->Consequences.DebugString()));
		Owner.OnMissionEnded.Broadcast(GoN::ToFName(Mission.Id), GoN::ToFString(Outcome));
	}

	virtual void OnRunnerMessage(const GoN::Narrative::FMission& Mission, GoN::Narrative::ESeverity Severity, const std::string& Message) override
	{
		const FString Text = FString::Printf(TEXT("GoN Narrative: [%s] %s"), *GoN::ToFString(Mission.Id), *GoN::ToFString(Message));
		switch (Severity)
		{
		case GoN::Narrative::ESeverity::Error: UE_LOG(LogGoNNarrative, Error, TEXT("%s"), *Text); break;
		case GoN::Narrative::ESeverity::Warning: UE_LOG(LogGoNNarrative, Warning, TEXT("%s"), *Text); break;
		case GoN::Narrative::ESeverity::Info: UE_LOG(LogGoNNarrative, Log, TEXT("%s"), *Text); break;
		}
	}
};

UGoNNarrativeSubsystem* UGoNNarrativeSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGoNNarrativeSubsystem>() : nullptr;
}

void UGoNNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Runtime = MakeShared<FGoNNarrativeRuntime>(*this);
	ReloadData();
	DebugDrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UGoNNarrativeSubsystem::DrawDebugOverlay));
}

void UGoNNarrativeSubsystem::Deinitialize()
{
	UDebugDrawService::Unregister(DebugDrawHandle);
	Runtime.Reset();
	Super::Deinitialize();
}

FString UGoNNarrativeSubsystem::GetNarrativeDirectory() const
{
	return UGoNTimelineValidator::GetNarrativeDirectory();
}

bool UGoNNarrativeSubsystem::ReloadData()
{
	if (!Runtime.IsValid())
	{
		return false;
	}
	if (Runtime->DispatchDepth > 0)
	{
		UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: cannot reload data from inside a narrative callback"));
		return false;
	}

	Runtime->Runner.Reset(); // holds references into the old campaign
	Runtime->NarrativeDirectory = GetNarrativeDirectory();
	TUniquePtr<GoN::Narrative::FCampaign> NewCampaign = MakeUnique<GoN::Narrative::FCampaign>();
	Runtime->Issues = GoN::Narrative::FIssueList();
	const FGoNUnrealFileSystem FileSystem;
	const bool bOk = GoN::Narrative::LoadAndValidateCampaign(FileSystem, GoN::ToStd(Runtime->NarrativeDirectory), *NewCampaign, Runtime->Issues);
	Runtime->Campaign = MoveTemp(NewCampaign);

	// Keep consequences across reloads; only re-point them at the new schema.
	if (!Runtime->State.IsValid())
	{
		Runtime->State = MakeUnique<GoN::Narrative::FCampaignState>(&Runtime->Campaign->Schema);
	}
	Runtime->State->Consequences.SetSchema(&Runtime->Campaign->Schema);

	const bool bLogWarnings = UGoNDeveloperSettings::Get()->bLogValidationWarnings;
	for (const GoN::Narrative::FIssue& Issue : Runtime->Issues.Items)
	{
		GoNNarrativeSubsystemDetail::LogIssue(Issue, bLogWarnings);
	}
	UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: loaded %d missions from %s (%d errors, %d warnings)"),
		static_cast<int32>(Runtime->Campaign->Missions.size()), *Runtime->NarrativeDirectory, Runtime->Issues.ErrorCount(), Runtime->Issues.WarningCount());
	return bOk;
}

bool UGoNNarrativeSubsystem::StartMission(FName MissionId)
{
	if (!Runtime.IsValid() || !Runtime->Campaign.IsValid())
	{
		return false;
	}
	if (Runtime->DispatchDepth > 0)
	{
		Runtime->PendingStart = MissionId;
		return true;
	}
	const GoN::Narrative::FMission* Mission = Runtime->Campaign->FindMission(GoN::ToStd(MissionId.ToString()));
	if (Mission == nullptr)
	{
		UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: unknown mission '%s' (gon.narrative.list shows the ids)"), *MissionId.ToString());
		return false;
	}

	Runtime->Runner.Reset();
	Runtime->State->Progress.MissionId = Mission->Id;
	Runtime->State->Progress.CheckpointBeatId.clear();
	Runtime->Runner = MakeUnique<GoN::Narrative::FMissionRunner>(*Mission, Runtime->State->Consequences, Runtime.Get());
	UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: start mission %s"), *MissionId.ToString());
	bool bStarted = false;
	{
		FGoNNarrativeRuntime::FDispatchScope Scope(Runtime->DispatchDepth);
		bStarted = Runtime->Runner->Start();
	}
	DrainPendingEvents();
	return bStarted;
}

bool UGoNNarrativeSubsystem::SendEvent(FName EventName)
{
	if (!Runtime.IsValid() || !Runtime->Runner.IsValid())
	{
		return false;
	}
	if (Runtime->DispatchDepth > 0)
	{
		Runtime->PendingEvents.Add(EventName);
		return true;
	}
	bool bConsumed = false;
	{
		FGoNNarrativeRuntime::FDispatchScope Scope(Runtime->DispatchDepth);
		bConsumed = Runtime->Runner->HandleEvent(GoN::ToStd(EventName.ToString()));
	}
	UE_LOG(LogGoNNarrative, Verbose, TEXT("GoN Narrative: event '%s' %s"), *EventName.ToString(), bConsumed ? TEXT("completed the beat") : TEXT("ignored"));
	DrainPendingEvents();
	return bConsumed;
}

bool UGoNNarrativeSubsystem::SkipCurrentBeat()
{
	if (!Runtime.IsValid() || !Runtime->Runner.IsValid() || Runtime->DispatchDepth > 0)
	{
		return false;
	}
	bool bSkipped = false;
	{
		FGoNNarrativeRuntime::FDispatchScope Scope(Runtime->DispatchDepth);
		bSkipped = Runtime->Runner->SkipCurrentBeat();
	}
	DrainPendingEvents();
	return bSkipped;
}

void UGoNNarrativeSubsystem::DrainPendingEvents()
{
	// Bounded: a handler that keeps queueing events cannot hang the game.
	for (int32 Guard = 0; Guard < 64 && Runtime.IsValid() && (Runtime->PendingEvents.Num() > 0 || !Runtime->PendingStart.IsNone()); ++Guard)
	{
		if (!Runtime->PendingStart.IsNone())
		{
			const FName Start = Runtime->PendingStart;
			Runtime->PendingStart = NAME_None;
			Runtime->PendingEvents.Reset();
			StartMission(Start);
			continue;
		}
		const FName Event = Runtime->PendingEvents[0];
		Runtime->PendingEvents.RemoveAt(0);
		SendEvent(Event);
	}
}

bool UGoNNarrativeSubsystem::SetFlag(const FString& Key, bool bValue)
{
	if (!Runtime.IsValid() || !Runtime->State.IsValid())
	{
		return false;
	}
	GoN::Narrative::FEffect Effect;
	Effect.Op = bValue ? GoN::Narrative::EEffectOp::SetFlag : GoN::Narrative::EEffectOp::ClearFlag;
	Effect.Key = GoN::ToStd(Key);
	std::string Error;
	if (!Runtime->State->Consequences.Apply(Effect, &Error))
	{
		UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: SetFlag refused: %s"), *GoN::ToFString(Error));
		return false;
	}
	return true;
}

bool UGoNNarrativeSubsystem::HasFlag(const FString& Key) const
{
	return Runtime.IsValid() && Runtime->State.IsValid() && Runtime->State->Consequences.HasFlag(GoN::ToStd(Key));
}

float UGoNNarrativeSubsystem::GetStat(const FString& Key) const
{
	return (Runtime.IsValid() && Runtime->State.IsValid()) ? static_cast<float>(Runtime->State->Consequences.GetStat(GoN::ToStd(Key))) : 0.0f;
}

FName UGoNNarrativeSubsystem::GetCurrentMissionId() const
{
	return (Runtime.IsValid() && Runtime->Runner.IsValid()) ? GoN::ToFName(Runtime->Runner->GetMission().Id) : NAME_None;
}

FName UGoNNarrativeSubsystem::GetCurrentBeatId() const
{
	const GoN::Narrative::FBeat* Beat = (Runtime.IsValid() && Runtime->Runner.IsValid()) ? Runtime->Runner->GetCurrentBeat() : nullptr;
	return Beat ? GoN::ToFName(Beat->Id) : NAME_None;
}

bool UGoNNarrativeSubsystem::SaveCheckpoint()
{
	if (!Runtime.IsValid() || !Runtime->State.IsValid())
	{
		return false;
	}
	UGoNSaveGame* Save = Cast<UGoNSaveGame>(UGameplayStatics::CreateSaveGameObject(UGoNSaveGame::StaticClass()));
	if (Save == nullptr)
	{
		return false;
	}
	Save->CampaignStateJson = GoN::ToFString(Runtime->State->Serialize());
	Save->MissionId = GoN::ToFString(Runtime->State->Progress.MissionId);
	Save->CheckpointId = GoN::ToFString(Runtime->State->Progress.CheckpointBeatId);
	Save->SavedAtUtc = FDateTime::UtcNow();
	const FString& Slot = UGoNDeveloperSettings::Get()->CheckpointSlotName;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: checkpoint save to slot '%s' %s (mission '%s', checkpoint '%s')"), *Slot, bSaved ? TEXT("ok") : TEXT("FAILED"), *Save->MissionId, *Save->CheckpointId);
	return bSaved;
}

bool UGoNNarrativeSubsystem::LoadCheckpoint()
{
	if (!Runtime.IsValid() || !Runtime->Campaign.IsValid() || Runtime->DispatchDepth > 0)
	{
		return false;
	}
	const FString& Slot = UGoNDeveloperSettings::Get()->CheckpointSlotName;
	UGoNSaveGame* Save = UGameplayStatics::DoesSaveGameExist(Slot, 0) ? Cast<UGoNSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
	if (Save == nullptr)
	{
		UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: no checkpoint in slot '%s'"), *Slot);
		return false;
	}

	std::string Error;
	std::vector<std::string> Warnings;
	Runtime->Runner.Reset();
	if (!Runtime->State->Deserialize(GoN::ToStd(Save->CampaignStateJson), &Error, &Warnings))
	{
		UE_LOG(LogGoNNarrative, Error, TEXT("GoN Narrative: checkpoint load failed: %s"), *GoN::ToFString(Error));
		return false;
	}
	for (const std::string& Warning : Warnings)
	{
		UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: checkpoint: %s"), *GoN::ToFString(Warning));
	}

	const GoN::Narrative::FCampaignProgress& Progress = Runtime->State->Progress;
	UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: checkpoint loaded (saved %s UTC) | %s"), *Save->SavedAtUtc.ToString(), *GoN::ToFString(Runtime->State->Consequences.DebugString()));
	if (Progress.MissionId.empty())
	{
		return true;
	}
	const GoN::Narrative::FMission* Mission = Runtime->Campaign->FindMission(Progress.MissionId);
	if (Mission == nullptr)
	{
		UE_LOG(LogGoNNarrative, Error, TEXT("GoN Narrative: saved mission '%s' no longer exists"), *GoN::ToFString(Progress.MissionId));
		return false;
	}
	Runtime->Runner = MakeUnique<GoN::Narrative::FMissionRunner>(*Mission, Runtime->State->Consequences, Runtime.Get());
	bool bResumed = false;
	{
		FGoNNarrativeRuntime::FDispatchScope Scope(Runtime->DispatchDepth);
		bResumed = Progress.CheckpointBeatId.empty() ? Runtime->Runner->Start() : Runtime->Runner->Start(Progress.CheckpointBeatId);
	}
	DrainPendingEvents();
	return bResumed;
}

bool UGoNNarrativeSubsystem::AutoRunMission(FName MissionId, int32 MaxSteps)
{
	if (!StartMission(MissionId))
	{
		return false;
	}
	for (int32 Step = 0; Step < MaxSteps && Runtime->Runner.IsValid() && Runtime->Runner->GetState() == GoN::Narrative::ERunState::Running; ++Step)
	{
		if (!SkipCurrentBeat())
		{
			break;
		}
	}
	const bool bEnded = Runtime->Runner.IsValid() && Runtime->Runner->GetState() == GoN::Narrative::ERunState::Ended;
	UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: autorun %s %s"), *MissionId.ToString(), bEnded ? TEXT("reached its end") : TEXT("did NOT reach an end beat"));
	return bEnded;
}

void UGoNNarrativeSubsystem::Tick(float DeltaTime)
{
	++TickCount;
	// DeltaTime comes from the world tick: already dilated, and the world does not tick us while paused.
	if (Runtime.IsValid() && Runtime->Runner.IsValid() && DeltaTime > 0.0f && Runtime->DispatchDepth == 0)
	{
		{
			FGoNNarrativeRuntime::FDispatchScope Scope(Runtime->DispatchDepth);
			Runtime->Runner->Tick(DeltaTime);
		}
		DrainPendingEvents();
	}
}

ETickableTickType UGoNNarrativeSubsystem::GetTickableTickType() const
{
	// The class default object must never tick.
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGoNNarrativeSubsystem::IsTickable() const
{
	return Runtime.IsValid();
}

UWorld* UGoNNarrativeSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

TStatId UGoNNarrativeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGoNNarrativeSubsystem, STATGROUP_Tickables);
}

void UGoNNarrativeSubsystem::DrawDebugOverlay(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (Canvas == nullptr || GoNNarrativeSubsystemDetail::CVarNarrativeDebug.GetValueOnGameThread() <= 0)
	{
		return;
	}
	// Every game viewport draws through this service; only draw in our own game instance's world.
	const UWorld* OwnWorld = GetTickableGameObjectWorld();
	if (PlayerController != nullptr && PlayerController->GetWorld() != OwnWorld)
	{
		return;
	}
	// Ids and keys only (ASCII): Arabic text must go through the shaped HUD path, not Canvas->DrawText.
	Canvas->SetDrawColor(FColor::Yellow);
	Canvas->DrawText(GEngine->GetSmallFont(), TEXT("GoN Narrative: ") + DescribeState(), 20.0f, 60.0f);
}

TArray<FName> UGoNNarrativeSubsystem::GetMissionIds() const
{
	TArray<FName> Ids;
	if (Runtime.IsValid() && Runtime->Campaign.IsValid())
	{
		for (const GoN::Narrative::FMission& Mission : Runtime->Campaign->Missions)
		{
			Ids.Add(GoN::ToFName(Mission.Id));
		}
	}
	return Ids;
}

TArray<FString> UGoNNarrativeSubsystem::GetValidationMessages(bool bIncludeWarnings) const
{
	TArray<FString> Messages;
	if (Runtime.IsValid())
	{
		for (const GoN::Narrative::FIssue& Issue : Runtime->Issues.Items)
		{
			if (Issue.Severity == GoN::Narrative::ESeverity::Error || bIncludeWarnings)
			{
				Messages.Add(GoN::ToFString(Issue.ToString()));
			}
		}
	}
	return Messages;
}

int32 UGoNNarrativeSubsystem::GetValidationErrorCount() const
{
	return Runtime.IsValid() ? Runtime->Issues.ErrorCount() : 0;
}

FString UGoNNarrativeSubsystem::DescribeState() const
{
	if (!Runtime.IsValid() || !Runtime->State.IsValid())
	{
		return TEXT("GoN Narrative: not initialised");
	}
	FString Text;
	if (Runtime->Runner.IsValid())
	{
		const GoN::Narrative::FMissionRunner& Runner = *Runtime->Runner;
		const GoN::Narrative::FBeat* Beat = Runner.GetCurrentBeat();
		Text = FString::Printf(TEXT("mission %s [%s]"), *GoN::ToFString(Runner.GetMission().Id), ANSI_TO_TCHAR(GoN::Narrative::RunStateName(Runner.GetState())));
		if (Beat != nullptr)
		{
			Text += FString::Printf(TEXT(" | beat %s (%s)"), *GoN::ToFString(Beat->Id), ANSI_TO_TCHAR(GoN::Narrative::BeatTypeName(Beat->Type)));
			if (Runner.GetState() == GoN::Narrative::ERunState::Running && GoN::Narrative::IsEventBeat(Beat->Type))
			{
				Text += FString::Printf(TEXT(" waiting for '%s'"), *GoN::ToFString(GoN::Narrative::FMissionRunner::CompletionEvent(*Beat)));
			}
			else if (Runner.GetState() == GoN::Narrative::ERunState::Running && Beat->Type == GoN::Narrative::EBeatType::Wait)
			{
				Text += FString::Printf(TEXT(" %.1fs left"), Runner.GetWaitRemaining());
			}
		}
		if (Runner.GetState() == GoN::Narrative::ERunState::Ended)
		{
			Text += FString::Printf(TEXT(" | outcome %s"), *GoN::ToFString(Runner.GetOutcome()));
		}
	}
	else
	{
		Text = TEXT("no mission running");
	}
	Text += TEXT(" | ") + GoN::ToFString(Runtime->State->Consequences.DebugString());
	Text += FString::Printf(TEXT(" | ticks %llu"), static_cast<unsigned long long>(TickCount));
	return Text;
}
