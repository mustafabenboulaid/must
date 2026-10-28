#include "Logic/GoNTimeline.h"

#include <algorithm>
#include <set>

namespace GoN::Narrative
{
	namespace TimelineDetail
	{
		bool Contains(const std::vector<std::string>& List, const std::string& Value)
		{
			return std::find(List.begin(), List.end(), Value) != List.end();
		}

		std::string DescribePresence(const FCharacter& Character)
		{
			std::string Out;
			for (const FPresenceWindow& Window : Character.Presence)
			{
				Out += (Out.empty() ? "" : ", ") + Window.Region + " " + Window.Dates.ToString();
			}
			return Out;
		}

		void CheckSourceIds(const FCampaign& Campaign, const std::vector<std::string>& SourceIds, const std::string& Context, FIssueList& Issues)
		{
			for (const std::string& SourceId : SourceIds)
			{
				if (Campaign.Sources.count(SourceId) == 0)
				{
					Issues.Add(ESeverity::Error, "source.unknown", Context, "source '" + SourceId + "' is not in sources.json");
				}
			}
		}

		void CheckCharacterInMission(const FMission& Mission, const FPlace* Place, const FCharacter& Character, FIssueList& Issues)
		{
			const std::string Ctx = Mission.Id + "/" + Character.Id;
			if (Character.bExcluded)
			{
				Issues.Add(ESeverity::Error, "history.excluded", Ctx, Character.NameLatin + " must not appear: " + Character.ExcludedReason);
			}
			if (!Character.bApproved)
			{
				Issues.Add(ESeverity::Warning, "character.unapproved", Ctx, "fictional character name/backstory still awaits approval");
			}
			if (Mission.bTestOnly || !Character.bHistorical)
			{
				return;
			}

			const FDateRange& Dates = Mission.Timeline.Dates;
			if (Character.bHasDeath)
			{
				if (Dates.To > Character.Death.To)
				{
					Issues.Add(ESeverity::Error, "timeline.after_death", Ctx, Character.NameLatin + " died " + (Character.Death.From == Character.Death.To ? Character.Death.From.ToString() : "between " + Character.Death.ToString()) + "; mission runs " + Dates.ToString());
				}
				else if (Character.Death.From < Character.Death.To && Dates.To > Character.Death.From)
				{
					Issues.Add(ESeverity::Warning, "timeline.death_disputed", Ctx, "mission " + Dates.ToString() + " reaches the disputed death window " + Character.Death.ToString() + "; the historical consultant must decide");
				}
			}

			if (Place == nullptr)
			{
				return;
			}
			if (Character.Presence.empty())
			{
				Issues.Add(ESeverity::Warning, "history.presence_unknown", Ctx, "no documented presence windows for " + Character.NameLatin + "; region cannot be checked");
				return;
			}
			bool bDocumented = false;
			for (const FPresenceWindow& Window : Character.Presence)
			{
				bDocumented = bDocumented || (Window.Region == Place->Region && Window.Dates.Contains(Dates));
			}
			if (!bDocumented)
			{
				Issues.Add(ESeverity::Error, "history.region", Ctx, Character.NameLatin + " is not documented in region '" + Place->Region + "' during " + Dates.ToString() + " (documented: " + DescribePresence(Character) + ")");
			}
		}
	} // namespace TimelineDetail

	void ValidateDialogue(const FCampaign& Campaign, const FMission& Mission, FIssueList& Issues)
	{
		static const char* const KnownLanguages[] = {"ar-dz", "ar", "fr", "tzm", "en"};
		for (const FDialogueLine& Line : Mission.Lines)
		{
			const std::string Ctx = Mission.Id + "/line:" + Line.Id;
			const FCharacter* Speaker = Campaign.FindCharacter(Line.SpeakerId);
			if (Speaker == nullptr)
			{
				Issues.Add(ESeverity::Error, "dialogue.speaker_unknown", Ctx, "speaker '" + Line.SpeakerId + "' is not in characters.json");
				continue;
			}
			if (Speaker->bExcluded)
			{
				Issues.Add(ESeverity::Error, "history.excluded", Ctx, Speaker->NameLatin + " must not appear: " + Speaker->ExcludedReason);
			}
			if (Speaker->bHistorical && !Line.bDramatized && Line.SourceId.empty())
			{
				Issues.Add(ESeverity::Error, "dialogue.unsourced", Ctx, "line attributed to " + Speaker->NameLatin + " needs a 'source' or \"dramatized\": true");
			}
			if (Speaker->bHistorical && Line.bDramatized && !Line.SourceId.empty())
			{
				Issues.Add(ESeverity::Warning, "dialogue.sourced_and_dramatized", Ctx, "line has a source but is also tagged dramatized; pick one");
			}
			if (!Line.SourceId.empty() && Campaign.Sources.count(Line.SourceId) == 0)
			{
				Issues.Add(ESeverity::Error, "source.unknown", Ctx, "source '" + Line.SourceId + "' is not in sources.json");
			}
			if (!Line.bRemote && !TimelineDetail::Contains(Mission.Timeline.CharacterIds, Line.SpeakerId))
			{
				Issues.Add(ESeverity::Error, "dialogue.speaker_absent", Ctx, "speaker '" + Line.SpeakerId + "' is not present in the mission timeline (use \"remote\": true for radio/network voices)");
			}
			bool bKnownLanguage = false;
			for (const char* Language : KnownLanguages)
			{
				bKnownLanguage = bKnownLanguage || Line.Language == Language;
			}
			if (!bKnownLanguage)
			{
				Issues.Add(ESeverity::Warning, "dialogue.language", Ctx, "unknown language '" + Line.Language + "' (ar-dz, ar, fr, tzm, en)");
			}
			if (Line.Text.empty())
			{
				Issues.Add(ESeverity::Warning, "dialogue.text_empty", Ctx, "line has no text");
			}
		}
	}

	void ValidateTimeline(const FCampaign& Campaign, FIssueList& Issues)
	{
		using namespace TimelineDetail;

		for (const auto& Pair : Campaign.Characters)
		{
			const FCharacter& Character = Pair.second;
			CheckSourceIds(Campaign, Character.DeathSourceIds, "characters.json/" + Character.Id + "/death", Issues);
			for (const FPresenceWindow& Window : Character.Presence)
			{
				CheckSourceIds(Campaign, Window.SourceIds, "characters.json/" + Character.Id + "/presence", Issues);
			}
			if (!Character.bHistorical && !Character.Presence.empty())
			{
				Issues.Add(ESeverity::Warning, "character.fictional_presence", "characters.json/" + Character.Id, "presence windows are only checked for historical characters");
			}
		}

		for (const FMission& Mission : Campaign.Missions)
		{
			const FPlace* Place = Campaign.FindPlace(Mission.Timeline.PlaceId);
			if (Place == nullptr && !Mission.Timeline.PlaceId.empty())
			{
				Issues.Add(ESeverity::Error, "timeline.place_unknown", Mission.Id, "place '" + Mission.Timeline.PlaceId + "' is not in places.json");
			}
			std::set<std::string> Seen;
			for (const std::string& CharacterId : Mission.Timeline.CharacterIds)
			{
				if (!Seen.insert(CharacterId).second)
				{
					Issues.Add(ESeverity::Warning, "timeline.character_twice", Mission.Id, "character '" + CharacterId + "' is listed twice");
					continue;
				}
				const FCharacter* Character = Campaign.FindCharacter(CharacterId);
				if (Character == nullptr)
				{
					Issues.Add(ESeverity::Error, "timeline.character_unknown", Mission.Id, "character '" + CharacterId + "' is not in characters.json");
					continue;
				}
				CheckCharacterInMission(Mission, Place, *Character, Issues);
			}
			ValidateDialogue(Campaign, Mission, Issues);
		}

		// Nobody (historical or fictional) is in two regions at once.
		for (size_t A = 0; A < Campaign.Missions.size(); ++A)
		{
			const FMission& First = Campaign.Missions[A];
			const FPlace* FirstPlace = Campaign.FindPlace(First.Timeline.PlaceId);
			if (First.bTestOnly || FirstPlace == nullptr)
			{
				continue;
			}
			for (size_t B = A + 1; B < Campaign.Missions.size(); ++B)
			{
				const FMission& Second = Campaign.Missions[B];
				const FPlace* SecondPlace = Campaign.FindPlace(Second.Timeline.PlaceId);
				if (Second.bTestOnly || SecondPlace == nullptr || FirstPlace->Region == SecondPlace->Region)
				{
					continue;
				}
				if (!First.Timeline.Dates.Overlaps(Second.Timeline.Dates))
				{
					continue;
				}
				for (const std::string& CharacterId : First.Timeline.CharacterIds)
				{
					if (Contains(Second.Timeline.CharacterIds, CharacterId))
					{
						Issues.Add(ESeverity::Error, "timeline.two_places", CharacterId, "in '" + First.Id + "' (" + FirstPlace->Region + ", " + First.Timeline.Dates.ToString() + ") and '" + Second.Id + "' (" + SecondPlace->Region + ", " + Second.Timeline.Dates.ToString() + ") at the same time");
					}
				}
			}
		}
	}
} // namespace GoN::Narrative
