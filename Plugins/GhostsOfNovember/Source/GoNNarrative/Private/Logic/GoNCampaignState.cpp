#include "Logic/GoNCampaignState.h"

#include "Logic/GoNJson.h"

#include <algorithm>

namespace GoN::Narrative
{
	void FCampaignState::Reset()
	{
		Consequences.Reset();
		Progress = FCampaignProgress();
	}

	void FCampaignState::RecordMissionEnd(const std::string& MissionId, const std::string& Outcome)
	{
		if (!IsMissionCompleted(MissionId))
		{
			Progress.CompletedMissions.push_back(MissionId);
		}
		Progress.Outcomes[MissionId] = Outcome;
		if (Progress.MissionId == MissionId)
		{
			Progress.MissionId.clear();
			Progress.CheckpointBeatId.clear();
		}
	}

	bool FCampaignState::IsMissionCompleted(const std::string& MissionId) const
	{
		return std::find(Progress.CompletedMissions.begin(), Progress.CompletedMissions.end(), MissionId) != Progress.CompletedMissions.end();
	}

	std::string FCampaignState::Serialize() const
	{
		using Json::FValue;
		FValue Root = FValue::MakeObject();
		Root.Set("schema", FValue::MakeNumber(SchemaVersion));
		FValue ProgressJson = FValue::MakeObject();
		ProgressJson.Set("mission", FValue::MakeString(Progress.MissionId));
		ProgressJson.Set("checkpoint", FValue::MakeString(Progress.CheckpointBeatId));
		FValue Completed = FValue::MakeArray();
		for (const std::string& MissionId : Progress.CompletedMissions)
		{
			Completed.Push(FValue::MakeString(MissionId));
		}
		ProgressJson.Set("completed", std::move(Completed));
		FValue Outcomes = FValue::MakeObject();
		for (const auto& Pair : Progress.Outcomes)
		{
			Outcomes.Set(Pair.first, FValue::MakeString(Pair.second));
		}
		ProgressJson.Set("outcomes", std::move(Outcomes));
		Root.Set("progress", std::move(ProgressJson));
		Root.Set("consequences", Consequences.ToJson());
		return Json::Write(Root, 0);
	}

	bool FCampaignState::Deserialize(const std::string& Text, std::string* OutError, std::vector<std::string>* OutWarnings)
	{
		Json::FValue Root;
		Json::FParseError ParseError;
		if (!Json::Parse(Text, Root, ParseError))
		{
			if (OutError) { *OutError = "save payload is not valid JSON: " + ParseError.ToString(); }
			return false;
		}
		const int Version = static_cast<int>(Root.GetNumber("schema", 0.0));
		if (Version < 1 || Version > SchemaVersion)
		{
			if (OutError) { *OutError = "unsupported save schema " + std::to_string(Version); }
			return false;
		}

		FCampaignProgress Loaded;
		if (const Json::FValue* ProgressJson = Root.Find("progress"))
		{
			Loaded.MissionId = ProgressJson->GetString("mission");
			Loaded.CheckpointBeatId = ProgressJson->GetString("checkpoint");
			if (const Json::FValue* Completed = ProgressJson->Find("completed"))
			{
				for (const Json::FValue& Item : Completed->Items())
				{
					if (Item.IsString())
					{
						Loaded.CompletedMissions.push_back(Item.AsString());
					}
				}
			}
			if (const Json::FValue* Outcomes = ProgressJson->Find("outcomes"))
			{
				for (const auto& Member : Outcomes->Members())
				{
					Loaded.Outcomes[Member.first] = Member.second.AsString();
				}
			}
		}

		FConsequenceState LoadedConsequences(Consequences.GetSchema());
		if (const Json::FValue* ConsequencesJson = Root.Find("consequences"))
		{
			if (!LoadedConsequences.FromJson(*ConsequencesJson, OutWarnings))
			{
				if (OutError) { *OutError = "'consequences' must be an object"; }
				return false;
			}
		}
		Progress = Loaded;
		Consequences = LoadedConsequences;
		return true;
	}
} // namespace GoN::Narrative
