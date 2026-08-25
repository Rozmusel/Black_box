#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include "YandexDiskClient.h"
#include "YaDisk.h"
#include "spdlog/spdlog.h"
#include "PathUtils.h"

std::string uploadFileToYandexDisk(std::string& token, const std::string& localFilePath, const std::string& remoteFilePath) {
    try {
        YandexDiskClient yandex(token);
        const auto localPath = resolveExistingPath(makePathFromString(localFilePath));
        if (!std::filesystem::exists(localPath)) {
            throw std::runtime_error("Local file does not exist: " + pathToUtf8String(localPath));
        }
        if (yandex.exists(remoteFilePath)) {
            yandex.deleteFileOrDir(remoteFilePath);
            spdlog::info("Deleted existing file on Yandex Disk: {}", remoteFilePath);
        }
        const std::string localFullPath = pathToUtf8String(std::filesystem::absolute(localPath));
        yandex.uploadFile(remoteFilePath, localFullPath);
        if (yandex.publish(remoteFilePath)) {
            return yandex.getPublicDownloadLink(remoteFilePath);
        }
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Error uploading file to Yandex Disk: {}", e.what());
        return "";
    }
}

std::string publishFolder(std::string& token, const std::string& remoteFolderPath) {
    try {
        spdlog::info("Attempting to publish folder on Yandex Disk: {}", remoteFolderPath);
        YandexDiskClient yandex(token);
        if (!yandex.exists(remoteFolderPath)) {
            yandex.createDirectory(remoteFolderPath);
            spdlog::info("Created folder on Yandex Disk: {}", remoteFolderPath);
        }
        if (yandex.publish(remoteFolderPath)) {
            return yandex.getPublicDownloadLink(remoteFolderPath);
        } else {
            spdlog::warn("Failed to publish folder: {}", remoteFolderPath);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error publishing folder on Yandex Disk: {}", e.what());
    }
    return {};
}