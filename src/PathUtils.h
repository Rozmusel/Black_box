#pragma once

#include <filesystem>
#include <string>

std::filesystem::path makePathFromString(const std::string& filePath);
std::string pathToUtf8String(const std::filesystem::path& path);
std::filesystem::path resolveExistingPath(const std::filesystem::path& path);