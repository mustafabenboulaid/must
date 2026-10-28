#pragma once

#include "CoreMinimal.h"

#include <string>

// FString <-> UTF-8 std::string for the pure C++ core.
namespace GoN
{
	inline std::string ToStd(const FString& Text)
	{
		const FTCHARToUTF8 Converter(*Text);
		return std::string(Converter.Get(), static_cast<size_t>(Converter.Length()));
	}

	inline FString ToFString(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	inline FName ToFName(const std::string& Text)
	{
		return FName(*ToFString(Text));
	}
}
