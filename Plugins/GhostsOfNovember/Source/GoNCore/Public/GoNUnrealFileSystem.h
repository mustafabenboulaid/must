#pragma once

#include "CoreMinimal.h"
#include "GoNStringConv.h"
#include "HAL/FileManager.h"
#include "Logic/GoNFileSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// GoN::IFileSystem over Unreal's file layer (works with paks in packaged builds).
class FGoNUnrealFileSystem final : public GoN::IFileSystem
{
public:
	virtual bool ReadTextFile(const std::string& Path, std::string& OutText) const override
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *GoN::ToFString(Path), FILEREAD_Silent))
		{
			return false;
		}
		OutText.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		return true;
	}

	virtual std::vector<std::string> ListFiles(const std::string& Directory, const std::string& Extension) const override
	{
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *(GoN::ToFString(Directory) / (TEXT("*") + GoN::ToFString(Extension))), true, false);
		Found.Sort();
		std::vector<std::string> Out;
		for (const FString& Name : Found)
		{
			Out.push_back(GoN::ToStd(Name));
		}
		return Out;
	}
};
