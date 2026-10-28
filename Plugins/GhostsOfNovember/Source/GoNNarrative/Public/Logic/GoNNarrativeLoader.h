// Ghosts of November — builds an FCampaign from the JSON files in Data/Narrative (pure C++).
// File access stays outside: Unreal and the standalone tests read the files and pass the text.
#pragma once

#include "Logic/GoNFileSystem.h"
#include "Logic/GoNNarrativeTypes.h"

#include <string>
#include <utility>
#include <vector>

namespace GoN::Narrative
{
	struct FCampaignFiles
	{
		std::string Campaign;     // campaign.json  — chapters and mission order
		std::string Characters;   // characters.json — historical + fictional people
		std::string Places;       // places.json
		std::string Sources;      // sources.json — citations with certainty
		std::string Consequences; // consequences.json — stat and flag schema
		std::vector<std::pair<std::string, std::string>> Missions; // (file name, text) from missions/*.json
	};

	// Parses everything it can and reports problems with file/line context.
	// Returns false only when the result is unusable (a file failed to parse).
	bool LoadCampaign(const FCampaignFiles& Files, FCampaign& Out, FIssueList& Issues);

	// Single-file helpers (tests build small fixtures with these).
	bool LoadMission(const std::string& FileName, const std::string& Text, FMission& Out, FIssueList& Issues);
	bool LoadConsequenceSchema(const std::string& Text, FConsequenceSchema& Out, FIssueList& Issues);
	bool LoadCharacters(const std::string& Text, std::map<std::string, FCharacter>& Out, FIssueList& Issues);
	bool LoadPlaces(const std::string& Text, std::map<std::string, FPlace>& Out, FIssueList& Issues);
	bool LoadSources(const std::string& Text, std::map<std::string, FSource>& Out, FIssueList& Issues);

	// Reads campaign.json, characters.json, places.json, sources.json, consequences.json and
	// missions/*.json from NarrativeDirectory (".../Data/Narrative").
	bool ReadCampaignFiles(const IFileSystem& FileSystem, const std::string& NarrativeDirectory, FCampaignFiles& Out, FIssueList& Issues);

	// ReadCampaignFiles + LoadCampaign + ValidateCampaign.
	bool LoadAndValidateCampaign(const IFileSystem& FileSystem, const std::string& NarrativeDirectory, FCampaign& Out, FIssueList& Issues);

	// Full data check: loader issues + every mission graph + timeline/history rules.
	void ValidateCampaign(const FCampaign& Campaign, FIssueList& Issues);
} // namespace GoN::Narrative
