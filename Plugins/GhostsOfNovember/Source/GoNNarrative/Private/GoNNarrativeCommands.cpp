// gon.narrative.* and gon.timeline.* console commands (brief §4: every system has debug commands).
#include "GoNNarrativeLog.h"
#include "GoNNarrativeSubsystem.h"
#include "GoNTimelineValidator.h"
#include "HAL/IConsoleManager.h"

namespace GoNNarrativeCommandsDetail
{
	UGoNNarrativeSubsystem* Subsystem(UWorld* World)
	{
		UGoNNarrativeSubsystem* Found = UGoNNarrativeSubsystem::Get(World);
		if (Found == nullptr)
		{
			UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: no game instance here; run the command during Play (PIE)."));
		}
		return Found;
	}

	void PrintState(UGoNNarrativeSubsystem& Narrative)
	{
		UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: %s"), *Narrative.DescribeState());
	}

	FAutoConsoleCommandWithWorldAndArgs ListCommand(
		TEXT("gon.narrative.list"),
		TEXT("GoN Narrative: list mission ids."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				for (const FName& Id : Narrative->GetMissionIds())
				{
					UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative:   %s"), *Id.ToString());
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs StartCommand(
		TEXT("gon.narrative.start"),
		TEXT("GoN Narrative: gon.narrative.start <missionId> - start a mission from its first beat."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGoNNarrativeSubsystem* Narrative = Subsystem(World);
			if (Narrative && Args.Num() >= 1)
			{
				Narrative->StartMission(FName(*Args[0]));
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs EventCommand(
		TEXT("gon.narrative.event"),
		TEXT("GoN Narrative: gon.narrative.event <eventName> - send a gameplay event (completes the beat waiting for it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGoNNarrativeSubsystem* Narrative = Subsystem(World);
			if (Narrative && Args.Num() >= 1)
			{
				const bool bConsumed = Narrative->SendEvent(FName(*Args[0]));
				UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: event '%s' %s"), *Args[0], bConsumed ? TEXT("completed the beat") : TEXT("was not awaited"));
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SkipCommand(
		TEXT("gon.narrative.skip"),
		TEXT("GoN Narrative: complete the current waiting beat."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				Narrative->SkipCurrentBeat();
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs AutoRunCommand(
		TEXT("gon.narrative.autorun"),
		TEXT("GoN Narrative: gon.narrative.autorun [missionId] - start and skip every beat to the end (default gym_test_mission)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				Narrative->AutoRunMission(FName(Args.Num() >= 1 ? *Args[0] : TEXT("gym_test_mission")));
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs StateCommand(
		TEXT("gon.narrative.state"),
		TEXT("GoN Narrative: print mission, beat and consequence state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs FlagCommand(
		TEXT("gon.narrative.flag"),
		TEXT("GoN Narrative: gon.narrative.flag <key> [0|1] - set/clear a declared flag (e.g. choice.group_power)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGoNNarrativeSubsystem* Narrative = Subsystem(World);
			if (Narrative && Args.Num() >= 1)
			{
				Narrative->SetFlag(Args[0], Args.Num() < 2 || Args[1] != TEXT("0"));
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SaveCommand(
		TEXT("gon.narrative.save"),
		TEXT("GoN Narrative: save the campaign state to the checkpoint slot now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				Narrative->SaveCheckpoint();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs LoadCommand(
		TEXT("gon.narrative.load"),
		TEXT("GoN Narrative: load the checkpoint slot and resume its mission."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				Narrative->LoadCheckpoint();
				PrintState(*Narrative);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ReloadCommand(
		TEXT("gon.narrative.reload"),
		TEXT("GoN Narrative: re-read Data/Narrative (stops the running mission, keeps consequences)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGoNNarrativeSubsystem* Narrative = Subsystem(World))
			{
				Narrative->ReloadData();
			}
		}));

	FAutoConsoleCommand ValidateCommand(
		TEXT("gon.timeline.validate"),
		TEXT("GoN Narrative: validate timeline, history rules and mission graphs from disk (works in the editor without Play). Add 'errors' to hide warnings."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const bool bWarnings = !(Args.Num() >= 1 && Args[0] == TEXT("errors"));
			TArray<FString> Messages;
			const int32 Errors = UGoNTimelineValidator::ValidateNarrativeData(Messages, bWarnings);
			for (const FString& Message : Messages)
			{
				if (Message.StartsWith(TEXT("error")))
				{
					UE_LOG(LogGoNNarrative, Error, TEXT("GoN Narrative: %s"), *Message);
				}
				else
				{
					UE_LOG(LogGoNNarrative, Warning, TEXT("GoN Narrative: %s"), *Message);
				}
			}
			UE_LOG(LogGoNNarrative, Display, TEXT("GoN Narrative: timeline validation finished with %d error(s)"), Errors);
		}));
}
