// Ghosts of November — standalone runner for the pure-logic tests (no Unreal needed).
// Usage: gon_logic_tests [name-prefix] [--data <Plugins/GhostsOfNovember/Data>]
//        gon_logic_tests --validate [--data <dir>]   print every data issue (errors and warnings)
#include "Logic/GoNFileSystem.h"
#include "Logic/GoNLogicTest.h"
#include "Logic/GoNNarrativeLoader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifndef GON_DEFAULT_DATA_DIR
#define GON_DEFAULT_DATA_DIR ""
#endif

namespace
{
	class FStdFileSystem final : public GoN::IFileSystem
	{
	public:
		bool ReadTextFile(const std::string& Path, std::string& OutText) const override
		{
			std::ifstream Stream(std::filesystem::path(reinterpret_cast<const char8_t*>(Path.c_str())), std::ios::binary);
			if (!Stream)
			{
				return false;
			}
			OutText.assign(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
			return true;
		}

		std::vector<std::string> ListFiles(const std::string& Directory, const std::string& Extension) const override
		{
			std::vector<std::string> Names;
			std::error_code Error;
			const std::filesystem::path Dir(reinterpret_cast<const char8_t*>(Directory.c_str()));
			for (const auto& Entry : std::filesystem::directory_iterator(Dir, Error))
			{
				if (Entry.is_regular_file() && Entry.path().extension().string() == Extension)
				{
					const std::u8string Name = Entry.path().filename().u8string();
					Names.emplace_back(Name.begin(), Name.end());
				}
			}
			std::sort(Names.begin(), Names.end());
			return Names;
		}
	};
} // namespace

int main(int ArgCount, char** Args)
{
	std::string Prefix;
	std::string DataDirectory = GON_DEFAULT_DATA_DIR;
	bool bValidateOnly = false;
	for (int Index = 1; Index < ArgCount; ++Index)
	{
		if (std::strcmp(Args[Index], "--validate") == 0)
		{
			bValidateOnly = true;
		}
		else if (std::strcmp(Args[Index], "--data") == 0 && Index + 1 < ArgCount)
		{
			DataDirectory = Args[++Index];
		}
		else
		{
			Prefix = Args[Index];
		}
	}

	static FStdFileSystem FileSystem;
	GoN::LogicTest::DataAccess().FileSystem = &FileSystem;
	GoN::LogicTest::DataAccess().DataDirectory = DataDirectory;

	if (bValidateOnly)
	{
		GoN::Narrative::FCampaign Campaign;
		GoN::Narrative::FIssueList Issues;
		GoN::Narrative::LoadAndValidateCampaign(FileSystem, GoN::JoinPath(DataDirectory, "Narrative"), Campaign, Issues);
		for (const GoN::Narrative::FIssue& Issue : Issues.Items)
		{
			std::printf("%s\n", Issue.ToString().c_str());
		}
		std::printf("\n%zu missions, %d errors, %d warnings\n", Campaign.Missions.size(), Issues.ErrorCount(), Issues.WarningCount());
		return Issues.HasErrors() ? 1 : 0;
	}

	std::vector<GoN::LogicTest::FCase> Cases = GoN::LogicTest::CasesWithPrefix(Prefix);
	std::sort(Cases.begin(), Cases.end(), [](const GoN::LogicTest::FCase& A, const GoN::LogicTest::FCase& B) { return std::strcmp(A.Name, B.Name) < 0; });

	int Failed = 0;
	int Checks = 0;
	for (const GoN::LogicTest::FCase& Case : Cases)
	{
		GoN::LogicTest::FContext Context;
		Case.Function(Context);
		Checks += Context.Checks;
		if (Context.Failures.empty())
		{
			std::printf("[ OK ] %s (%d checks)\n", Case.Name, Context.Checks);
			continue;
		}
		++Failed;
		std::printf("[FAIL] %s\n", Case.Name);
		for (const std::string& Failure : Context.Failures)
		{
			std::printf("       %s\n", Failure.c_str());
		}
	}
	std::printf("\n%zu tests, %d checks, %d failed (data: %s)\n", Cases.size(), Checks, Failed, DataDirectory.c_str());
	return (Failed == 0 && !Cases.empty()) ? 0 : 1;
}
