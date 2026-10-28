#include "Logic/GoNMissionRunner.h"

#include <map>
#include <set>
#include <vector>

namespace GoN::Narrative
{
	FMissionRunner::FMissionRunner(const FMission& InMission, FConsequenceState& InState, IMissionListener* InListener)
		: Mission(InMission)
		, StateRef(InState)
		, Listener(InListener)
	{
	}

	std::string FMissionRunner::CompletionEvent(const FBeat& Beat)
	{
		return Beat.CompleteOn.empty() ? Beat.Id + ".done" : Beat.CompleteOn;
	}

	bool FMissionRunner::Start(const std::string& ResumeBeatId)
	{
		State = ERunState::Running;
		Current = nullptr;
		Outcome.clear();
		WaitRemaining = 0.0;

		if (ResumeBeatId.empty())
		{
			LastCheckpoint.clear();
			Enter(Mission.StartBeat);
			return State != ERunState::Failed;
		}

		// Resuming: the saved state already includes the checkpoint's OnEnter effects,
		// so continue from the point right after OnCheckpoint fired.
		const FBeat* Checkpoint = Mission.FindBeat(ResumeBeatId);
		if (Checkpoint == nullptr || Checkpoint->Type != EBeatType::Checkpoint)
		{
			Fail("cannot resume: '" + ResumeBeatId + "' is not a checkpoint beat");
			return false;
		}
		Current = Checkpoint;
		LastCheckpoint = Checkpoint->Id;
		if (Listener)
		{
			Listener->OnRunnerMessage(Mission, ESeverity::Info, "resumed at checkpoint '" + Checkpoint->Id + "'");
		}
		CompleteCurrent();
		return State != ERunState::Failed;
	}

	bool FMissionRunner::HandleEvent(const std::string& EventName)
	{
		if (State != ERunState::Running || Current == nullptr || !IsEventBeat(Current->Type))
		{
			return false;
		}
		if (CompletionEvent(*Current) != EventName)
		{
			return false;
		}
		CompleteCurrent();
		return true;
	}

	void FMissionRunner::Tick(double DeltaSeconds)
	{
		if (State != ERunState::Running || Current == nullptr || Current->Type != EBeatType::Wait)
		{
			return;
		}
		WaitRemaining -= DeltaSeconds;
		if (WaitRemaining <= 0.0)
		{
			WaitRemaining = 0.0;
			CompleteCurrent();
		}
	}

	bool FMissionRunner::SkipCurrentBeat()
	{
		if (State != ERunState::Running || Current == nullptr)
		{
			return false;
		}
		if (!IsEventBeat(Current->Type) && Current->Type != EBeatType::Wait)
		{
			return false;
		}
		WaitRemaining = 0.0;
		CompleteCurrent();
		return true;
	}

	void FMissionRunner::ApplyEffects(const FBeat& Beat, const std::vector<FEffect>& Effects)
	{
		std::vector<std::string> Errors;
		StateRef.ApplyAll(Effects, &Errors);
		if (Listener)
		{
			for (const std::string& Error : Errors)
			{
				// Data bug: the validator reports it as an error; at runtime keep the mission going.
				Listener->OnRunnerMessage(Mission, ESeverity::Warning, "beat '" + Beat.Id + "': effect refused: " + Error);
			}
		}
	}

	std::string FMissionRunner::ResolveNext(const FBeat& Beat) const
	{
		if (Beat.Type == EBeatType::Branch)
		{
			for (const FBranch& Branch : Beat.Branches)
			{
				if (StateRef.EvaluateAll(Branch.Conditions))
				{
					return Branch.Next;
				}
			}
			return Beat.ElseNext;
		}
		return Beat.Next;
	}

	void FMissionRunner::Fail(const std::string& Message)
	{
		State = ERunState::Failed;
		if (Listener)
		{
			Listener->OnRunnerMessage(Mission, ESeverity::Error, Message);
		}
	}

	void FMissionRunner::CompleteCurrent()
	{
		const FBeat* Beat = Current;
		if (Beat == nullptr)
		{
			return;
		}
		ApplyEffects(*Beat, Beat->OnComplete);
		if (Listener)
		{
			Listener->OnBeatCompleted(Mission, *Beat);
		}
		const std::string NextId = ResolveNext(*Beat);
		if (NextId.empty())
		{
			Fail("beat '" + Beat->Id + "' has no next beat");
			return;
		}
		Enter(NextId);
	}

	void FMissionRunner::Enter(const std::string& BeatId)
	{
		std::string NextId = BeatId;
		for (int Transition = 0; Transition < MaxInstantTransitions; ++Transition)
		{
			const FBeat* Beat = Mission.FindBeat(NextId);
			if (Beat == nullptr)
			{
				Fail("missing beat '" + NextId + "'");
				return;
			}
			Current = Beat;
			ApplyEffects(*Beat, Beat->OnEnter);
			if (Listener)
			{
				Listener->OnBeatEntered(Mission, *Beat);
			}

			if (Beat->Type == EBeatType::Wait && Beat->WaitSeconds > 0.0)
			{
				WaitRemaining = Beat->WaitSeconds;
				return;
			}
			if (IsEventBeat(Beat->Type))
			{
				return;
			}

			// Instant beats (and zero-length waits) complete immediately.
			if (Beat->Type == EBeatType::Checkpoint)
			{
				LastCheckpoint = Beat->Id;
				if (Listener)
				{
					Listener->OnCheckpoint(Mission, *Beat);
				}
			}
			ApplyEffects(*Beat, Beat->OnComplete);
			if (Listener)
			{
				Listener->OnBeatCompleted(Mission, *Beat);
			}
			if (Beat->Type == EBeatType::End)
			{
				State = ERunState::Ended;
				Outcome = Beat->Outcome.empty() ? std::string("success") : Beat->Outcome;
				if (Listener)
				{
					Listener->OnMissionEnded(Mission, Outcome);
				}
				return;
			}
			NextId = ResolveNext(*Beat);
			if (NextId.empty())
			{
				Fail("beat '" + Beat->Id + "' has no next beat");
				return;
			}
		}
		Fail("more than " + std::to_string(MaxInstantTransitions) + " instant transitions: loop of branch/checkpoint beats near '" + NextId + "'");
	}

	// ---- Static validation ----------------------------------------------------------

	namespace MissionGraphDetail
	{
		std::vector<std::string> Successors(const FBeat& Beat)
		{
			std::vector<std::string> Out;
			if (Beat.Type == EBeatType::End)
			{
				return Out;
			}
			if (Beat.Type == EBeatType::Branch)
			{
				for (const FBranch& Branch : Beat.Branches)
				{
					Out.push_back(Branch.Next);
				}
				Out.push_back(Beat.ElseNext);
			}
			else
			{
				Out.push_back(Beat.Next);
			}
			return Out;
		}

		bool CompletesInstantly(const FBeat& Beat)
		{
			return IsInstantBeat(Beat.Type) || (Beat.Type == EBeatType::Wait && Beat.WaitSeconds <= 0.0);
		}

		void CheckEffects(const FMission& Mission, const FBeat& Beat, const std::vector<FEffect>& Effects, const FConsequenceSchema& Schema, FIssueList& Issues)
		{
			for (const FEffect& Effect : Effects)
			{
				std::string Error;
				if (!IsEffectAllowed(Schema, Effect, &Error))
				{
					Issues.Add(ESeverity::Error, "mission.effect", Mission.Id + "/" + Beat.Id, Error);
				}
			}
		}
	} // namespace MissionGraphDetail

	void ValidateMissionGraph(const FMission& Mission, const FConsequenceSchema& Schema, FIssueList& Issues)
	{
		using namespace MissionGraphDetail;
		const std::string& Ctx = Mission.Id;

		if (Mission.Id.empty())
		{
			Issues.Add(ESeverity::Error, "mission.id", Mission.SourceFile, "mission has no id");
		}
		if (Mission.Beats.empty())
		{
			Issues.Add(ESeverity::Error, "mission.empty", Ctx, "mission has no beats");
			return;
		}

		std::map<std::string, const FBeat*> ById;
		for (const FBeat& Beat : Mission.Beats)
		{
			if (Beat.Id.empty())
			{
				Issues.Add(ESeverity::Error, "beat.id", Ctx, "a beat has no id");
				continue;
			}
			if (!ById.emplace(Beat.Id, &Beat).second)
			{
				Issues.Add(ESeverity::Error, "beat.duplicate", Ctx + "/" + Beat.Id, "duplicate beat id");
			}
		}

		std::set<std::string> LineIds;
		for (const FDialogueLine& Line : Mission.Lines)
		{
			if (Line.Id.empty() || !LineIds.insert(Line.Id).second)
			{
				Issues.Add(ESeverity::Error, "line.id", Ctx + "/" + Line.Id, "dialogue line id is empty or duplicated");
			}
		}

		if (ById.find(Mission.StartBeat) == ById.end())
		{
			Issues.Add(ESeverity::Error, "mission.start", Ctx, "start beat '" + Mission.StartBeat + "' does not exist");
		}

		for (const FBeat& Beat : Mission.Beats)
		{
			const std::string BeatCtx = Ctx + "/" + Beat.Id;
			for (const std::string& Next : Successors(Beat))
			{
				if (Next.empty())
				{
					Issues.Add(ESeverity::Error, "beat.next_missing", BeatCtx, Beat.Type == EBeatType::Branch ? "branch needs every 'next' and an 'else'" : "beat has no 'next'");
				}
				else if (ById.find(Next) == ById.end())
				{
					Issues.Add(ESeverity::Error, "beat.next_unknown", BeatCtx, "next beat '" + Next + "' does not exist");
				}
			}
			if (Beat.Type == EBeatType::Branch && Beat.Branches.empty())
			{
				Issues.Add(ESeverity::Warning, "beat.branch_empty", BeatCtx, "branch has no conditions; it always takes 'else'");
			}
			if (Beat.Type == EBeatType::Wait && Beat.WaitSeconds < 0.0)
			{
				Issues.Add(ESeverity::Error, "beat.wait_negative", BeatCtx, "wait seconds must be >= 0");
			}
			if (Beat.Type == EBeatType::Dialogue)
			{
				if (Beat.LineIds.empty())
				{
					Issues.Add(ESeverity::Error, "beat.dialogue_empty", BeatCtx, "dialogue beat lists no lines");
				}
				for (const std::string& LineId : Beat.LineIds)
				{
					if (LineIds.count(LineId) == 0)
					{
						Issues.Add(ESeverity::Error, "beat.line_unknown", BeatCtx, "line '" + LineId + "' is not defined in the mission");
					}
				}
			}
			if ((Beat.Type == EBeatType::Cinematic || Beat.Type == EBeatType::Scene) && Beat.Asset.empty())
			{
				Issues.Add(ESeverity::Warning, "beat.asset_missing", BeatCtx, std::string(BeatTypeName(Beat.Type)) + " beat has no asset yet (placeholder)");
			}
			CheckEffects(Mission, Beat, Beat.OnEnter, Schema, Issues);
			CheckEffects(Mission, Beat, Beat.OnComplete, Schema, Issues);
			for (const FBranch& Branch : Beat.Branches)
			{
				for (const FCondition& Condition : Branch.Conditions)
				{
					std::string Error;
					if (!IsConditionKnown(Schema, Condition, &Error))
					{
						Issues.Add(ESeverity::Error, "beat.condition", BeatCtx, Error);
					}
				}
			}
		}

		// Reachability from the start beat.
		std::set<std::string> Reached;
		std::vector<std::string> Stack;
		if (ById.count(Mission.StartBeat) > 0)
		{
			Stack.push_back(Mission.StartBeat);
		}
		while (!Stack.empty())
		{
			const std::string Id = Stack.back();
			Stack.pop_back();
			if (!Reached.insert(Id).second)
			{
				continue;
			}
			const auto It = ById.find(Id);
			if (It == ById.end())
			{
				continue;
			}
			for (const std::string& Next : Successors(*It->second))
			{
				if (ById.count(Next) > 0 && Reached.count(Next) == 0)
				{
					Stack.push_back(Next);
				}
			}
		}
		for (const FBeat& Beat : Mission.Beats)
		{
			if (Reached.count(Beat.Id) == 0)
			{
				Issues.Add(ESeverity::Warning, "beat.unreachable", Ctx + "/" + Beat.Id, "beat cannot be reached from the start beat");
			}
		}

		// Every reachable beat must be able to finish the mission.
		std::set<std::string> CanFinish;
		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (const FBeat& Beat : Mission.Beats)
			{
				if (CanFinish.count(Beat.Id) > 0)
				{
					continue;
				}
				bool bFinishes = Beat.Type == EBeatType::End;
				for (const std::string& Next : Successors(Beat))
				{
					bFinishes = bFinishes || CanFinish.count(Next) > 0;
				}
				if (bFinishes)
				{
					CanFinish.insert(Beat.Id);
					bChanged = true;
				}
			}
		}
		if (ById.count(Mission.StartBeat) > 0 && CanFinish.count(Mission.StartBeat) == 0)
		{
			Issues.Add(ESeverity::Error, "mission.no_end", Ctx, "no end beat is reachable from the start beat");
		}
		for (const FBeat& Beat : Mission.Beats)
		{
			if (Reached.count(Beat.Id) > 0 && CanFinish.count(Beat.Id) == 0 && CanFinish.count(Mission.StartBeat) > 0)
			{
				Issues.Add(ESeverity::Error, "beat.dead_end", Ctx + "/" + Beat.Id, "the mission cannot finish after this beat");
			}
		}

		// Loops made only of instant beats would spin forever at runtime.
		std::map<std::string, int> Color; // 0 white, 1 grey, 2 black
		bool bReportedLoop = false;
		for (const FBeat& Root : Mission.Beats)
		{
			if (bReportedLoop || !CompletesInstantly(Root) || Color[Root.Id] != 0)
			{
				continue;
			}
			std::vector<std::pair<std::string, size_t>> Path{{Root.Id, 0}};
			Color[Root.Id] = 1;
			while (!Path.empty() && !bReportedLoop)
			{
				auto& Top = Path.back();
				const FBeat* Beat = ById.count(Top.first) > 0 ? ById[Top.first] : nullptr;
				const std::vector<std::string> Nexts = Beat ? Successors(*Beat) : std::vector<std::string>();
				if (Top.second >= Nexts.size())
				{
					Color[Top.first] = 2;
					Path.pop_back();
					continue;
				}
				const std::string Next = Nexts[Top.second++];
				const auto NextIt = ById.find(Next);
				if (NextIt == ById.end() || !CompletesInstantly(*NextIt->second))
				{
					continue;
				}
				if (Color[Next] == 1)
				{
					Issues.Add(ESeverity::Error, "mission.instant_loop", Ctx + "/" + Next, "loop of instant beats (branch/checkpoint/zero wait) with no event or wait to break it");
					bReportedLoop = true;
				}
				else if (Color[Next] == 0)
				{
					Color[Next] = 1;
					Path.emplace_back(Next, 0);
				}
			}
		}

		// Design doc §12 checklist.
		const std::pair<const char*, const std::string*> DesignFields[] = {
			{"visibleGoal", &Mission.Design.VisibleGoal},
			{"dramaticGoal", &Mission.Design.DramaticGoal},
			{"obstacle", &Mission.Design.Obstacle},
			{"decision", &Mission.Design.Decision},
			{"consequence", &Mission.Design.Consequence},
			{"transition", &Mission.Design.Transition},
		};
		for (const auto& Field : DesignFields)
		{
			if (Field.second->empty())
			{
				Issues.Add(ESeverity::Warning, "mission.design_missing", Ctx, std::string("design checklist field '") + Field.first + "' is empty");
			}
		}
	}
} // namespace GoN::Narrative
