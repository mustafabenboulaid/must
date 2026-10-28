// Ghosts of November — campaign progress + consequences, serialized for checkpoints (pure C++).
// Unreal stores the string in UGoNSaveGame (GoNCore); this side owns the format.
#pragma once

#include "Logic/GoNConsequences.h"

#include <map>
#include <string>
#include <vector>

namespace GoN::Narrative
{
	struct FCampaignProgress
	{
		std::string MissionId;        // mission in progress ("" between missions)
		std::string CheckpointBeatId; // last checkpoint reached in that mission
		std::vector<std::string> CompletedMissions;
		std::map<std::string, std::string> Outcomes; // mission id -> outcome
	};

	class FCampaignState
	{
	public:
		static constexpr int SchemaVersion = 1;

		explicit FCampaignState(const FConsequenceSchema* Schema = nullptr) : Consequences(Schema) {}

		FConsequenceState Consequences;
		FCampaignProgress Progress;

		void Reset();
		void RecordMissionEnd(const std::string& MissionId, const std::string& Outcome);
		bool IsMissionCompleted(const std::string& MissionId) const;

		std::string Serialize() const;
		// Keeps the current schema pointer; unknown keys from older saves become warnings.
		bool Deserialize(const std::string& Text, std::string* OutError, std::vector<std::string>* OutWarnings = nullptr);
	};
} // namespace GoN::Narrative
