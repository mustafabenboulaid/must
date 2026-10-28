// Ghosts of November — tiny registry for pure-logic tests.
// The same test cases run in two places:
//   * Tools/LogicTests (standalone executable, no Unreal),
//   * Unreal automation tests "GoN.<Module>.Logic.*" (see each module's Private/Tests).
#pragma once

#include "Logic/GoNFileSystem.h"

#include <string>
#include <vector>

#if defined(UE_BUILD_SHIPPING) && UE_BUILD_SHIPPING
	#define GON_WITH_LOGIC_TESTS 0
#else
	#define GON_WITH_LOGIC_TESTS 1
#endif

namespace GoN::LogicTest
{
	struct FContext
	{
		int Checks = 0;
		std::vector<std::string> Failures;

		void Fail(const char* File, int Line, const std::string& Message)
		{
			std::string Path(File);
			const size_t Slash = Path.find_last_of("/\\");
			if (Slash != std::string::npos)
			{
				Path = Path.substr(Slash + 1);
			}
			Failures.push_back(Path + ":" + std::to_string(Line) + ": " + Message);
		}
	};

	using FTestFunction = void (*)(FContext&);

	struct FCase
	{
		const char* Name;
		FTestFunction Function;
	};

	// Inline static: one registry per module binary (each DLL has its own copy).
	// Names start with the module tag ("Narrative.", "Blockout.", ...) so a monolithic
	// build, where registries merge, can still filter per module.
	inline std::vector<FCase>& Registry()
	{
		static std::vector<FCase> Cases;
		return Cases;
	}

	struct FRegistrar
	{
		FRegistrar(const char* Name, FTestFunction Function) { Registry().push_back(FCase{Name, Function}); }
	};

	inline std::vector<FCase> CasesWithPrefix(const std::string& Prefix)
	{
		std::vector<FCase> Out;
		for (const FCase& Case : Registry())
		{
			if (std::string(Case.Name).rfind(Prefix, 0) == 0)
			{
				Out.push_back(Case);
			}
		}
		return Out;
	}

	// Set by the runner (standalone main or the Unreal automation test) before data tests run.
	struct FDataAccess
	{
		const IFileSystem* FileSystem = nullptr;
		std::string DataDirectory; // .../Plugins/GhostsOfNovember/Data
	};
	inline FDataAccess& DataAccess()
	{
		static FDataAccess Access;
		return Access;
	}

	inline std::string Describe(const std::string& Value) { return "\"" + Value + "\""; }
	inline std::string Describe(const char* Value) { return Describe(std::string(Value ? Value : "")); }
	inline std::string Describe(bool bValue) { return bValue ? "true" : "false"; }
	inline std::string Describe(int Value) { return std::to_string(Value); }
	inline std::string Describe(long Value) { return std::to_string(Value); }
	inline std::string Describe(unsigned long Value) { return std::to_string(Value); }
	inline std::string Describe(unsigned long long Value) { return std::to_string(Value); }
	inline std::string Describe(long long Value) { return std::to_string(Value); }
	inline std::string Describe(unsigned Value) { return std::to_string(Value); }
	inline std::string Describe(double Value) { return std::to_string(Value); }
	inline std::string Describe(float Value) { return std::to_string(Value); }
} // namespace GoN::LogicTest

// Id must be unique within the module: unity builds merge .cpp files.
#define GON_LOGIC_TEST(Name, Id) \
	static void GoNLogicTest_##Id(::GoN::LogicTest::FContext& Ctx); \
	static ::GoN::LogicTest::FRegistrar GoNLogicTestRegistrar_##Id(Name, &GoNLogicTest_##Id); \
	static void GoNLogicTest_##Id(::GoN::LogicTest::FContext& Ctx)

#define GON_EXPECT(Condition) \
	do { ++Ctx.Checks; if (!(Condition)) { Ctx.Fail(__FILE__, __LINE__, "expected: " #Condition); } } while (0)

#define GON_EXPECT_EQ(Actual, Expected) \
	do { \
		++Ctx.Checks; \
		const auto& GoNActualValue = (Actual); \
		const auto& GoNExpectedValue = (Expected); \
		if (!(GoNActualValue == GoNExpectedValue)) \
		{ \
			Ctx.Fail(__FILE__, __LINE__, std::string(#Actual " == " #Expected ": got ") + ::GoN::LogicTest::Describe(GoNActualValue) + ", expected " + ::GoN::LogicTest::Describe(GoNExpectedValue)); \
		} \
	} while (0)

// Stops the test (e.g. before dereferencing something that must exist).
#define GON_REQUIRE(Condition) \
	do { ++Ctx.Checks; if (!(Condition)) { Ctx.Fail(__FILE__, __LINE__, "required: " #Condition); return; } } while (0)
