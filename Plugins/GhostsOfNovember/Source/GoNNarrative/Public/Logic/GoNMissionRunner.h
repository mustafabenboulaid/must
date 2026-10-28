// Ghosts of November — runs one mission's beat graph from data (pure C++).
// Event beats wait for gameplay events, wait beats for time, branch/checkpoint/end beats
// resolve instantly. The Unreal subsystem forwards events and ticks; this class decides.
#pragma once

#include "Logic/GoNConsequences.h"
#include "Logic/GoNNarrativeTypes.h"

#include <string>

namespace GoN::Narrative
{
	class IMissionListener
	{
	public:
		virtual ~IMissionListener() = default;
		virtual void OnBeatEntered(const FMission& Mission, const FBeat& Beat) { (void)Mission; (void)Beat; }
		virtual void OnBeatCompleted(const FMission& Mission, const FBeat& Beat) { (void)Mission; (void)Beat; }
		virtual void OnCheckpoint(const FMission& Mission, const FBeat& Beat) { (void)Mission; (void)Beat; }
		virtual void OnMissionEnded(const FMission& Mission, const std::string& Outcome) { (void)Mission; (void)Outcome; }
		virtual void OnRunnerMessage(const FMission& Mission, ESeverity Severity, const std::string& Message) { (void)Mission; (void)Severity; (void)Message; }
	};

	enum class ERunState
	{
		Idle,
		Running,
		Ended,
		Failed // data error at runtime (missing beat, effect refused, instant loop)
	};

	inline const char* RunStateName(ERunState State)
	{
		switch (State)
		{
		case ERunState::Idle: return "idle";
		case ERunState::Running: return "running";
		case ERunState::Ended: return "ended";
		case ERunState::Failed: return "failed";
		}
		return "?";
	}

	class FMissionRunner
	{
	public:
		FMissionRunner(const FMission& InMission, FConsequenceState& InState, IMissionListener* InListener = nullptr);

		// Starts at the mission's start beat, or resumes at a checkpoint beat id.
		bool Start(const std::string& ResumeBeatId = std::string());

		// Returns true when the event completed the current beat.
		bool HandleEvent(const std::string& EventName);

		void Tick(double DeltaSeconds);

		// Debug: completes the current waiting beat as if its event arrived.
		bool SkipCurrentBeat();

		ERunState GetState() const { return State; }
		const FBeat* GetCurrentBeat() const { return Current; }
		const std::string& GetOutcome() const { return Outcome; }
		const std::string& GetLastCheckpoint() const { return LastCheckpoint; }
		double GetWaitRemaining() const { return WaitRemaining; }
		const FMission& GetMission() const { return Mission; }

		// Event that completes an event beat (explicit CompleteOn or "<beatId>.done").
		static std::string CompletionEvent(const FBeat& Beat);

	private:
		static constexpr int MaxInstantTransitions = 256;

		const FMission& Mission;
		FConsequenceState& StateRef;
		IMissionListener* Listener = nullptr;

		ERunState State = ERunState::Idle;
		const FBeat* Current = nullptr;
		double WaitRemaining = 0.0;
		std::string Outcome;
		std::string LastCheckpoint;

		void Enter(const std::string& BeatId);
		void CompleteCurrent();
		std::string ResolveNext(const FBeat& Beat) const;
		void ApplyEffects(const FBeat& Beat, const std::vector<FEffect>& Effects);
		void Fail(const std::string& Message);
	};

	// Static graph checks: ids, references, reachability, instant loops, §12 checklist.
	void ValidateMissionGraph(const FMission& Mission, const FConsequenceSchema& Schema, FIssueList& Issues);
} // namespace GoN::Narrative
