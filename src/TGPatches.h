#pragma once
#include <tgbot/tgbot.h>
#include <string>

using namespace std;
using namespace TgBot;
InputMediaPhoto::Ptr MessageMedia(int64_t chatId, int messageId, const string& mediaId, const string& caption, InlineKeyboardMarkup::Ptr keyboard);
InputFile::Ptr LoadFile(const string filePath, const string mimeType);