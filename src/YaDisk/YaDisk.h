#pragma once
#include <iostream>
#include <cstdlib>
#include <string>

std::string uploadFileToYandexDisk(std::string& token, const std::string& localFilePath, const std::string& remoteFilePath);