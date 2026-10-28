#include "GoNTimelineValidator.h"

#include "GoNDeveloperSettings.h"
#include "GoNNarrativeLog.h"
#include "GoNStringConv.h"
#include "GoNUnrealFileSystem.h"
#include "Interfaces/IPluginManager.h"
#include "Logic/GoNNarrativeLoader.h"

FString UGoNTimelineValidator::GetNarrativeDirectory()
{
	const FString& Override = UGoNDeveloperSettings::Get()->NarrativeDataDirectoryOverride.Path;
	if (!Override.IsEmpty())
	{
		return Override;
	}
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GhostsOfNovember"));
	return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Data") / TEXT("Narrative")) : FString();
}

int32 UGoNTimelineValidator::ValidateNarrativeData(TArray<FString>& OutMessages, bool bIncludeWarnings)
{
	OutMessages.Reset();
	const FGoNUnrealFileSystem FileSystem;
	GoN::Narrative::FCampaign Campaign;
	GoN::Narrative::FIssueList Issues;
	GoN::Narrative::LoadAndValidateCampaign(FileSystem, GoN::ToStd(GetNarrativeDirectory()), Campaign, Issues);
	for (const GoN::Narrative::FIssue& Issue : Issues.Items)
	{
		if (Issue.Severity == GoN::Narrative::ESeverity::Error || bIncludeWarnings)
		{
			OutMessages.Add(GoN::ToFString(Issue.ToString()));
		}
	}
	UE_LOG(LogGoNNarrative, Log, TEXT("GoN Narrative: timeline validation of %d missions: %d errors, %d warnings"), static_cast<int32>(Campaign.Missions.size()), Issues.ErrorCount(), Issues.WarningCount());
	return Issues.ErrorCount();
}
