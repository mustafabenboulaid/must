#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "GoNNarrativeSubsystem.generated.h"

class APlayerController;
class FGoNNarrativeRuntime;
class UCanvas;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGoNBeatEnteredSignature, FName, MissionId, FName, BeatId, const FString&, BeatType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGoNCheckpointSignature, FName, MissionId, FName, CheckpointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGoNMissionEndedSignature, FName, MissionId, const FString&, Outcome);

// Thin Unreal wrapper over the pure narrative core (Logic/GoNMissionRunner.h):
// loads Data/Narrative, validates it, runs one mission at a time, forwards gameplay
// events, ticks wait beats (with the game world: pause and time dilation apply) and saves at
// checkpoints. Lives for the whole game session, so consequences carry across map changes.
UCLASS()
class GONNARRATIVE_API UGoNNarrativeSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UGoNNarrativeSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Re-reads the JSON files (tune data without recompiling). Stops the running mission.
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool ReloadData();

	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool StartMission(FName MissionId);

	// Completes the current beat when EventName matches its "completeOn" event.
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool SendEvent(FName EventName);

	// Debug: completes the current objective/trigger/dialogue/cinematic/scene/wait beat.
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool SkipCurrentBeat();

	// Player choices from UI (e.g. "choice.group_power" picked in the briefing).
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool SetFlag(const FString& Key, bool bValue);

	UFUNCTION(BlueprintPure, Category = "GoN|Narrative")
	bool HasFlag(const FString& Key) const;

	UFUNCTION(BlueprintPure, Category = "GoN|Narrative")
	float GetStat(const FString& Key) const;

	UFUNCTION(BlueprintPure, Category = "GoN|Narrative")
	FName GetCurrentMissionId() const;

	UFUNCTION(BlueprintPure, Category = "GoN|Narrative")
	FName GetCurrentBeatId() const;

	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool SaveCheckpoint();

	// Restores consequences and resumes the saved mission at its checkpoint.
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	bool LoadCheckpoint();

	// Starts MissionId and skips every beat until it ends (or MaxSteps). For the M1 check.
	bool AutoRunMission(FName MissionId, int32 MaxSteps = 500);

	TArray<FName> GetMissionIds() const;
	TArray<FString> GetValidationMessages(bool bIncludeWarnings) const;
	int32 GetValidationErrorCount() const;
	FString DescribeState() const;
	FString GetNarrativeDirectory() const;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

	UPROPERTY(BlueprintAssignable, Category = "GoN|Narrative")
	FGoNBeatEnteredSignature OnBeatEntered;

	UPROPERTY(BlueprintAssignable, Category = "GoN|Narrative")
	FGoNCheckpointSignature OnCheckpointReached;

	UPROPERTY(BlueprintAssignable, Category = "GoN|Narrative")
	FGoNMissionEndedSignature OnMissionEnded;

private:
	friend class APlayerController;
class FGoNNarrativeRuntime;
class UCanvas;

	void DrainPendingEvents();
	// `gon.narrative.debug 1` overlay, drawn on the game viewport canvas every frame.
	void DrawDebugOverlay(UCanvas* Canvas, APlayerController* PlayerController);

	// Pure-core state lives in the .cpp (TSharedPtr tolerates the incomplete type here).
	TSharedPtr<FGoNNarrativeRuntime> Runtime;
	FDelegateHandle DebugDrawHandle;
	uint64 TickCount = 0;
};
