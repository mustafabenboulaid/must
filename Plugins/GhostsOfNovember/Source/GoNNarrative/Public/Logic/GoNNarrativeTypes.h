// Ghosts of November — narrative data model (pure C++, no Unreal headers).
// Campaign -> Chapter -> Mission -> Beat, plus the historical database the
// timeline validator checks against. Loaded from Plugins/GhostsOfNovember/Data/Narrative.
#pragma once

#include "Logic/GoNDate.h"

#include <map>
#include <string>
#include <vector>

namespace GoN::Narrative
{
	// Keys under this prefix describe documented history; gameplay effects may never write them.
	inline const char* ReservedHistoryPrefix() { return "history."; }

	enum class ESeverity
	{
		Info,
		Warning,
		Error
	};

	inline const char* SeverityName(ESeverity Severity)
	{
		switch (Severity)
		{
		case ESeverity::Info: return "info";
		case ESeverity::Warning: return "warning";
		case ESeverity::Error: return "error";
		}
		return "?";
	}

	struct FIssue
	{
		ESeverity Severity = ESeverity::Error;
		std::string Code;    // stable id, e.g. "timeline.two_places"
		std::string Context; // where: file, mission, beat or line id
		std::string Message;

		std::string ToString() const { return std::string(SeverityName(Severity)) + " [" + Code + "] " + Context + ": " + Message; }
	};

	struct FIssueList
	{
		std::vector<FIssue> Items;

		void Add(ESeverity Severity, const std::string& Code, const std::string& Context, const std::string& Message)
		{
			Items.push_back(FIssue{Severity, Code, Context, Message});
		}
		int Count(ESeverity Severity) const
		{
			int N = 0;
			for (const FIssue& Issue : Items)
			{
				N += Issue.Severity == Severity ? 1 : 0;
			}
			return N;
		}
		int ErrorCount() const { return Count(ESeverity::Error); }
		int WarningCount() const { return Count(ESeverity::Warning); }
		bool HasErrors() const { return ErrorCount() > 0; }
		bool HasCode(const std::string& Code) const
		{
			for (const FIssue& Issue : Items)
			{
				if (Issue.Code == Code)
				{
					return true;
				}
			}
			return false;
		}
	};

	// Language code -> text ("ar-dz", "ar", "fr", "tzm", "en").
	using FLocalizedText = std::map<std::string, std::string>;

	// ---- Consequences -------------------------------------------------------

	enum class EEffectOp
	{
		Add,
		Set,
		SetFlag,
		ClearFlag
	};

	struct FEffect
	{
		EEffectOp Op = EEffectOp::Add;
		std::string Key;
		double Value = 0.0;
	};

	enum class ECompare
	{
		Equal,
		NotEqual,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		FlagSet,
		FlagUnset
	};

	struct FCondition
	{
		std::string Key;
		ECompare Compare = ECompare::GreaterEqual;
		double Value = 0.0;
	};

	// A stat ("suspicion.aures") or a pattern ("trust.*") with its clamp range.
	struct FStatDef
	{
		std::string Key;
		double Min = 0.0;
		double Max = 100.0;
		double Default = 0.0;

		bool IsPattern() const { return Key.size() >= 2 && Key.compare(Key.size() - 2, 2, ".*") == 0; }
		bool Matches(const std::string& InKey) const
		{
			if (!IsPattern())
			{
				return Key == InKey;
			}
			const std::string Prefix = Key.substr(0, Key.size() - 1); // keep the dot
			return InKey.size() > Prefix.size() && InKey.compare(0, Prefix.size(), Prefix) == 0;
		}
	};

	struct FConsequenceSchema
	{
		std::vector<FStatDef> Stats;
		std::vector<std::string> FlagPatterns; // "info.*", "fact.network_cell_arrested"

		// Exact keys win over patterns.
		const FStatDef* FindStat(const std::string& Key) const
		{
			const FStatDef* PatternMatch = nullptr;
			for (const FStatDef& Def : Stats)
			{
				if (!Def.IsPattern() && Def.Key == Key)
				{
					return &Def;
				}
				if (Def.IsPattern() && PatternMatch == nullptr && Def.Matches(Key))
				{
					PatternMatch = &Def;
				}
			}
			return PatternMatch;
		}

		bool IsKnownFlag(const std::string& Key) const
		{
			for (const std::string& Pattern : FlagPatterns)
			{
				FStatDef Probe;
				Probe.Key = Pattern;
				if (Probe.Matches(Key))
				{
					return true;
				}
			}
			return false;
		}
	};

	// ---- Dialogue -------------------------------------------------------------

	struct FDialogueLine
	{
		std::string Id;
		std::string SpeakerId;
		std::string Language = "ar-dz";
		std::string Text;
		std::string VoiceAsset; // object path of the VO sound, optional
		bool bDramatized = false;
		std::string SourceId; // required for real people unless dramatized
		bool bRemote = false; // heard over the network/radio: speaker need not be present
	};

	// ---- Beats and missions ---------------------------------------------------

	enum class EBeatType
	{
		Objective,
		Trigger,
		Dialogue,
		Cinematic,
		Scene,
		Wait,
		Branch,
		Checkpoint,
		End
	};

	inline const char* BeatTypeName(EBeatType Type)
	{
		switch (Type)
		{
		case EBeatType::Objective: return "objective";
		case EBeatType::Trigger: return "trigger";
		case EBeatType::Dialogue: return "dialogue";
		case EBeatType::Cinematic: return "cinematic";
		case EBeatType::Scene: return "scene";
		case EBeatType::Wait: return "wait";
		case EBeatType::Branch: return "branch";
		case EBeatType::Checkpoint: return "checkpoint";
		case EBeatType::End: return "end";
		}
		return "?";
	}

	// Beats that finish immediately when entered.
	inline bool IsInstantBeat(EBeatType Type)
	{
		return Type == EBeatType::Branch || Type == EBeatType::Checkpoint || Type == EBeatType::End;
	}

	// Beats that wait for a gameplay event (CompleteOn).
	inline bool IsEventBeat(EBeatType Type)
	{
		return Type == EBeatType::Objective || Type == EBeatType::Trigger || Type == EBeatType::Dialogue || Type == EBeatType::Cinematic || Type == EBeatType::Scene;
	}

	struct FBranch
	{
		std::vector<FCondition> Conditions; // all must hold
		std::string Next;
	};

	struct FBeat
	{
		std::string Id;
		EBeatType Type = EBeatType::Objective;
		std::string Next;
		std::string CompleteOn; // event name; defaults to "<beatId>.done"
		double WaitSeconds = 0.0;
		std::vector<FBranch> Branches;
		std::string ElseNext;
		std::vector<FEffect> OnEnter;
		std::vector<FEffect> OnComplete;
		std::vector<std::string> LineIds; // dialogue beats
		std::string Asset;                // level sequence / scene id for cinematic & scene beats
		FLocalizedText Text;              // objective text, card text
		std::string Outcome;              // end beats: "success", "failed", ...
	};

	// Design doc §12: every mission needs these, stated by the writer.
	struct FMissionDesign
	{
		std::string VisibleGoal;
		std::string DramaticGoal;
		std::string Obstacle;
		std::string Decision;
		std::string Consequence;
		std::string Transition;
	};

	struct FTimelineEntry
	{
		FDateRange Dates;
		std::string PlaceId;
		std::vector<std::string> CharacterIds; // physically present
	};

	struct FMission
	{
		std::string Id;
		std::string ChapterId;
		std::string SourceFile;
		FLocalizedText Title;
		FMissionDesign Design;
		FTimelineEntry Timeline;
		std::string StartBeat;
		std::vector<FBeat> Beats;
		std::vector<FDialogueLine> Lines;
		bool bTestOnly = false; // gym/test content: excluded from the campaign timeline

		const FBeat* FindBeat(const std::string& BeatId) const
		{
			for (const FBeat& Beat : Beats)
			{
				if (Beat.Id == BeatId)
				{
					return &Beat;
				}
			}
			return nullptr;
		}
		const FDialogueLine* FindLine(const std::string& LineId) const
		{
			for (const FDialogueLine& Line : Lines)
			{
				if (Line.Id == LineId)
				{
					return &Line;
				}
			}
			return nullptr;
		}
	};

	struct FChapter
	{
		std::string Id;
		int Order = 0;
		FLocalizedText Title;
		std::vector<std::string> MissionIds;
	};

	// ---- Historical database ----------------------------------------------------

	struct FPresenceWindow
	{
		std::string Region;
		FDateRange Dates;
		std::string Note;
		std::vector<std::string> SourceIds;
	};

	struct FCharacter
	{
		std::string Id;
		std::string NameAr;
		std::string NameLatin;
		bool bHistorical = false;
		bool bExcluded = false; // must never appear (e.g. outside the game's period)
		std::string ExcludedReason;
		bool bHasDeath = false;
		FDateRange Death; // From == To when undisputed; a range when sources differ
		std::vector<std::string> DeathSourceIds;
		std::vector<FPresenceWindow> Presence; // documented whereabouts (historical only)
		bool bApproved = true;                 // fictional names/backstories need the user's approval
		std::string Review;                    // "pending", "approved", consultant notes
	};

	struct FPlace
	{
		std::string Id;
		std::string Region;
		std::string NameAr;
		bool bFictional = false;
	};

	enum class ECertainty
	{
		Documented,
		Probable,
		Disputed,
		Reconstruction
	};

	struct FSource
	{
		std::string Id;
		std::string Citation;
		std::string Url;
		ECertainty Certainty = ECertainty::Documented;
	};

	struct FCampaign
	{
		std::string Id;
		std::vector<FChapter> Chapters;
		std::vector<FMission> Missions;
		std::map<std::string, FCharacter> Characters;
		std::map<std::string, FPlace> Places;
		std::map<std::string, FSource> Sources;
		FConsequenceSchema Schema;

		const FMission* FindMission(const std::string& MissionId) const
		{
			for (const FMission& Mission : Missions)
			{
				if (Mission.Id == MissionId)
				{
					return &Mission;
				}
			}
			return nullptr;
		}
		const FCharacter* FindCharacter(const std::string& CharacterId) const
		{
			const auto It = Characters.find(CharacterId);
			return It == Characters.end() ? nullptr : &It->second;
		}
		const FPlace* FindPlace(const std::string& PlaceId) const
		{
			const auto It = Places.find(PlaceId);
			return It == Places.end() ? nullptr : &It->second;
		}
	};
} // namespace GoN::Narrative
