// Pure-logic tests for the narrative core. Run by Tools/LogicTests and "GoN.Narrative.Logic".
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNCampaignState.h"
#include "Logic/GoNConsequences.h"
#include "Logic/GoNMissionRunner.h"
#include "Logic/GoNNarrativeLoader.h"
#include "Logic/GoNTimeline.h"

namespace GoNNarrativeLogicTestsDetail
{
	using namespace GoN::Narrative;

	const char* SchemaJson = R"({
		"stats": [
			{ "key": "trust.*", "min": -100, "max": 100, "default": 0 },
			{ "key": "suspicion.aures", "min": 0, "max": 100, "default": 0 },
			{ "key": "network.health", "min": 0, "max": 100, "default": 60 }
		],
		"flags": ["info.*", "choice.*"]
	})";

	const char* CharactersJson = R"({
		"characters": [
			{ "id": "ben_boulaid", "nameLatin": "Mostefa Ben Boulaid", "historical": true,
			  "death": { "from": "1956-03-22", "to": "1956-03-23" },
			  "presence": [ { "region": "aures", "from": "1954-10-01", "to": "1955-01-31" } ] },
			{ "id": "ben_mhidi", "nameLatin": "Larbi Ben M'Hidi", "historical": true,
			  "presence": [ { "region": "oranie", "from": "1954-10-01", "to": "1954-12-31" } ] },
			{ "id": "didouche", "nameLatin": "Didouche Mourad", "historical": true,
			  "death": { "from": "1955-01-18", "to": "1955-01-18" },
			  "presence": [ { "region": "north_constantine", "from": "1954-10-01", "to": "1955-01-18" } ] },
			{ "id": "nsoumer", "nameLatin": "Lalla Fatma N'Soumer", "historical": true, "excluded": true, "excludedReason": "19th century" },
			{ "id": "fighter", "nameLatin": "Fictional fighter", "historical": false },
			{ "id": "narrator", "historical": false }
		]
	})";

	const char* PlacesJson = R"({
		"places": [
			{ "id": "town", "region": "aures", "fictional": true },
			{ "id": "mountains", "region": "aures" },
			{ "id": "skikda_hills", "region": "north_constantine" },
			{ "id": "oran", "region": "oranie" }
		]
	})";

	const char* SourcesJson = R"({ "sources": [ { "id": "src_a", "citation": "A source", "certainty": "documented" } ] })";

	// Every beat type: objective -> dialogue -> checkpoint -> wait -> trigger -> branch -> end.
	const char* RunnerMissionJson = R"({
		"id": "runner_test", "chapter": "test", "testOnly": true,
		"design": { "visibleGoal": "v", "dramaticGoal": "d", "obstacle": "o", "decision": "c", "consequence": "q", "transition": "t" },
		"timeline": { "from": "1954-10-30", "to": "1954-10-30", "place": "town", "characters": ["fighter"] },
		"startBeat": "goal",
		"lines": [ { "id": "l1", "speaker": "fighter", "text": "x" } ],
		"beats": [
			{ "id": "goal", "type": "objective", "completeOn": "reached", "onComplete": [ { "op": "add", "key": "trust.town", "value": 15 } ], "next": "talk" },
			{ "id": "talk", "type": "dialogue", "lines": ["l1"], "next": "cp" },
			{ "id": "cp", "type": "checkpoint", "onEnter": [ { "op": "setFlag", "key": "info.cp_seen" } ], "next": "rest" },
			{ "id": "rest", "type": "wait", "seconds": 1.5, "next": "listen" },
			{ "id": "listen", "type": "trigger", "next": "decide" },
			{ "id": "decide", "type": "branch", "branches": [ { "if": [ { "key": "trust.town", "op": ">=", "value": 10 } ], "next": "won" } ], "else": "lost" },
			{ "id": "won", "type": "end", "outcome": "trusted" },
			{ "id": "lost", "type": "end", "outcome": "alone" }
		]
	})";

	struct FRecorder : IMissionListener
	{
		std::vector<std::string> Entered;
		std::vector<std::string> Checkpoints;
		std::vector<std::string> Messages;
		std::string Outcome;

		void OnBeatEntered(const FMission& Mission, const FBeat& Beat) override { (void)Mission; Entered.push_back(Beat.Id); }
		void OnCheckpoint(const FMission& Mission, const FBeat& Beat) override { (void)Mission; Checkpoints.push_back(Beat.Id); }
		void OnMissionEnded(const FMission& Mission, const std::string& InOutcome) override { (void)Mission; Outcome = InOutcome; }
		void OnRunnerMessage(const FMission& Mission, ESeverity Severity, const std::string& Message) override { (void)Mission; (void)Severity; Messages.push_back(Message); }
	};

	FCampaign MakeCampaign(const std::vector<std::string>& MissionJsons, FIssueList& Issues)
	{
		FCampaign Campaign;
		LoadConsequenceSchema(SchemaJson, Campaign.Schema, Issues);
		LoadCharacters(CharactersJson, Campaign.Characters, Issues);
		LoadPlaces(PlacesJson, Campaign.Places, Issues);
		LoadSources(SourcesJson, Campaign.Sources, Issues);
		int Index = 0;
		for (const std::string& MissionJson : MissionJsons)
		{
			FMission Mission;
			LoadMission("fixture" + std::to_string(Index++) + ".json", MissionJson, Mission, Issues);
			Campaign.Missions.push_back(Mission);
		}
		return Campaign;
	}

	// Minimal campaign mission with a given place, dates and characters.
	std::string TimelineMission(const std::string& Id, const std::string& Place, const std::string& From, const std::string& To, const std::string& CharactersCsvJson, const std::string& LinesJson = "[]")
	{
		return R"({ "id": ")" + Id + R"(", "chapter": "c",
			"design": { "visibleGoal": "v", "dramaticGoal": "d", "obstacle": "o", "decision": "c", "consequence": "q", "transition": "t" },
			"timeline": { "from": ")" + From + R"(", "to": ")" + To + R"(", "place": ")" + Place + R"(", "characters": [)" + CharactersCsvJson + R"(] },
			"startBeat": "e", "lines": )" + LinesJson + R"(, "beats": [ { "id": "e", "type": "end" } ] })";
	}

// Tests stay inside the namespace so the using-directive above cannot leak into other
// files of a unity build.

GON_LOGIC_TEST("Narrative.Consequences.ClampsAndDefaults", Narrative_Consequences_Clamp)
{
	FIssueList Issues;
	FConsequenceSchema Schema;
	GON_REQUIRE(LoadConsequenceSchema(SchemaJson, Schema, Issues));
	FConsequenceState State(&Schema);
	GON_EXPECT_EQ(State.GetStat("network.health"), 60.0);
	GON_EXPECT(State.Apply(FEffect{EEffectOp::Add, "trust.douar", 150.0}));
	GON_EXPECT_EQ(State.GetStat("trust.douar"), 100.0);
	GON_EXPECT(State.Apply(FEffect{EEffectOp::Add, "trust.douar", -500.0}));
	GON_EXPECT_EQ(State.GetStat("trust.douar"), -100.0);
	GON_EXPECT(State.Apply(FEffect{EEffectOp::Set, "suspicion.aures", 42.0}));
	GON_EXPECT_EQ(State.GetStat("suspicion.aures"), 42.0);
	GON_EXPECT(State.Apply(FEffect{EEffectOp::SetFlag, "info.patrol", 0.0}));
	GON_EXPECT(State.HasFlag("info.patrol"));
	GON_EXPECT(State.Apply(FEffect{EEffectOp::ClearFlag, "info.patrol", 0.0}));
	GON_EXPECT(!State.HasFlag("info.patrol"));
}

GON_LOGIC_TEST("Narrative.Consequences.RefusesHistoryAndUnknownKeys", Narrative_Consequences_Refuse)
{
	FIssueList Issues;
	FConsequenceSchema Schema;
	LoadConsequenceSchema(SchemaJson, Schema, Issues);
	FConsequenceState State(&Schema);
	std::string Error;
	GON_EXPECT(!State.Apply(FEffect{EEffectOp::SetFlag, "history.ben_boulaid_survives", 0.0}, &Error));
	GON_EXPECT(Error.find("documented history") != std::string::npos);
	GON_EXPECT(!State.Apply(FEffect{EEffectOp::Add, "trsut.douar", 1.0}, &Error)); // typo
	GON_EXPECT(!State.Apply(FEffect{EEffectOp::SetFlag, "secret.thing", 0.0}, &Error));
	GON_EXPECT(!State.Apply(FEffect{EEffectOp::Add, "trust.", 1.0}, &Error)); // bare pattern prefix

	FConsequenceSchema Reserved;
	FIssueList ReservedIssues;
	LoadConsequenceSchema(R"({ "stats": [ { "key": "history.year", "min": 0, "max": 3000 } ] })", Reserved, ReservedIssues);
	GON_EXPECT(ReservedIssues.HasCode("consequence.reserved"));
}

GON_LOGIC_TEST("Narrative.Consequences.Conditions", Narrative_Consequences_Conditions)
{
	FIssueList Issues;
	FConsequenceSchema Schema;
	LoadConsequenceSchema(SchemaJson, Schema, Issues);
	FConsequenceState State(&Schema);
	State.Apply(FEffect{EEffectOp::Set, "trust.x", 10.0});
	GON_EXPECT(State.Evaluate(FCondition{"trust.x", ECompare::GreaterEqual, 10.0}));
	GON_EXPECT(State.Evaluate(FCondition{"trust.x", ECompare::Equal, 10.0}));
	GON_EXPECT(!State.Evaluate(FCondition{"trust.x", ECompare::Greater, 10.0}));
	GON_EXPECT(State.Evaluate(FCondition{"trust.y", ECompare::Equal, 0.0})); // default
	GON_EXPECT(State.Evaluate(FCondition{"info.none", ECompare::FlagUnset, 0.0}));
	GON_EXPECT(!State.EvaluateAll({FCondition{"trust.x", ECompare::Less, 20.0}, FCondition{"info.none", ECompare::FlagSet, 0.0}}));
}

GON_LOGIC_TEST("Narrative.Runner.RunsStartToEnd", Narrative_Runner_StartToEnd)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({RunnerMissionJson}, Issues);
	GON_REQUIRE(!Issues.HasErrors());
	const FMission& Mission = Campaign.Missions[0];
	FConsequenceState State(&Campaign.Schema);
	FRecorder Recorder;
	FMissionRunner Runner(Mission, State, &Recorder);

	GON_REQUIRE(Runner.Start());
	GON_EXPECT_EQ(Runner.GetCurrentBeat()->Id, std::string("goal"));
	GON_EXPECT(!Runner.HandleEvent("wrong.event"));
	GON_EXPECT(Runner.HandleEvent("reached"));
	GON_EXPECT_EQ(Runner.GetCurrentBeat()->Id, std::string("talk"));
	GON_EXPECT(Runner.HandleEvent("talk.done")); // default completion event
	// Checkpoint is instant; the runner now waits in "rest".
	GON_EXPECT_EQ(Runner.GetCurrentBeat()->Id, std::string("rest"));
	GON_EXPECT_EQ(Recorder.Checkpoints.size(), size_t(1));
	GON_EXPECT(State.HasFlag("info.cp_seen"));
	Runner.Tick(1.0);
	GON_EXPECT_EQ(Runner.GetCurrentBeat()->Id, std::string("rest"));
	Runner.Tick(0.6);
	GON_EXPECT_EQ(Runner.GetCurrentBeat()->Id, std::string("listen"));
	GON_EXPECT(Runner.SkipCurrentBeat());
	GON_EXPECT(Runner.GetState() == ERunState::Ended);
	GON_EXPECT_EQ(Runner.GetOutcome(), std::string("trusted"));
	GON_EXPECT_EQ(Recorder.Outcome, std::string("trusted"));
	GON_EXPECT(Recorder.Messages.empty());
	GON_EXPECT(!Runner.HandleEvent("reached")); // ended runners ignore events
}

GON_LOGIC_TEST("Narrative.Runner.BranchElse", Narrative_Runner_BranchElse)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({RunnerMissionJson}, Issues);
	FConsequenceState State(&Campaign.Schema);
	State.Apply(FEffect{EEffectOp::Set, "trust.town", -50.0});
	FMissionRunner Runner(Campaign.Missions[0], State);
	GON_REQUIRE(Runner.Start());
	Runner.HandleEvent("reached"); // +15 -> -35, below the branch threshold
	while (Runner.GetState() == ERunState::Running)
	{
		GON_REQUIRE(Runner.SkipCurrentBeat());
	}
	GON_EXPECT_EQ(Runner.GetOutcome(), std::string("alone"));
}

GON_LOGIC_TEST("Narrative.Runner.ResumesFromCheckpointSave", Narrative_Runner_Resume)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({RunnerMissionJson}, Issues);
	const FMission& Mission = Campaign.Missions[0];

	// Play to the checkpoint and save there, as the Unreal subsystem does in OnCheckpoint.
	FCampaignState Saved(&Campaign.Schema);
	FMissionRunner First(Mission, Saved.Consequences);
	First.Start();
	First.HandleEvent("reached");
	First.HandleEvent("talk.done");
	GON_REQUIRE(First.GetLastCheckpoint() == "cp");
	Saved.Progress.MissionId = Mission.Id;
	Saved.Progress.CheckpointBeatId = First.GetLastCheckpoint();
	const std::string Payload = Saved.Serialize();

	FCampaignState Loaded(&Campaign.Schema);
	std::string Error;
	GON_REQUIRE(Loaded.Deserialize(Payload, &Error));
	GON_EXPECT_EQ(Loaded.Consequences.GetStat("trust.town"), 15.0);
	GON_EXPECT(Loaded.Consequences.HasFlag("info.cp_seen"));

	FRecorder Recorder;
	FMissionRunner Resumed(Mission, Loaded.Consequences, &Recorder);
	GON_REQUIRE(Resumed.Start(Loaded.Progress.CheckpointBeatId));
	GON_EXPECT_EQ(Resumed.GetCurrentBeat()->Id, std::string("rest"));
	GON_EXPECT(Recorder.Checkpoints.empty()); // no second save on resume
	GON_EXPECT_EQ(Loaded.Consequences.GetStat("trust.town"), 15.0); // effects not re-applied

	FMissionRunner Bad(Mission, Loaded.Consequences);
	GON_EXPECT(!Bad.Start("talk")); // only checkpoints can be resumed
}

GON_LOGIC_TEST("Narrative.CampaignState.RoundTripAndBadInput", Narrative_CampaignState_RoundTrip)
{
	FIssueList Issues;
	FConsequenceSchema Schema;
	LoadConsequenceSchema(SchemaJson, Schema, Issues);
	FCampaignState State(&Schema);
	State.Consequences.Apply(FEffect{EEffectOp::Add, "suspicion.aures", 33.0});
	State.Consequences.Apply(FEffect{EEffectOp::SetFlag, "choice.route_ridge", 0.0});
	State.RecordMissionEnd("m1", "network_holds");

	FCampaignState Back(&Schema);
	std::string Error;
	GON_REQUIRE(Back.Deserialize(State.Serialize(), &Error));
	GON_EXPECT_EQ(Back.Consequences.GetStat("suspicion.aures"), 33.0);
	GON_EXPECT(Back.Consequences.HasFlag("choice.route_ridge"));
	GON_EXPECT(Back.IsMissionCompleted("m1"));
	GON_EXPECT_EQ(Back.Progress.Outcomes["m1"], std::string("network_holds"));

	GON_EXPECT(!Back.Deserialize("not json", &Error));
	GON_EXPECT(!Back.Deserialize("{\"schema\": 99}", &Error));
	GON_EXPECT(Back.IsMissionCompleted("m1")); // failed loads leave state untouched

	std::vector<std::string> Warnings;
	GON_EXPECT(Back.Deserialize(R"({"schema":1,"consequences":{"stats":{"removed.stat":5},"flags":["gone.flag"]}})", &Error, &Warnings));
	GON_EXPECT_EQ(Warnings.size(), size_t(2));
}

GON_LOGIC_TEST("Narrative.Graph.ValidMissionHasNoErrors", Narrative_Graph_Valid)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({RunnerMissionJson}, Issues);
	ValidateMissionGraph(Campaign.Missions[0], Campaign.Schema, Issues);
	GON_EXPECT_EQ(Issues.ErrorCount(), 0);
	GON_EXPECT_EQ(Issues.WarningCount(), 0);
}

GON_LOGIC_TEST("Narrative.Graph.DetectsBrokenGraphs", Narrative_Graph_Broken)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({R"({
		"id": "broken", "chapter": "c", "testOnly": true,
		"timeline": { "from": "1954-10-30", "to": "1954-10-30", "place": "town", "characters": [] },
		"startBeat": "a",
		"beats": [
			{ "id": "a", "type": "objective", "next": "b", "onComplete": [ { "op": "add", "key": "trsut.x", "value": 1 } ] },
			{ "id": "b", "type": "branch", "branches": [ { "if": [ { "key": "nope.x", "op": ">", "value": 1 } ], "next": "c" } ], "else": "missing" },
			{ "id": "c", "type": "checkpoint", "next": "d" },
			{ "id": "d", "type": "branch", "branches": [], "else": "c" },
			{ "id": "orphan", "type": "end" },
			{ "id": "talk", "type": "dialogue", "lines": ["ghost"], "next": "orphan" },
			{ "id": "c", "type": "wait", "seconds": 1, "next": "d" },
			{ "id": "w", "type": "bogus" }
		]
	})"}, Issues);
	ValidateMissionGraph(Campaign.Missions[0], Campaign.Schema, Issues);
	GON_EXPECT(Issues.HasCode("beat.duplicate"));
	GON_EXPECT(Issues.HasCode("beat.type"));
	GON_EXPECT(Issues.HasCode("beat.next_unknown"));
	GON_EXPECT(Issues.HasCode("mission.effect"));
	GON_EXPECT(Issues.HasCode("beat.condition"));
	GON_EXPECT(Issues.HasCode("mission.instant_loop"));
	GON_EXPECT(Issues.HasCode("mission.no_end"));
	GON_EXPECT(Issues.HasCode("beat.unreachable"));
	GON_EXPECT(Issues.HasCode("beat.line_unknown"));
	GON_EXPECT(Issues.HasCode("mission.design_missing"));
}

GON_LOGIC_TEST("Narrative.Runner.InstantLoopFailsSafely", Narrative_Runner_InstantLoop)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({R"({
		"id": "loop", "chapter": "c", "testOnly": true,
		"timeline": { "from": "1954-10-30", "to": "1954-10-30", "place": "town", "characters": [] },
		"startBeat": "a",
		"beats": [
			{ "id": "a", "type": "checkpoint", "next": "b" },
			{ "id": "b", "type": "branch", "else": "a" }
		]
	})"}, Issues);
	FConsequenceState State(&Campaign.Schema);
	FRecorder Recorder;
	FMissionRunner Runner(Campaign.Missions[0], State, &Recorder);
	GON_EXPECT(!Runner.Start());
	GON_EXPECT(Runner.GetState() == ERunState::Failed);
	GON_EXPECT(!Recorder.Messages.empty());
}

GON_LOGIC_TEST("Narrative.Loader.FlagsTyposAndBadDates", Narrative_Loader_Typos)
{
	FIssueList Issues;
	FMission Mission;
	LoadMission("typo.json", R"({
		"id": "typo", "chapter": "c", "startBeat": "a",
		"timeline": { "from": "1954-11-31", "to": "1954-11-01", "place": "town" },
		"beats": [ { "id": "a", "type": "objective", "complete_on": "x", "next": "b" }, { "id": "b", "type": "end" } ]
	})", Mission, Issues);
	GON_EXPECT(Issues.HasCode("data.unknown_field"));
	GON_EXPECT(Issues.HasCode("data.date"));

	FIssueList ParseIssues;
	GON_EXPECT(!LoadMission("bad.json", "{ \"id\": ", Mission, ParseIssues));
	GON_EXPECT(ParseIssues.HasCode("json.parse"));
}

GON_LOGIC_TEST("Narrative.Timeline.PrototypeSquadIsRejected", Narrative_Timeline_PrototypeSquad)
{
	// The old prototype put Ben M'Hidi and Didouche in Ben Boulaid's Aures squad on 1 Nov 1954.
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({TimelineMission("proto", "town", "1954-10-31", "1954-11-01", R"("ben_boulaid","ben_mhidi","didouche")")}, Issues);
	ValidateTimeline(Campaign, Issues);
	int RegionErrors = 0;
	for (const FIssue& Issue : Issues.Items)
	{
		RegionErrors += Issue.Code == "history.region" ? 1 : 0;
	}
	GON_EXPECT_EQ(RegionErrors, 2);

	FIssueList Fixed;
	FCampaign FixedCampaign = MakeCampaign({TimelineMission("fixed", "town", "1954-10-31", "1954-11-01", R"("ben_boulaid","fighter")")}, Fixed);
	ValidateTimeline(FixedCampaign, Fixed);
	GON_EXPECT_EQ(Fixed.ErrorCount(), 0);
}

GON_LOGIC_TEST("Narrative.Timeline.TwoPlacesAtOnce", Narrative_Timeline_TwoPlaces)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({
		TimelineMission("m_aures", "town", "1954-11-01", "1954-11-03", R"("fighter")"),
		TimelineMission("m_north", "skikda_hills", "1954-11-03", "1954-11-05", R"("fighter")"),
		TimelineMission("m_aures_2", "mountains", "1954-11-02", "1954-11-02", R"("fighter")"),
	}, Issues);
	ValidateTimeline(Campaign, Issues);
	int TwoPlaces = 0;
	for (const FIssue& Issue : Issues.Items)
	{
		TwoPlaces += Issue.Code == "timeline.two_places" ? 1 : 0;
	}
	GON_EXPECT_EQ(TwoPlaces, 1); // same region (town/mountains) is allowed
}

GON_LOGIC_TEST("Narrative.Timeline.DeathDates", Narrative_Timeline_Death)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({
		TimelineMission("after_didouche", "skikda_hills", "1955-01-10", "1955-01-20", R"("didouche")"),
		TimelineMission("disputed", "mountains", "1956-03-20", "1956-03-23", R"("ben_boulaid")"),
	}, Issues);
	ValidateTimeline(Campaign, Issues);
	GON_EXPECT(Issues.HasCode("timeline.after_death"));
	GON_EXPECT(Issues.HasCode("timeline.death_disputed"));
	// Ben Boulaid has no documented 1956 window in the fixture: the region rule fires too.
	GON_EXPECT(Issues.HasCode("history.region"));
}

GON_LOGIC_TEST("Narrative.Timeline.DialogueRules", Narrative_Timeline_Dialogue)
{
	FIssueList Issues;
	const std::string Lines = R"([
		{ "id": "unsourced", "speaker": "ben_boulaid", "text": "x" },
		{ "id": "dramatized", "speaker": "ben_boulaid", "text": "x", "dramatized": true },
		{ "id": "sourced", "speaker": "ben_boulaid", "text": "x", "source": "src_a" },
		{ "id": "badsource", "speaker": "ben_boulaid", "text": "x", "source": "src_missing" },
		{ "id": "absent", "speaker": "didouche", "text": "x", "dramatized": true },
		{ "id": "radio", "speaker": "narrator", "text": "x", "remote": true },
		{ "id": "excluded", "speaker": "nsoumer", "text": "x", "dramatized": true, "remote": true }
	])";
	FCampaign Campaign = MakeCampaign({TimelineMission("talk", "town", "1954-10-31", "1954-11-01", R"("ben_boulaid")", Lines)}, Issues);
	ValidateTimeline(Campaign, Issues);
	std::map<std::string, std::vector<std::string>> CodesByLine;
	for (const FIssue& Issue : Issues.Items)
	{
		CodesByLine[Issue.Context].push_back(Issue.Code);
	}
	GON_EXPECT_EQ(CodesByLine["talk/line:unsourced"].size(), size_t(1));
	GON_EXPECT(CodesByLine["talk/line:dramatized"].empty());
	GON_EXPECT(CodesByLine["talk/line:sourced"].empty());
	GON_EXPECT(CodesByLine["talk/line:badsource"].size() == 1 && CodesByLine["talk/line:badsource"][0] == "source.unknown");
	GON_EXPECT(CodesByLine["talk/line:absent"].size() == 1 && CodesByLine["talk/line:absent"][0] == "dialogue.speaker_absent");
	GON_EXPECT(CodesByLine["talk/line:radio"].empty());
	GON_EXPECT(CodesByLine["talk/line:excluded"].size() == 1 && CodesByLine["talk/line:excluded"][0] == "history.excluded");
}

GON_LOGIC_TEST("Narrative.Timeline.ExcludedCharacterInMission", Narrative_Timeline_Excluded)
{
	FIssueList Issues;
	FCampaign Campaign = MakeCampaign({TimelineMission("m", "mountains", "1954-11-01", "1954-11-01", R"("nsoumer")")}, Issues);
	ValidateTimeline(Campaign, Issues);
	GON_EXPECT(Issues.HasCode("history.excluded"));
}

// ---- Shipped data (Plugins/GhostsOfNovember/Data/Narrative) ----------------------------

GON_LOGIC_TEST("Narrative.Data.ShippedCampaignValidates", Narrative_Data_Validates)
{
	const GoN::LogicTest::FDataAccess& Access = GoN::LogicTest::DataAccess();
	GON_REQUIRE(Access.FileSystem != nullptr);
	FCampaign Campaign;
	FIssueList Issues;
	const bool bOk = LoadAndValidateCampaign(*Access.FileSystem, GoN::JoinPath(Access.DataDirectory, "Narrative"), Campaign, Issues);
	for (const FIssue& Issue : Issues.Items)
	{
		if (Issue.Severity == ESeverity::Error)
		{
			Ctx.Fail(__FILE__, __LINE__, Issue.ToString());
		}
	}
	GON_EXPECT(bOk);
	GON_EXPECT(Campaign.FindMission("gym_test_mission") != nullptr);
	GON_EXPECT(Campaign.FindMission("ch1_mb_first_november") != nullptr);
	// The prototype's Ben Boulaid briefing lines are kept, tagged dramatized (brief §3).
	const FMission* Briefing = Campaign.FindMission("ch1_mb_first_november");
	GON_REQUIRE(Briefing != nullptr);
	int BenBoulaidLines = 0;
	for (const FDialogueLine& Line : Briefing->Lines)
	{
		if (Line.SpeakerId == "ben_boulaid")
		{
			++BenBoulaidLines;
			GON_EXPECT(Line.bDramatized);
		}
	}
	GON_EXPECT_EQ(BenBoulaidLines, 10);
}

GON_LOGIC_TEST("Narrative.Data.GymMissionRunsStartToEnd", Narrative_Data_GymMission)
{
	const GoN::LogicTest::FDataAccess& Access = GoN::LogicTest::DataAccess();
	GON_REQUIRE(Access.FileSystem != nullptr);
	FCampaign Campaign;
	FIssueList Issues;
	LoadAndValidateCampaign(*Access.FileSystem, GoN::JoinPath(Access.DataDirectory, "Narrative"), Campaign, Issues);
	const FMission* Mission = Campaign.FindMission("gym_test_mission");
	GON_REQUIRE(Mission != nullptr);

	FCampaignState State(&Campaign.Schema);
	FRecorder Recorder;
	FMissionRunner Runner(*Mission, State.Consequences, &Recorder);
	GON_REQUIRE(Runner.Start());
	GON_EXPECT(Runner.HandleEvent("gym.reached_cover"));
	GON_EXPECT(Runner.HandleEvent("radio.done"));
	GON_EXPECT_EQ(Runner.GetLastCheckpoint(), std::string("cp_after_radio"));
	Runner.Tick(2.5);
	GON_EXPECT(Runner.HandleEvent("gym.heard_patrol"));
	GON_EXPECT(Runner.GetState() == ERunState::Ended);
	GON_EXPECT_EQ(Runner.GetOutcome(), std::string("success"));
	GON_EXPECT(State.Consequences.HasFlag("gym.patrol_route_known"));
	GON_EXPECT_EQ(State.Consequences.GetStat("group.fatigue"), 10.0);
	GON_EXPECT(Recorder.Messages.empty());
}

GON_LOGIC_TEST("Narrative.Data.EveryMissionCanBeSkippedToEnd", Narrative_Data_AllMissionsFinish)
{
	const GoN::LogicTest::FDataAccess& Access = GoN::LogicTest::DataAccess();
	GON_REQUIRE(Access.FileSystem != nullptr);
	FCampaign Campaign;
	FIssueList Issues;
	LoadAndValidateCampaign(*Access.FileSystem, GoN::JoinPath(Access.DataDirectory, "Narrative"), Campaign, Issues);
	GON_REQUIRE(!Campaign.Missions.empty());
	for (const FMission& Mission : Campaign.Missions)
	{
		FConsequenceState State(&Campaign.Schema);
		FRecorder Recorder;
		FMissionRunner Runner(Mission, State, &Recorder);
		Runner.Start();
		for (int Step = 0; Step < 1000 && Runner.GetState() == ERunState::Running; ++Step)
		{
			Runner.SkipCurrentBeat();
		}
		if (Runner.GetState() != ERunState::Ended)
		{
			Ctx.Fail(__FILE__, __LINE__, "mission '" + Mission.Id + "' did not reach an end beat (state " + RunStateName(Runner.GetState()) + ")");
		}
		for (const std::string& Message : Recorder.Messages)
		{
			Ctx.Fail(__FILE__, __LINE__, Mission.Id + ": " + Message);
		}
	}
}

} // namespace GoNNarrativeLogicTestsDetail

#endif // GON_WITH_LOGIC_TESTS
