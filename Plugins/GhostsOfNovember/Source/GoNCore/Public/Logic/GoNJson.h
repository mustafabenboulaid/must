// Ghosts of November — minimal JSON reader/writer for game data and save payloads.
// Pure C++ (no Unreal headers): compiled inside UE modules and by Tools/LogicTests.
// Header-only on purpose: each module gets its own copy, so nothing needs exporting.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace GoN::Json
{
	enum class EType : uint8_t
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	};

	class FValue
	{
	public:
		FValue() = default;
		static FValue MakeBool(bool bInValue) { FValue V; V.Type = EType::Bool; V.bBool = bInValue; return V; }
		static FValue MakeNumber(double InValue) { FValue V; V.Type = EType::Number; V.Number = InValue; return V; }
		static FValue MakeString(std::string InValue) { FValue V; V.Type = EType::String; V.String = std::move(InValue); return V; }
		static FValue MakeArray() { FValue V; V.Type = EType::Array; return V; }
		static FValue MakeObject() { FValue V; V.Type = EType::Object; return V; }

		EType GetType() const { return Type; }
		bool IsNull() const { return Type == EType::Null; }
		bool IsBool() const { return Type == EType::Bool; }
		bool IsNumber() const { return Type == EType::Number; }
		bool IsString() const { return Type == EType::String; }
		bool IsArray() const { return Type == EType::Array; }
		bool IsObject() const { return Type == EType::Object; }

		bool AsBool(bool bFallback = false) const { return Type == EType::Bool ? bBool : bFallback; }
		double AsNumber(double Fallback = 0.0) const { return Type == EType::Number ? Number : Fallback; }
		const std::string& AsString() const { return String; }
		const std::vector<FValue>& Items() const { return Array; }
		const std::vector<std::pair<std::string, FValue>>& Members() const { return Object; }

		// Object access. Returns nullptr when missing or when this is not an object.
		const FValue* Find(const std::string& Key) const
		{
			if (Type != EType::Object)
			{
				return nullptr;
			}
			for (const auto& Member : Object)
			{
				if (Member.first == Key)
				{
					return &Member.second;
				}
			}
			return nullptr;
		}

		std::string GetString(const std::string& Key, const std::string& Fallback = std::string()) const
		{
			const FValue* V = Find(Key);
			return (V && V->IsString()) ? V->String : Fallback;
		}
		double GetNumber(const std::string& Key, double Fallback = 0.0) const
		{
			const FValue* V = Find(Key);
			return (V && V->IsNumber()) ? V->Number : Fallback;
		}
		bool GetBool(const std::string& Key, bool bFallback = false) const
		{
			const FValue* V = Find(Key);
			return (V && V->IsBool()) ? V->bBool : bFallback;
		}

		// Builders.
		FValue& Set(const std::string& Key, FValue InValue)
		{
			if (Type != EType::Object)
			{
				*this = MakeObject();
			}
			for (auto& Member : Object)
			{
				if (Member.first == Key)
				{
					Member.second = std::move(InValue);
					return Member.second;
				}
			}
			Object.emplace_back(Key, std::move(InValue));
			return Object.back().second;
		}
		FValue& Push(FValue InValue)
		{
			if (Type != EType::Array)
			{
				*this = MakeArray();
			}
			Array.push_back(std::move(InValue));
			return Array.back();
		}

	private:
		EType Type = EType::Null;
		bool bBool = false;
		double Number = 0.0;
		std::string String;
		std::vector<FValue> Array;
		std::vector<std::pair<std::string, FValue>> Object;
	};

	struct FParseError
	{
		int Line = 0;
		int Column = 0;
		std::string Message;

		std::string ToString() const
		{
			return "line " + std::to_string(Line) + ", column " + std::to_string(Column) + ": " + Message;
		}
	};

	namespace Detail
	{
		class FParser
		{
		public:
			FParser(const std::string& InText, FParseError& InError) : Text(InText), Error(InError) {}

			bool ParseDocument(FValue& Out)
			{
				// Skip a UTF-8 BOM written by some Windows editors.
				if (Text.size() >= 3 && static_cast<unsigned char>(Text[0]) == 0xEF && static_cast<unsigned char>(Text[1]) == 0xBB && static_cast<unsigned char>(Text[2]) == 0xBF)
				{
					Pos = 3;
				}
				SkipWhitespace();
				if (!ParseValue(Out, 0))
				{
					return false;
				}
				SkipWhitespace();
				if (Pos != Text.size())
				{
					return Fail("unexpected trailing characters");
				}
				return true;
			}

		private:
			static constexpr int MaxDepth = 64;

			const std::string& Text;
			FParseError& Error;
			size_t Pos = 0;

			bool Fail(const std::string& Message)
			{
				int LineNumber = 1;
				int ColumnNumber = 1;
				for (size_t Index = 0; Index < Pos && Index < Text.size(); ++Index)
				{
					if (Text[Index] == '\n')
					{
						++LineNumber;
						ColumnNumber = 1;
					}
					else
					{
						++ColumnNumber;
					}
				}
				Error.Line = LineNumber;
				Error.Column = ColumnNumber;
				Error.Message = Message;
				return false;
			}

			void SkipWhitespace()
			{
				while (Pos < Text.size())
				{
					const char C = Text[Pos];
					if (C == ' ' || C == '\t' || C == '\n' || C == '\r')
					{
						++Pos;
					}
					else if (C == '/' && Pos + 1 < Text.size() && Text[Pos + 1] == '/')
					{
						// Line comments are accepted so designers can annotate data files.
						while (Pos < Text.size() && Text[Pos] != '\n')
						{
							++Pos;
						}
					}
					else
					{
						break;
					}
				}
			}

			bool Match(const char* Literal)
			{
				size_t Index = 0;
				while (Literal[Index] != '\0')
				{
					if (Pos + Index >= Text.size() || Text[Pos + Index] != Literal[Index])
					{
						return false;
					}
					++Index;
				}
				Pos += Index;
				return true;
			}

			bool ParseValue(FValue& Out, int Depth)
			{
				if (Depth > MaxDepth)
				{
					return Fail("nesting too deep");
				}
				if (Pos >= Text.size())
				{
					return Fail("unexpected end of input");
				}
				const char C = Text[Pos];
				if (C == '{')
				{
					return ParseObject(Out, Depth);
				}
				if (C == '[')
				{
					return ParseArray(Out, Depth);
				}
				if (C == '"')
				{
					std::string Str;
					if (!ParseString(Str))
					{
						return false;
					}
					Out = FValue::MakeString(std::move(Str));
					return true;
				}
				if (C == '-' || (C >= '0' && C <= '9'))
				{
					return ParseNumber(Out);
				}
				if (Match("true"))
				{
					Out = FValue::MakeBool(true);
					return true;
				}
				if (Match("false"))
				{
					Out = FValue::MakeBool(false);
					return true;
				}
				if (Match("null"))
				{
					Out = FValue();
					return true;
				}
				return Fail(std::string("unexpected character '") + C + "'");
			}

			bool ParseObject(FValue& Out, int Depth)
			{
				Out = FValue::MakeObject();
				++Pos; // '{'
				SkipWhitespace();
				if (Pos < Text.size() && Text[Pos] == '}')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					SkipWhitespace();
					if (Pos >= Text.size() || Text[Pos] != '"')
					{
						return Fail("expected a quoted key");
					}
					std::string Key;
					if (!ParseString(Key))
					{
						return false;
					}
					if (Out.Find(Key) != nullptr)
					{
						return Fail("duplicate key \"" + Key + "\"");
					}
					SkipWhitespace();
					if (Pos >= Text.size() || Text[Pos] != ':')
					{
						return Fail("expected ':' after key \"" + Key + "\"");
					}
					++Pos;
					SkipWhitespace();
					FValue Member;
					if (!ParseValue(Member, Depth + 1))
					{
						return false;
					}
					Out.Set(Key, std::move(Member));
					SkipWhitespace();
					if (Pos < Text.size() && Text[Pos] == ',')
					{
						++Pos;
						continue;
					}
					if (Pos < Text.size() && Text[Pos] == '}')
					{
						++Pos;
						return true;
					}
					return Fail("expected ',' or '}' in object");
				}
			}

			bool ParseArray(FValue& Out, int Depth)
			{
				Out = FValue::MakeArray();
				++Pos; // '['
				SkipWhitespace();
				if (Pos < Text.size() && Text[Pos] == ']')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					SkipWhitespace();
					FValue Item;
					if (!ParseValue(Item, Depth + 1))
					{
						return false;
					}
					Out.Push(std::move(Item));
					SkipWhitespace();
					if (Pos < Text.size() && Text[Pos] == ',')
					{
						++Pos;
						continue;
					}
					if (Pos < Text.size() && Text[Pos] == ']')
					{
						++Pos;
						return true;
					}
					return Fail("expected ',' or ']' in array");
				}
			}

			static void AppendUtf8(std::string& Out, uint32_t CodePoint)
			{
				if (CodePoint < 0x80)
				{
					Out += static_cast<char>(CodePoint);
				}
				else if (CodePoint < 0x800)
				{
					Out += static_cast<char>(0xC0 | (CodePoint >> 6));
					Out += static_cast<char>(0x80 | (CodePoint & 0x3F));
				}
				else if (CodePoint < 0x10000)
				{
					Out += static_cast<char>(0xE0 | (CodePoint >> 12));
					Out += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F));
					Out += static_cast<char>(0x80 | (CodePoint & 0x3F));
				}
				else
				{
					Out += static_cast<char>(0xF0 | (CodePoint >> 18));
					Out += static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F));
					Out += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F));
					Out += static_cast<char>(0x80 | (CodePoint & 0x3F));
				}
			}

			bool ParseHex4(uint32_t& Out)
			{
				if (Pos + 4 > Text.size())
				{
					return Fail("truncated \\u escape");
				}
				Out = 0;
				for (int Index = 0; Index < 4; ++Index)
				{
					const char C = Text[Pos++];
					Out <<= 4;
					if (C >= '0' && C <= '9') { Out |= static_cast<uint32_t>(C - '0'); }
					else if (C >= 'a' && C <= 'f') { Out |= static_cast<uint32_t>(C - 'a' + 10); }
					else if (C >= 'A' && C <= 'F') { Out |= static_cast<uint32_t>(C - 'A' + 10); }
					else { return Fail("invalid hex digit in \\u escape"); }
				}
				return true;
			}

			bool ParseString(std::string& Out)
			{
				++Pos; // opening quote
				while (Pos < Text.size())
				{
					const char C = Text[Pos++];
					if (C == '"')
					{
						return true;
					}
					if (static_cast<unsigned char>(C) < 0x20)
					{
						return Fail("control character in string");
					}
					if (C != '\\')
					{
						Out += C; // UTF-8 bytes pass through untouched (Arabic text).
						continue;
					}
					if (Pos >= Text.size())
					{
						break;
					}
					const char Esc = Text[Pos++];
					switch (Esc)
					{
					case '"': Out += '"'; break;
					case '\\': Out += '\\'; break;
					case '/': Out += '/'; break;
					case 'b': Out += '\b'; break;
					case 'f': Out += '\f'; break;
					case 'n': Out += '\n'; break;
					case 'r': Out += '\r'; break;
					case 't': Out += '\t'; break;
					case 'u':
					{
						uint32_t CodePoint = 0;
						if (!ParseHex4(CodePoint))
						{
							return false;
						}
						if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF)
						{
							uint32_t Low = 0;
							if (!Match("\\u") || !ParseHex4(Low) || Low < 0xDC00 || Low > 0xDFFF)
							{
								return Fail("invalid UTF-16 surrogate pair");
							}
							CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (Low - 0xDC00);
						}
						AppendUtf8(Out, CodePoint);
						break;
					}
					default:
						return Fail(std::string("invalid escape '\\") + Esc + "'");
					}
				}
				return Fail("unterminated string");
			}

			bool ParseNumber(FValue& Out)
			{
				const size_t Start = Pos;
				if (Text[Pos] == '-')
				{
					++Pos;
				}
				bool bDigits = false;
				while (Pos < Text.size() && ((Text[Pos] >= '0' && Text[Pos] <= '9') || Text[Pos] == '.' || Text[Pos] == 'e' || Text[Pos] == 'E' || Text[Pos] == '+' || Text[Pos] == '-'))
				{
					bDigits = bDigits || (Text[Pos] >= '0' && Text[Pos] <= '9');
					++Pos;
				}
				if (!bDigits)
				{
					return Fail("invalid number");
				}
				const std::string Token = Text.substr(Start, Pos - Start);
				char* End = nullptr;
				const double Parsed = std::strtod(Token.c_str(), &End);
				if (End == nullptr || *End != '\0')
				{
					Pos = Start;
					return Fail("invalid number '" + Token + "'");
				}
				Out = FValue::MakeNumber(Parsed);
				return true;
			}
		};

		inline void WriteString(std::string& Out, const std::string& Str)
		{
			Out += '"';
			for (const char C : Str)
			{
				switch (C)
				{
				case '"': Out += "\\\""; break;
				case '\\': Out += "\\\\"; break;
				case '\n': Out += "\\n"; break;
				case '\r': Out += "\\r"; break;
				case '\t': Out += "\\t"; break;
				default:
					if (static_cast<unsigned char>(C) < 0x20)
					{
						char Buffer[8];
						std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(C)));
						Out += Buffer;
					}
					else
					{
						Out += C;
					}
				}
			}
			Out += '"';
		}

		inline void WriteNumber(std::string& Out, double Number)
		{
			char Buffer[40];
			// %.17g round-trips doubles; whole numbers print without a fraction.
			std::snprintf(Buffer, sizeof(Buffer), "%.17g", Number);
			Out += Buffer;
		}

		inline void WriteValue(std::string& Out, const FValue& Value, int Indent, int Level)
		{
			const auto NewLine = [&Out, Indent](int ToLevel)
			{
				if (Indent > 0)
				{
					Out += '\n';
					Out.append(static_cast<size_t>(Indent * ToLevel), ' ');
				}
			};
			switch (Value.GetType())
			{
			case EType::Null: Out += "null"; break;
			case EType::Bool: Out += Value.AsBool() ? "true" : "false"; break;
			case EType::Number: WriteNumber(Out, Value.AsNumber()); break;
			case EType::String: WriteString(Out, Value.AsString()); break;
			case EType::Array:
			{
				Out += '[';
				bool bFirst = true;
				for (const FValue& Item : Value.Items())
				{
					if (!bFirst) { Out += ','; }
					bFirst = false;
					NewLine(Level + 1);
					WriteValue(Out, Item, Indent, Level + 1);
				}
				if (!Value.Items().empty()) { NewLine(Level); }
				Out += ']';
				break;
			}
			case EType::Object:
			{
				Out += '{';
				bool bFirst = true;
				for (const auto& Member : Value.Members())
				{
					if (!bFirst) { Out += ','; }
					bFirst = false;
					NewLine(Level + 1);
					WriteString(Out, Member.first);
					Out += Indent > 0 ? ": " : ":";
					WriteValue(Out, Member.second, Indent, Level + 1);
				}
				if (!Value.Members().empty()) { NewLine(Level); }
				Out += '}';
				break;
			}
			}
		}
	} // namespace Detail

	inline bool Parse(const std::string& Text, FValue& Out, FParseError& OutError)
	{
		Detail::FParser Parser(Text, OutError);
		return Parser.ParseDocument(Out);
	}

	inline std::string Write(const FValue& Value, int Indent = 2)
	{
		std::string Out;
		Detail::WriteValue(Out, Value, Indent, 0);
		return Out;
	}
} // namespace GoN::Json
