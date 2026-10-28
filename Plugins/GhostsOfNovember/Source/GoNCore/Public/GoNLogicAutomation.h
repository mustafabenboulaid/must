#pragma once

#include "CoreMinimal.h"
#include "GoNStringConv.h"
#include "GoNUnrealFileSystem.h"
#include "Interfaces/IPluginManager.h"
#include "Logic/GoNLogicTest.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

// Exposes the pure-logic test cases whose names start with Prefix as one Unreal automation
// test per case: "<PrettyName>.<case name without Prefix>".
// The same cases run without Unreal in Tools/LogicTests.
namespace GoN::LogicTest
{
	inline void PrepareUnrealDataAccess()
	{
		static FGoNUnrealFileSystem FileSystem;
		FDataAccess& Access = DataAccess();
		Access.FileSystem = &FileSystem;
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GhostsOfNovember")))
		{
			Access.DataDirectory = ToStd(FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Data")));
		}
	}

	inline void GetUnrealTests(const char* Prefix, TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands)
	{
		const size_t PrefixLength = std::string(Prefix).size();
		for (const FCase& Case : CasesWithPrefix(Prefix))
		{
			OutBeautifiedNames.Add(ToFString(std::string(Case.Name).substr(PrefixLength)));
			OutTestCommands.Add(ToFString(Case.Name));
		}
	}

	inline bool RunUnrealTest(FAutomationTestBase& Test, const FString& CaseName)
	{
		PrepareUnrealDataAccess();
		const std::string Name = ToStd(CaseName);
		for (const FCase& Case : Registry())
		{
			if (Name == Case.Name)
			{
				FContext Context;
				Case.Function(Context);
				for (const std::string& Failure : Context.Failures)
				{
					Test.AddError(ToFString(Failure));
				}
				Test.AddInfo(FString::Printf(TEXT("%d checks"), Context.Checks));
				return Context.Failures.empty();
			}
		}
		Test.AddError(FString::Printf(TEXT("logic test '%s' not found"), *CaseName));
		return false;
	}
}

#define GON_IMPLEMENT_LOGIC_AUTOMATION_TESTS(ClassName, PrettyName, Prefix) \
	IMPLEMENT_COMPLEX_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter) \
	void ClassName::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const \
	{ \
		::GoN::LogicTest::GetUnrealTests(Prefix, OutBeautifiedNames, OutTestCommands); \
	} \
	bool ClassName::RunTest(const FString& Parameters) \
	{ \
		return ::GoN::LogicTest::RunUnrealTest(*this, Parameters); \
	}
