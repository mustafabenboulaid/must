#include "GoNLogicAutomation.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GoNBuildInfo.h"
#include "Modules/ModuleManager.h"

GON_IMPLEMENT_LOGIC_AUTOMATION_TESTS(FGoNCoreLogicTests, "GoN.Core.Logic", "Core.")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoNCoreModulesLoadedTest, "GoN.Core.ModulesLoaded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGoNCoreModulesLoadedTest::RunTest(const FString& Parameters)
{
	for (const FName& Module : GoN::Core::ExpectedModules())
	{
		TestTrue(FString::Printf(TEXT("%s is loaded"), *Module.ToString()), FModuleManager::Get().IsModuleLoaded(Module));
	}
	TestEqual(TEXT("every module registered its build time"), GoN::Core::GetModuleBuilds().Num(), GoN::Core::ExpectedModules().Num());
	return true;
}

#endif
