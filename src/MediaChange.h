#pragma once
#include <tgbot/tgbot.h>

using namespace TgBot;
InputMediaPhoto::Ptr MessageMedia(int64_t chatId, int messageId, const std::string& mediaId, const std::string& caption, InlineKeyboardMarkup::Ptr keyboard);