#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GoNTimelineValidator.generated.h"

// Brief §2/§4: validates the central timeline and historical rules on the narrative data
// (no one in two regions at once, nobody after their death, historical figures only in
// documented regions, real people's lines sourced or dramatized, excluded figures absent,
// plus every mission graph). Usable from the editor, Blueprints, `gon.timeline.validate`
// and the automation test "GoN.Narrative.TimelineValidator".
UCLASS()
class GONNARRATIVE_API UGoNTimelineValidator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Reads the JSON from disk (independent of any running game). Returns the error count.
	UFUNCTION(BlueprintCallable, Category = "GoN|Narrative")
	static int32 ValidateNarrativeData(TArray<FString>& OutMessages, bool bIncludeWarnings = true);

	// Plugins/GhostsOfNovember/Data/Narrative unless overridden in Project Settings.
	static FString GetNarrativeDirectory();
};
