#pragma once
#include <tgbot/tgbot.h>
#include <string>
#include "PathUtils.h"

using namespace std;
using namespace TgBot;
InputMediaPhoto::Ptr MessageMedia(const string& mediaId, const string& caption);
InputFile::Ptr LoadFile(const string filePath, const string& mimeType);
std::string SendDocumentViaLocalServer(const std::string& apiUrl,
                                      const std::string& token,
                                      long long chatId,
                                      const std::string& filePath,
                                      const std::string& mimeType,
                                      const std::string& caption = "",
                                      const std::string& parseMode = "HTML");