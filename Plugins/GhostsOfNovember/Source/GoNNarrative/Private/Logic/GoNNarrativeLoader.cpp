#include "Logic/GoNNarrativeLoader.h"

#include "Logic/GoNConsequences.h"
#include "Logic/GoNJson.h"
#include "Logic/GoNMissionRunner.h"
#include "Logic/GoNTimeline.h"

#include <initializer_list>
#include <set>

namespace GoN::Narrative
{
	namespace LoaderDetail
	{
		using Json::FValue;

		bool ParseFile(const std::string& FileName, const std::string& Text, FValue& Out, FIssueList& Issues)
		{
			Json::FParseError Error;
			if (!Json::Parse(Text, Out, Error))
			{
				Issues.Add(ESeverity::Error, "json.parse", FileName, Error.ToString());
				return false;
			}
			if (!Out.IsObject())
			{
				Issues.Add(ESeverity::Error, "json.root", FileName, "root must be an object");
				return false;
			}
			return true;
		}

		// Unknown keys are almost always typos ("complete_on"); flag them.
		void CheckKeys(const FValue& Object, std::initializer_list<const char*> Allowed, const std::string& Context, FIssueList& Issues)
		{
			for (const auto& Member : Object.Members())
			{
				bool bKnown = false;
				for (const char* Key : Allowed)
				{
					bKnown = bKnown || Member.first == Key;
				}
				if (!bKnown)
				{
					Issues.Add(ESeverity::Warning, "data.unknown_field", Context, "unknown field '" + Member.first + "'");
				}
			}
		}

		std::string RequireString(const FValue& Object, const char* Key, const std::string& Context, FIssueList& Issues)
		{
			const FValue* V = Object.Find(Key);
			if (V == nullptr || !V->IsString() || V->AsString().empty())
			{
				Issues.Add(ESeverity::Error, "data.missing_field", Context, std::string("missing string field '") + Key + "'");
				return std::string();
			}
			return V->AsString();
		}

		std::vector<std::string> StringList(const FValue& Object, const char* Key, const std::string& Context, FIssueList& Issues)
		{
			std::vector<std::string> Out;
			const FValue* V = Object.Find(Key);
			if (V == nullptr || V->IsNull())
			{
				return Out;
			}
			if (!V->IsArray())
			{
				Issues.Add(ESeverity::Error, "data.type", Context, std::string("'") + Key + "' must be an array of strings");
				return Out;
			}
			for (const FValue& Item : V->Items())
			{
				if (Item.IsString())
				{
					Out.push_back(Item.AsString());
				}
				else
				{
					Issues.Add(ESeverity::Error, "data.type", Context, std::string("'") + Key + "' must contain only strings");
				}
			}
			return Out;
		}

		FLocalizedText ReadText(const FValue* V)
		{
			FLocalizedText Out;
			if (V == nullptr)
			{
				return Out;
			}
			if (V->IsString())
			{
				Out["ar"] = V->AsString();
				return Out;
			}
			for (const auto& Member : V->Members())
			{
				if (Member.second.IsString())
				{
					Out[Member.first] = Member.second.AsString();
				}
			}
			return Out;
		}

		bool ReadDate(const FValue& Object, const char* Key, FDate& Out, const std::string& Context, FIssueList& Issues)
		{
			const std::string Text = Object.GetString(Key);
			if (!FDate::Parse(Text, Out))
			{
				Issues.Add(ESeverity::Error, "data.date", Context, std::string("'") + Key + "' must be a valid YYYY-MM-DD date (got '" + Text + "')");
				return false;
			}
			return true;
		}

		bool ReadDateRange(const FValue& Object, FDateRange& Out, const std::string& Context, FIssueList& Issues)
		{
			const bool bFrom = ReadDate(Object, "from", Out.From, Context, Issues);
			const bool bTo = ReadDate(Object, "to", Out.To, Context, Issues);
			if (bFrom && bTo && Out.To < Out.From)
			{
				Issues.Add(ESeverity::Error, "data.date_order", Context, "'to' (" + Out.To.ToString() + ") is before 'from' (" + Out.From.ToString() + ")");
				return false;
			}
			return bFrom && bTo;
		}

		void ReadTimelineEntry(const FValue& Object, FTimelineEntry& Out, const std::string& Context, FIssueList& Issues)
		{
			ReadDateRange(Object, Out.Dates, Context, Issues);
			Out.PlaceId = RequireString(Object, "place", Context, Issues);
			Out.CharacterIds = StringList(Object, "characters", Context, Issues);
			Out.PovCharacterId = Object.GetString("pov");
		}

		void ReadEffects(const FValue* List, std::vector<FEffect>& Out, const std::string& Context, FIssueList& Issues)
		{
			if (List == nullptr)
			{
				return;
			}
			for (const FValue& Item : List->Items())
			{
				CheckKeys(Item, {"op", "key", "value"}, Context, Issues);
				FEffect Effect;
				if (!ParseEffectOp(Item.GetString("op"), Effect.Op))
				{
					Issues.Add(ESeverity::Error, "data.effect_op", Context, "unknown effect op '" + Item.GetString("op") + "' (add, set, setFlag, clearFlag)");
					continue;
				}
				Effect.Key = Item.GetString("key");
				Effect.Value = Item.GetNumber("value", 0.0);
				Out.push_back(Effect);
			}
		}

		void ReadConditions(const FValue* List, std::vector<FCondition>& Out, const std::string& Context, FIssueList& Issues)
		{
			if (List == nullptr)
			{
				return;
			}
			for (const FValue& Item : List->Items())
			{
				CheckKeys(Item, {"key", "op", "value"}, Context, Issues);
				FCondition Condition;
				if (!ParseCompare(Item.GetString("op"), Condition.Compare))
				{
					Issues.Add(ESeverity::Error, "data.condition_op", Context, "unknown condition op '" + Item.GetString("op") + "'");
					continue;
				}
				Condition.Key = Item.GetString("key");
				Condition.Value = Item.GetNumber("value", 0.0);
				Out.push_back(Condition);
			}
		}

		bool ParseBeatType(const std::string& Text, EBeatType& Out)
		{
			const EBeatType All[] = {EBeatType::Objective, EBeatType::Trigger, EBeatType::Dialogue, EBeatType::Cinematic, EBeatType::Scene, EBeatType::Wait, EBeatType::Branch, EBeatType::Checkpoint, EBeatType::End};
			for (const EBeatType Type : All)
			{
				if (Text == BeatTypeName(Type))
				{
					Out = Type;
					return true;
				}
			}
			return false;
		}

		bool ParseCertainty(const std::string& Text, ECertainty& Out)
		{
			if (Text == "documented") { Out = ECertainty::Documented; return true; }
			if (Text == "probable") { Out = ECertainty::Probable; return true; }
			if (Text == "disputed") { Out = ECertainty::Disputed; return true; }
			if (Text == "reconstruction") { Out = ECertainty::Reconstruction; return true; }
			return false;
		}
	} // namespace LoaderDetail

	bool LoadMission(const std::string& FileName, const std::string& Text, FMission& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		FValue Root;
		if (!ParseFile(FileName, Text, Root, Issues))
		{
			return false;
		}
		CheckKeys(Root, {"schema", "id", "chapter", "testOnly", "title", "design", "timeline", "startBeat", "beats", "lines"}, FileName, Issues);

		Out = FMission();
		Out.SourceFile = FileName;
		Out.Id = RequireString(Root, "id", FileName, Issues);
		Out.ChapterId = Root.GetString("chapter");
		Out.bTestOnly = Root.GetBool("testOnly", false);
		Out.Title = ReadText(Root.Find("title"));
		Out.StartBeat = RequireString(Root, "startBeat", FileName, Issues);
		const std::string Ctx = Out.Id.empty() ? FileName : Out.Id;

		if (const FValue* Design = Root.Find("design"))
		{
			CheckKeys(*Design, {"visibleGoal", "dramaticGoal", "obstacle", "decision", "consequence", "transition"}, Ctx + "/design", Issues);
			Out.Design.VisibleGoal = Design->GetString("visibleGoal");
			Out.Design.DramaticGoal = Design->GetString("dramaticGoal");
			Out.Design.Obstacle = Design->GetString("obstacle");
			Out.Design.Decision = Design->GetString("decision");
			Out.Design.Consequence = Design->GetString("consequence");
			Out.Design.Transition = Design->GetString("transition");
		}

		if (const FValue* Timeline = Root.Find("timeline"))
		{
			CheckKeys(*Timeline, {"from", "to", "place", "characters", "pov", "segments", "note"}, Ctx + "/timeline", Issues);
			ReadTimelineEntry(*Timeline, Out.Timeline, Ctx + "/timeline", Issues);
			if (const FValue* Segments = Timeline->Find("segments"))
			{
				for (const FValue& SegmentJson : Segments->Items())
				{
					FTimelineEntry Segment;
					Segment.Id = RequireString(SegmentJson, "id", Ctx + "/timeline/segments", Issues);
					const std::string SegmentCtx = Ctx + "/segment:" + Segment.Id;
					CheckKeys(SegmentJson, {"id", "from", "to", "place", "characters", "pov", "note"}, SegmentCtx, Issues);
					ReadTimelineEntry(SegmentJson, Segment, SegmentCtx, Issues);
					Out.Segments.push_back(Segment);
				}
			}
		}
		else
		{
			Issues.Add(ESeverity::Error, "mission.timeline_missing", Ctx, "every mission needs a timeline entry (dates, place, characters)");
		}

		if (const FValue* Lines = Root.Find("lines"))
		{
			for (const FValue& Item : Lines->Items())
			{
				FDialogueLine Line;
				Line.Id = RequireString(Item, "id", Ctx + "/lines", Issues);
				const std::string LineCtx = Ctx + "/line:" + Line.Id;
				CheckKeys(Item, {"id", "speaker", "lang", "text", "vo", "dramatized", "source", "remote"}, LineCtx, Issues);
				Line.SpeakerId = RequireString(Item, "speaker", LineCtx, Issues);
				Line.Language = Item.GetString("lang", "ar-dz");
				Line.Text = Item.GetString("text");
				Line.VoiceAsset = Item.GetString("vo");
				Line.bDramatized = Item.GetBool("dramatized", false);
				Line.SourceId = Item.GetString("source");
				Line.bRemote = Item.GetBool("remote", false);
				Out.Lines.push_back(Line);
			}
		}

		const FValue* Beats = Root.Find("beats");
		if (Beats == nullptr || !Beats->IsArray())
		{
			Issues.Add(ESeverity::Error, "mission.beats_missing", Ctx, "'beats' must be an array");
			return true;
		}
		for (const FValue& Item : Beats->Items())
		{
			FBeat Beat;
			Beat.Id = RequireString(Item, "id", Ctx + "/beats", Issues);
			const std::string BeatCtx = Ctx + "/" + Beat.Id;
			CheckKeys(Item, {"id", "type", "next", "completeOn", "seconds", "branches", "else", "onEnter", "onComplete", "lines", "asset", "text", "outcome", "segment", "note"}, BeatCtx, Issues);
			const std::string TypeText = Item.GetString("type");
			if (!ParseBeatType(TypeText, Beat.Type))
			{
				Issues.Add(ESeverity::Error, "beat.type", BeatCtx, "unknown beat type '" + TypeText + "'");
				continue;
			}
			Beat.Next = Item.GetString("next");
			Beat.CompleteOn = Item.GetString("completeOn");
			Beat.WaitSeconds = Item.GetNumber("seconds", 0.0);
			Beat.ElseNext = Item.GetString("else");
			Beat.Asset = Item.GetString("asset");
			Beat.Text = ReadText(Item.Find("text"));
			Beat.Outcome = Item.GetString("outcome");
			Beat.Segment = Item.GetString("segment");
			Beat.LineIds = StringList(Item, "lines", BeatCtx, Issues);
			ReadEffects(Item.Find("onEnter"), Beat.OnEnter, BeatCtx, Issues);
			ReadEffects(Item.Find("onComplete"), Beat.OnComplete, BeatCtx, Issues);
			if (const FValue* Branches = Item.Find("branches"))
			{
				for (const FValue& BranchJson : Branches->Items())
				{
					CheckKeys(BranchJson, {"if", "next"}, BeatCtx, Issues);
					FBranch Branch;
					ReadConditions(BranchJson.Find("if"), Branch.Conditions, BeatCtx, Issues);
					Branch.Next = BranchJson.GetString("next");
					Beat.Branches.push_back(Branch);
				}
			}
			Out.Beats.push_back(Beat);
		}
		return true;
	}

	bool LoadConsequenceSchema(const std::string& Text, FConsequenceSchema& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		const std::string File = "consequences.json";
		FValue Root;
		if (!ParseFile(File, Text, Root, Issues))
		{
			return false;
		}
		CheckKeys(Root, {"schema", "stats", "flags"}, File, Issues);
		Out = FConsequenceSchema();
		if (const FValue* Stats = Root.Find("stats"))
		{
			for (const FValue& Item : Stats->Items())
			{
				FStatDef Def;
				Def.Key = RequireString(Item, "key", File, Issues);
				CheckKeys(Item, {"key", "min", "max", "default", "note"}, File + "/" + Def.Key, Issues);
				Def.Min = Item.GetNumber("min", 0.0);
				Def.Max = Item.GetNumber("max", 100.0);
				Def.Default = Item.GetNumber("default", Def.Min);
				if (Def.Max < Def.Min || Def.Default < Def.Min || Def.Default > Def.Max)
				{
					Issues.Add(ESeverity::Error, "consequence.range", File + "/" + Def.Key, "needs min <= default <= max");
				}
				if (Def.Key.rfind(ReservedHistoryPrefix(), 0) == 0)
				{
					Issues.Add(ESeverity::Error, "consequence.reserved", File + "/" + Def.Key, "'history.' keys are reserved for documented history");
				}
				Out.Stats.push_back(Def);
			}
		}
		Out.FlagPatterns = StringList(Root, "flags", File, Issues);
		for (const std::string& Flag : Out.FlagPatterns)
		{
			if (Flag.rfind(ReservedHistoryPrefix(), 0) == 0)
			{
				Issues.Add(ESeverity::Error, "consequence.reserved", File + "/" + Flag, "'history.' keys are reserved for documented history");
			}
		}
		return true;
	}

	bool LoadCharacters(const std::string& Text, std::map<std::string, FCharacter>& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		const std::string File = "characters.json";
		FValue Root;
		if (!ParseFile(File, Text, Root, Issues))
		{
			return false;
		}
		CheckKeys(Root, {"schema", "note", "characters"}, File, Issues);
		const FValue* List = Root.Find("characters");
		if (List == nullptr)
		{
			Issues.Add(ESeverity::Error, "data.missing_field", File, "missing 'characters' array");
			return true;
		}
		for (const FValue& Item : List->Items())
		{
			FCharacter Character;
			Character.Id = RequireString(Item, "id", File, Issues);
			const std::string Ctx = File + "/" + Character.Id;
			CheckKeys(Item, {"id", "nameAr", "nameLatin", "historical", "excluded", "excludedReason", "death", "presence", "approved", "review", "role", "note"}, Ctx, Issues);
			Character.NameAr = Item.GetString("nameAr");
			Character.NameLatin = Item.GetString("nameLatin");
			Character.bHistorical = Item.GetBool("historical", false);
			Character.bExcluded = Item.GetBool("excluded", false);
			Character.ExcludedReason = Item.GetString("excludedReason");
			Character.bApproved = Item.GetBool("approved", true);
			Character.Review = Item.GetString("review");
			if (const FValue* Death = Item.Find("death"))
			{
				if (Death->IsObject())
				{
					CheckKeys(*Death, {"from", "to", "sources", "note"}, Ctx + "/death", Issues);
					Character.bHasDeath = ReadDateRange(*Death, Character.Death, Ctx + "/death", Issues);
					Character.DeathSourceIds = StringList(*Death, "sources", Ctx + "/death", Issues);
				}
			}
			if (const FValue* Presence = Item.Find("presence"))
			{
				for (const FValue& WindowJson : Presence->Items())
				{
					CheckKeys(WindowJson, {"region", "from", "to", "note", "sources"}, Ctx + "/presence", Issues);
					FPresenceWindow Window;
					Window.Region = RequireString(WindowJson, "region", Ctx + "/presence", Issues);
					if (ReadDateRange(WindowJson, Window.Dates, Ctx + "/presence", Issues))
					{
						Window.Note = WindowJson.GetString("note");
						Window.SourceIds = StringList(WindowJson, "sources", Ctx + "/presence", Issues);
						Character.Presence.push_back(Window);
					}
				}
			}
			if (!Character.Id.empty() && !Out.emplace(Character.Id, Character).second)
			{
				Issues.Add(ESeverity::Error, "character.duplicate", Ctx, "duplicate character id");
			}
		}
		return true;
	}

	bool LoadPlaces(const std::string& Text, std::map<std::string, FPlace>& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		const std::string File = "places.json";
		FValue Root;
		if (!ParseFile(File, Text, Root, Issues))
		{
			return false;
		}
		CheckKeys(Root, {"schema", "note", "regions", "places"}, File, Issues);
		if (const FValue* List = Root.Find("places"))
		{
			for (const FValue& Item : List->Items())
			{
				FPlace Place;
				Place.Id = RequireString(Item, "id", File, Issues);
				CheckKeys(Item, {"id", "region", "nameAr", "nameLatin", "fictional", "note"}, File + "/" + Place.Id, Issues);
				Place.Region = RequireString(Item, "region", File + "/" + Place.Id, Issues);
				Place.NameAr = Item.GetString("nameAr");
				Place.bFictional = Item.GetBool("fictional", false);
				if (!Place.Id.empty() && !Out.emplace(Place.Id, Place).second)
				{
					Issues.Add(ESeverity::Error, "place.duplicate", File + "/" + Place.Id, "duplicate place id");
				}
			}
		}
		return true;
	}

	bool LoadSources(const std::string& Text, std::map<std::string, FSource>& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		const std::string File = "sources.json";
		FValue Root;
		if (!ParseFile(File, Text, Root, Issues))
		{
			return false;
		}
		CheckKeys(Root, {"schema", "note", "sources"}, File, Issues);
		if (const FValue* List = Root.Find("sources"))
		{
			for (const FValue& Item : List->Items())
			{
				FSource Source;
				Source.Id = RequireString(Item, "id", File, Issues);
				CheckKeys(Item, {"id", "citation", "url", "certainty", "note"}, File + "/" + Source.Id, Issues);
				Source.Citation = RequireString(Item, "citation", File + "/" + Source.Id, Issues);
				Source.Url = Item.GetString("url");
				if (!ParseCertainty(Item.GetString("certainty", "documented"), Source.Certainty))
				{
					Issues.Add(ESeverity::Error, "source.certainty", File + "/" + Source.Id, "certainty must be documented, probable, disputed or reconstruction");
				}
				if (!Source.Id.empty() && !Out.emplace(Source.Id, Source).second)
				{
					Issues.Add(ESeverity::Error, "source.duplicate", File + "/" + Source.Id, "duplicate source id");
				}
			}
		}
		return true;
	}

	bool LoadCampaign(const FCampaignFiles& Files, FCampaign& Out, FIssueList& Issues)
	{
		using namespace LoaderDetail;
		Out = FCampaign();
		bool bOk = true;

		FValue Root;
		if (ParseFile("campaign.json", Files.Campaign, Root, Issues))
		{
			CheckKeys(Root, {"schema", "id", "note", "chapters"}, "campaign.json", Issues);
			Out.Id = Root.GetString("id");
			if (const FValue* Chapters = Root.Find("chapters"))
			{
				for (const FValue& Item : Chapters->Items())
				{
					FChapter Chapter;
					Chapter.Id = RequireString(Item, "id", "campaign.json", Issues);
					CheckKeys(Item, {"id", "order", "title", "period", "missions", "note"}, "campaign.json/" + Chapter.Id, Issues);
					Chapter.Order = static_cast<int>(Item.GetNumber("order", 0.0));
					Chapter.Title = ReadText(Item.Find("title"));
					Chapter.MissionIds = StringList(Item, "missions", "campaign.json/" + Chapter.Id, Issues);
					Out.Chapters.push_back(Chapter);
				}
			}
		}
		else
		{
			bOk = false;
		}

		bOk = LoadCharacters(Files.Characters, Out.Characters, Issues) && bOk;
		bOk = LoadPlaces(Files.Places, Out.Places, Issues) && bOk;
		bOk = LoadSources(Files.Sources, Out.Sources, Issues) && bOk;
		bOk = LoadConsequenceSchema(Files.Consequences, Out.Schema, Issues) && bOk;

		std::set<std::string> MissionIds;
		for (const auto& File : Files.Missions)
		{
			FMission Mission;
			if (!LoadMission(File.first, File.second, Mission, Issues))
			{
				bOk = false;
				continue;
			}
			if (!MissionIds.insert(Mission.Id).second)
			{
				Issues.Add(ESeverity::Error, "mission.duplicate", File.first, "mission id '" + Mission.Id + "' is already used");
				continue;
			}
			Out.Missions.push_back(std::move(Mission));
		}
		return bOk;
	}

	bool ReadCampaignFiles(const IFileSystem& FileSystem, const std::string& NarrativeDirectory, FCampaignFiles& Out, FIssueList& Issues)
	{
		Out = FCampaignFiles();
		bool bOk = true;
		const std::pair<const char*, std::string*> Required[] = {
			{"campaign.json", &Out.Campaign},
			{"characters.json", &Out.Characters},
			{"places.json", &Out.Places},
			{"sources.json", &Out.Sources},
			{"consequences.json", &Out.Consequences},
		};
		for (const auto& File : Required)
		{
			const std::string Path = JoinPath(NarrativeDirectory, File.first);
			if (!FileSystem.ReadTextFile(Path, *File.second))
			{
				Issues.Add(ESeverity::Error, "data.file_missing", Path, "cannot read file");
				bOk = false;
			}
		}
		const std::string MissionDirectory = JoinPath(NarrativeDirectory, "missions");
		for (const std::string& Name : FileSystem.ListFiles(MissionDirectory, ".json"))
		{
			std::string Text;
			if (!FileSystem.ReadTextFile(JoinPath(MissionDirectory, Name), Text))
			{
				Issues.Add(ESeverity::Error, "data.file_missing", Name, "cannot read mission file");
				bOk = false;
				continue;
			}
			Out.Missions.emplace_back("missions/" + Name, std::move(Text));
		}
		if (Out.Missions.empty())
		{
			Issues.Add(ESeverity::Warning, "data.no_missions", MissionDirectory, "no mission files found");
		}
		return bOk;
	}

	bool LoadAndValidateCampaign(const IFileSystem& FileSystem, const std::string& NarrativeDirectory, FCampaign& Out, FIssueList& Issues)
	{
		FCampaignFiles Files;
		const bool bRead = ReadCampaignFiles(FileSystem, NarrativeDirectory, Files, Issues);
		const bool bLoaded = LoadCampaign(Files, Out, Issues);
		if (bRead && bLoaded)
		{
			ValidateCampaign(Out, Issues);
		}
		return bRead && bLoaded && !Issues.HasErrors();
	}

	void ValidateCampaign(const FCampaign& Campaign, FIssueList& Issues)
	{
		std::set<std::string> Listed;
		for (const FChapter& Chapter : Campaign.Chapters)
		{
			for (const std::string& MissionId : Chapter.MissionIds)
			{
				const FMission* Mission = Campaign.FindMission(MissionId);
				if (Mission == nullptr)
				{
					Issues.Add(ESeverity::Error, "campaign.mission_unknown", "campaign.json/" + Chapter.Id, "mission '" + MissionId + "' has no mission file");
					continue;
				}
				if (!Listed.insert(MissionId).second)
				{
					Issues.Add(ESeverity::Error, "campaign.mission_twice", "campaign.json/" + Chapter.Id, "mission '" + MissionId + "' is listed twice");
				}
				if (Mission->ChapterId != Chapter.Id)
				{
					Issues.Add(ESeverity::Error, "campaign.chapter_mismatch", MissionId, "mission says chapter '" + Mission->ChapterId + "' but is listed under '" + Chapter.Id + "'");
				}
			}
		}
		for (const FMission& Mission : Campaign.Missions)
		{
			if (!Mission.bTestOnly && Listed.count(Mission.Id) == 0)
			{
				Issues.Add(ESeverity::Warning, "campaign.mission_unlisted", Mission.Id, "mission file is not listed in any chapter");
			}
			ValidateMissionGraph(Mission, Campaign.Schema, Issues);
		}
		ValidateTimeline(Campaign, Issues);
	}
} // namespace GoN::Narrative
