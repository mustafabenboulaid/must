// Pure-logic tests for GoNCore utilities (JSON, dates). Run by Tools/LogicTests and "GoN.Core.Logic".
#include "Logic/GoNLogicTest.h"

#if GON_WITH_LOGIC_TESTS

#include "Logic/GoNDate.h"
#include "Logic/GoNJson.h"

GON_LOGIC_TEST("Core.Json.ParsesNestedDocument", Core_Json_Nested)
{
	GoN::Json::FValue Root;
	GoN::Json::FParseError Error;
	const std::string Text = "{\n  // comment\n  \"a\": [1, 2.5, -3e2],\n  \"b\": {\"c\": true, \"d\": null},\n  \"s\": \"x\\ny\\u0041\"\n}";
	GON_REQUIRE(GoN::Json::Parse(Text, Root, Error));
	GON_EXPECT_EQ(Root.Find("a")->Items().size(), size_t(3));
	GON_EXPECT_EQ(Root.Find("a")->Items()[2].AsNumber(), -300.0);
	GON_EXPECT(Root.Find("b")->GetBool("c"));
	GON_EXPECT(Root.Find("b")->Find("d")->IsNull());
	GON_EXPECT_EQ(Root.GetString("s"), std::string("x\nyA"));
}

GON_LOGIC_TEST("Core.Json.KeepsArabicUtf8", Core_Json_Arabic)
{
	// "حوار درامي متخيّل" as UTF-8 bytes, and the same text through \u escapes.
	const std::string Arabic = "\xD8\xAD\xD9\x88\xD8\xA7\xD8\xB1";
	GoN::Json::FValue Root;
	GoN::Json::FParseError Error;
	GON_REQUIRE(GoN::Json::Parse("{\"t\":\"" + Arabic + "\",\"e\":\"\\u062d\\u0648\\u0627\\u0631\"}", Root, Error));
	GON_EXPECT_EQ(Root.GetString("t"), Arabic);
	GON_EXPECT_EQ(Root.GetString("e"), Arabic);

	GoN::Json::FValue Reparsed;
	GON_REQUIRE(GoN::Json::Parse(GoN::Json::Write(Root), Reparsed, Error));
	GON_EXPECT_EQ(Reparsed.GetString("t"), Arabic);
}

GON_LOGIC_TEST("Core.Json.ReportsErrorsWithLine", Core_Json_Errors)
{
	GoN::Json::FValue Root;
	GoN::Json::FParseError Error;
	GON_EXPECT(!GoN::Json::Parse("{\n\"a\": 1,\n\"b\" 2\n}", Root, Error));
	GON_EXPECT_EQ(Error.Line, 3);
	GON_EXPECT(!GoN::Json::Parse("{\"a\":1,\"a\":2}", Root, Error));
	GON_EXPECT(Error.Message.find("duplicate") != std::string::npos);
	GON_EXPECT(!GoN::Json::Parse("[1,2", Root, Error));
	GON_EXPECT(!GoN::Json::Parse("\"unterminated", Root, Error));
	GON_EXPECT(!GoN::Json::Parse("{} x", Root, Error));
}

GON_LOGIC_TEST("Core.Json.WriteRoundTrip", Core_Json_RoundTrip)
{
	GoN::Json::FValue Root = GoN::Json::FValue::MakeObject();
	Root.Set("n", GoN::Json::FValue::MakeNumber(0.1));
	Root.Set("q", GoN::Json::FValue::MakeString("say \"hi\"\t\\"));
	GoN::Json::FValue List = GoN::Json::FValue::MakeArray();
	List.Push(GoN::Json::FValue::MakeBool(false));
	Root.Set("l", List);
	for (const int Indent : {0, 2})
	{
		GoN::Json::FValue Back;
		GoN::Json::FParseError Error;
		GON_REQUIRE(GoN::Json::Parse(GoN::Json::Write(Root, Indent), Back, Error));
		GON_EXPECT_EQ(Back.GetNumber("n"), 0.1);
		GON_EXPECT_EQ(Back.GetString("q"), std::string("say \"hi\"\t\\"));
		GON_EXPECT(Back.Find("l")->Items()[0].IsBool());
	}
}

GON_LOGIC_TEST("Core.Date.ParseAndValidate", Core_Date_Parse)
{
	GoN::FDate Date;
	GON_EXPECT(GoN::FDate::Parse("1954-11-01", Date));
	GON_EXPECT_EQ(Date.ToString(), std::string("1954-11-01"));
	GON_EXPECT(GoN::FDate::Parse("1956-02-29", Date)); // leap year
	GON_EXPECT(!GoN::FDate::Parse("1955-02-29", Date));
	GON_EXPECT(!GoN::FDate::Parse("1954-13-01", Date));
	GON_EXPECT(!GoN::FDate::Parse("1954-1-01", Date));
	GON_EXPECT(!GoN::FDate::Parse("", Date));
}

GON_LOGIC_TEST("Core.Date.OrderingAndRanges", Core_Date_Order)
{
	GoN::FDate A;
	GoN::FDate B;
	GoN::FDate::Parse("1954-10-31", A);
	GoN::FDate::Parse("1954-11-01", B);
	GON_EXPECT(A < B);
	GON_EXPECT_EQ(B.ToDayNumber() - A.ToDayNumber(), 1L);
	GoN::FDate Evian;
	GoN::FDate Independence;
	GoN::FDate::Parse("1962-03-18", Evian);
	GoN::FDate::Parse("1962-07-05", Independence);
	GON_EXPECT_EQ(Independence.ToDayNumber() - Evian.ToDayNumber(), 109L);

	const GoN::FDateRange Range{A, B};
	GON_EXPECT(Range.Contains(A) && Range.Contains(B));
	GON_EXPECT(Range.Overlaps(GoN::FDateRange{B, Independence}));
	GON_EXPECT(!Range.Overlaps(GoN::FDateRange{Evian, Independence}));
}

#endif // GON_WITH_LOGIC_TESTS
