#include "PathUtils.h"

#include <codecvt>
#include <locale>

std::filesystem::path makePathFromString(const std::string& filePath)
{
#ifdef _WIN32
    std::filesystem::path path = std::filesystem::u8path(filePath);
    if (std::filesystem::exists(path)) {
        return path;
    }
    try {
        std::wstring utf8Wide = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(filePath);
        std::filesystem::path utf8Path(utf8Wide);
        if (std::filesystem::exists(utf8Path)) {
            return utf8Path;
        }
    } catch (...) {
    }
    try {
        std::wstring acpWide = std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>().from_bytes(filePath);
        std::filesystem::path acpPath(acpWide);
        if (std::filesystem::exists(acpPath)) {
            return acpPath;
        }
    } catch (...) {
    }
    return path;
#else
    return std::filesystem::u8path(filePath);
#endif
}

std::string pathToUtf8String(const std::filesystem::path& path)
{
#ifdef _WIN32
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(path.wstring());
#else
    return path.string();
#endif
}

std::filesystem::path resolveExistingPath(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path)) {
        return path;
    }

    const auto absolutePath = std::filesystem::absolute(path);
    if (std::filesystem::exists(absolutePath)) {
        return absolutePath;
    }

    const auto currentDirectory = std::filesystem::current_path();
    for (auto directory = currentDirectory; !directory.empty(); directory = directory.parent_path()) {
        const auto candidate = directory / path;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (path.has_filename()) {
            const auto filenameCandidate = directory / path.filename();
            if (std::filesystem::exists(filenameCandidate)) {
                return filenameCandidate;
            }
        }
    }

    return path;
}