#include "GoNLogicAutomation.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GoNTimelineValidator.h"

// Every pure-logic case "Narrative.*" (graph, runner, consequences, saves, timeline, shipped data).
GON_IMPLEMENT_LOGIC_AUTOMATION_TESTS(FGoNNarrativeLogicTests, "GoN.Narrative.Logic", "Narrative.")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoNTimelineValidatorTest, "GoN.Narrative.TimelineValidator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGoNTimelineValidatorTest::RunTest(const FString& Parameters)
{
	TArray<FString> Messages;
	const int32 Errors = UGoNTimelineValidator::ValidateNarrativeData(Messages, false);
	for (const FString& Message : Messages)
	{
		AddError(Message);
	}
	TestEqual(TEXT("timeline validation errors in Data/Narrative"), Errors, 0);
	return Errors == 0;
}

#endif
