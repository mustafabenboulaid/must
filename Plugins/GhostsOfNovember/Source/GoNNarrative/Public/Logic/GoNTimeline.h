// Ghosts of November — timeline and historical-integrity validation (pure C++).
// Brief §2: one central timeline; no historical character in two places at once, after
// their death, or moved out of their documented region; real people's lines are sourced
// or tagged dramatized; excluded figures never appear.
#pragma once

#include "Logic/GoNNarrativeTypes.h"

namespace GoN::Narrative
{
	void ValidateTimeline(const FCampaign& Campaign, FIssueList& Issues);

	// Dialogue rules for one mission (also called by ValidateTimeline).
	void ValidateDialogue(const FCampaign& Campaign, const FMission& Mission, FIssueList& Issues);
} // namespace GoN::Narrative
