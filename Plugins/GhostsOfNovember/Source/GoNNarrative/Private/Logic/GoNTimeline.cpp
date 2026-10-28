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

		std::string EntryContext(const FMission& Mission, const FTimelineEntry& Entry)
		{
			return Entry.Id.empty() ? Mission.Id : Mission.Id + "/segment:" + Entry.Id;
		}

		// The mission-level entry first, then every segment.
		std::vector<const FTimelineEntry*> Entries(const FMission& Mission)
		{
			std::vector<const FTimelineEntry*> Out{&Mission.Timeline};
			for (const FTimelineEntry& Segment : Mission.Segments)
			{
				Out.push_back(&Segment);
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

		void CheckCharacterInEntry(const FMission& Mission, const FTimelineEntry& Entry, const FPlace* Place, const FCharacter& Character, FIssueList& Issues)
		{
			const std::string Ctx = EntryContext(Mission, Entry) + "/" + Character.Id;
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

			const FDateRange& Dates = Entry.Dates;
			if (Character.bHasDeath)
			{
				if (Dates.To > Character.Death.To)
				{
					Issues.Add(ESeverity::Error, "timeline.after_death", Ctx, Character.NameLatin + " died " + (Character.Death.From == Character.Death.To ? Character.Death.From.ToString() : "between " + Character.Death.ToString()) + "; entry runs " + Dates.ToString());
				}
				else if (Character.Death.From < Character.Death.To && Dates.To > Character.Death.From)
				{
					Issues.Add(ESeverity::Warning, "timeline.death_disputed", Ctx, "entry " + Dates.ToString() + " reaches the disputed death window " + Character.Death.ToString() + "; the historical consultant must decide");
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

		void CheckEntry(const FCampaign& Campaign, const FMission& Mission, const FTimelineEntry& Entry, FIssueList& Issues)
		{
			const std::string Ctx = EntryContext(Mission, Entry);
			const FPlace* Place = Campaign.FindPlace(Entry.PlaceId);
			if (Place == nullptr && !Entry.PlaceId.empty())
			{
				Issues.Add(ESeverity::Error, "timeline.place_unknown", Ctx, "place '" + Entry.PlaceId + "' is not in places.json");
			}
			if (!Entry.Id.empty() && !Mission.Timeline.Dates.Contains(Entry.Dates))
			{
				Issues.Add(ESeverity::Error, "timeline.segment_outside", Ctx, "segment " + Entry.Dates.ToString() + " is outside the mission's dates " + Mission.Timeline.Dates.ToString());
			}
			if (!Entry.PovCharacterId.empty() && !Contains(Entry.CharacterIds, Entry.PovCharacterId))
			{
				Issues.Add(ESeverity::Error, "timeline.pov_absent", Ctx, "POV character '" + Entry.PovCharacterId + "' is not listed as present");
			}
			std::set<std::string> Seen;
			for (const std::string& CharacterId : Entry.CharacterIds)
			{
				if (!Seen.insert(CharacterId).second)
				{
					Issues.Add(ESeverity::Warning, "timeline.character_twice", Ctx, "character '" + CharacterId + "' is listed twice");
					continue;
				}
				const FCharacter* Character = Campaign.FindCharacter(CharacterId);
				if (Character == nullptr)
				{
					Issues.Add(ESeverity::Error, "timeline.character_unknown", Ctx, "character '" + CharacterId + "' is not in characters.json");
					continue;
				}
				CheckCharacterInEntry(Mission, Entry, Place, *Character, Issues);
			}
		}

		void CheckLinePresence(const FMission& Mission, const FDialogueLine& Line, const FTimelineEntry& Entry, FIssueList& Issues)
		{
			if (!Line.bRemote && !Contains(Entry.CharacterIds, Line.SpeakerId))
			{
				Issues.Add(ESeverity::Error, "dialogue.speaker_absent", Mission.Id + "/line:" + Line.Id, "speaker '" + Line.SpeakerId + "' is not present in " + (Entry.Id.empty() ? std::string("the mission timeline") : "segment '" + Entry.Id + "'") + " (use \"remote\": true for radio/network voices)");
			}
		}
	} // namespace TimelineDetail

	void ValidateDialogue(const FCampaign& Campaign, const FMission& Mission, FIssueList& Issues)
	{
		using namespace TimelineDetail;
		static const char* const KnownLanguages[] = {"ar-dz", "ar", "fr", "tzm", "en"};

		// Which timeline entries each line is spoken in (through the dialogue beats that use it).
		std::set<std::string> ReferencedLines;
		for (const FBeat& Beat : Mission.Beats)
		{
			if (Beat.Type != EBeatType::Dialogue)
			{
				continue;
			}
			const FTimelineEntry* Entry = Mission.FindTimelineEntry(Beat.Segment);
			for (const std::string& LineId : Beat.LineIds)
			{
				const FDialogueLine* Line = Mission.FindLine(LineId);
				if (Line == nullptr || Entry == nullptr)
				{
					continue; // reported by the graph / segment checks
				}
				ReferencedLines.insert(LineId);
				CheckLinePresence(Mission, *Line, *Entry, Issues);
			}
		}

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
			if (ReferencedLines.count(Line.Id) == 0 && !Line.bRemote)
			{
				// Not used by any beat yet: the speaker must at least be present somewhere in the mission.
				bool bPresent = false;
				for (const FTimelineEntry* Entry : Entries(Mission))
				{
					bPresent = bPresent || Contains(Entry->CharacterIds, Line.SpeakerId);
				}
				if (!bPresent)
				{
					CheckLinePresence(Mission, Line, Mission.Timeline, Issues);
				}
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
			const std::string Ctx = "characters.json/" + Character.Id;
			CheckSourceIds(Campaign, Character.DeathSourceIds, Ctx + "/death", Issues);
			for (size_t A = 0; A < Character.Presence.size(); ++A)
			{
				CheckSourceIds(Campaign, Character.Presence[A].SourceIds, Ctx + "/presence", Issues);
				for (size_t B = A + 1; B < Character.Presence.size(); ++B)
				{
					const FPresenceWindow& First = Character.Presence[A];
					const FPresenceWindow& Second = Character.Presence[B];
					if (First.Region != Second.Region && First.Dates.Overlaps(Second.Dates))
					{
						Issues.Add(ESeverity::Error, "history.presence_overlap", Ctx, "documented in '" + First.Region + "' " + First.Dates.ToString() + " and '" + Second.Region + "' " + Second.Dates.ToString() + " at the same time");
					}
				}
			}
			if (!Character.bHistorical && !Character.Presence.empty())
			{
				Issues.Add(ESeverity::Warning, "character.fictional_presence", Ctx, "presence windows are only checked for historical characters");
			}
		}

		for (const FMission& Mission : Campaign.Missions)
		{
			std::set<std::string> SegmentIds;
			for (const FTimelineEntry& Segment : Mission.Segments)
			{
				if (Segment.Id.empty() || !SegmentIds.insert(Segment.Id).second)
				{
					Issues.Add(ESeverity::Error, "timeline.segment_id", Mission.Id, "segment id '" + Segment.Id + "' is empty or duplicated");
				}
			}
			for (const FBeat& Beat : Mission.Beats)
			{
				if (Mission.FindTimelineEntry(Beat.Segment) == nullptr)
				{
					Issues.Add(ESeverity::Error, "timeline.segment_unknown", Mission.Id + "/" + Beat.Id, "beat uses unknown segment '" + Beat.Segment + "'");
				}
			}
			for (const FTimelineEntry* Entry : Entries(Mission))
			{
				CheckEntry(Campaign, Mission, *Entry, Issues);
			}
			ValidateDialogue(Campaign, Mission, Issues);
		}

		// Nobody (historical or fictional) is in two regions at once. A mission's own entries are
		// compared with each other too (segments), but not with the mission-level summary entry.
		struct FPlacedEntry
		{
			const FMission* Mission;
			const FTimelineEntry* Entry;
			const FPlace* Place;
		};
		std::vector<FPlacedEntry> Placed;
		for (const FMission& Mission : Campaign.Missions)
		{
			if (Mission.bTestOnly)
			{
				continue;
			}
			for (const FTimelineEntry* Entry : Entries(Mission))
			{
				if (const FPlace* Place = Campaign.FindPlace(Entry->PlaceId))
				{
					Placed.push_back(FPlacedEntry{&Mission, Entry, Place});
				}
			}
		}
		for (size_t A = 0; A < Placed.size(); ++A)
		{
			for (size_t B = A + 1; B < Placed.size(); ++B)
			{
				const FPlacedEntry& First = Placed[A];
				const FPlacedEntry& Second = Placed[B];
				const bool bSameMission = First.Mission == Second.Mission;
				if ((bSameMission && (First.Entry->Id.empty() || Second.Entry->Id.empty())) || First.Place->Region == Second.Place->Region || !First.Entry->Dates.Overlaps(Second.Entry->Dates))
				{
					continue;
				}
				for (const std::string& CharacterId : First.Entry->CharacterIds)
				{
					if (Contains(Second.Entry->CharacterIds, CharacterId))
					{
						Issues.Add(ESeverity::Error, "timeline.two_places", CharacterId, "in '" + EntryContext(*First.Mission, *First.Entry) + "' (" + First.Place->Region + ", " + First.Entry->Dates.ToString() + ") and '" + EntryContext(*Second.Mission, *Second.Entry) + "' (" + Second.Place->Region + ", " + Second.Entry->Dates.ToString() + ") at the same time");
					}
				}
			}
		}
	}
} // namespace GoN::Narrative
