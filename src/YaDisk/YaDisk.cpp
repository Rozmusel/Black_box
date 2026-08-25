#include <iostream>
#include <cstdlib>
#include "YandexDiskClient.h"
#include "YaDisk.h"
#include "spdlog/spdlog.h"

std::string uploadFileToYandexDisk(std::string& token, const std::string& localFilePath, const std::string& remoteFilePath) {
    try {
        YandexDiskClient client(token);
        client.uploadFile(remoteFilePath, localFilePath);
        spdlog::info("File uploaded successfully to Yandex Disk: {}", remoteFilePath);
        std::string publicLink;
        if (client.publish(remoteFilePath)) {
            publicLink = client.getPublicDownloadLink(remoteFilePath);
            spdlog::info("File published successfully. Public link: {}", publicLink);
        } else {
            spdlog::warn("Failed to publish the file on Yandex Disk: {}", remoteFilePath);
        }
        return publicLink; // Success
    } catch (const std::exception& e) {
        spdlog::error("Error uploading file to Yandex Disk: {}", e.what());
        return ""; // Failure
    }
}