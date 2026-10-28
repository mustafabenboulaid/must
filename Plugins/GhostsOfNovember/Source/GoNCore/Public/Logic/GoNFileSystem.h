// Ghosts of November — file access seam for pure code.
// Unreal provides an IFileManager/FFileHelper implementation; Tools/LogicTests uses std::filesystem.
#pragma once

#include <string>
#include <vector>

namespace GoN
{
	class IFileSystem
	{
	public:
		virtual ~IFileSystem() = default;
		// Paths are UTF-8. Reads raw bytes (data files are UTF-8 JSON).
		virtual bool ReadTextFile(const std::string& Path, std::string& OutText) const = 0;
		// File names (not paths) in Directory ending with Extension (e.g. ".json"), sorted.
		virtual std::vector<std::string> ListFiles(const std::string& Directory, const std::string& Extension) const = 0;
	};

	inline std::string JoinPath(const std::string& A, const std::string& B)
	{
		if (A.empty())
		{
			return B;
		}
		const char Last = A.back();
		return (Last == '/' || Last == '\\') ? A + B : A + "/" + B;
	}
} // namespace GoN
