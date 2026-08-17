#pragma once
#include <tgbot/tgbot.h>
#include <string>

using namespace std;
using namespace TgBot;
InputMediaPhoto::Ptr MessageMedia(int64_t chatId, int messageId, const string& mediaId, const string& caption, InlineKeyboardMarkup::Ptr keyboard);
InputFile::Ptr LoadFile(const string filePath, const string& mimeType);
std::string SendDocumentViaLocalServer(const std::string& apiUrl,
                                      const std::string& token,
                                      long long chatId,
                                      const std::string& filePath,
                                      const std::string& mimeType,
                                      const std::string& caption = "",
                                      const std::string& parseMode = "HTML");