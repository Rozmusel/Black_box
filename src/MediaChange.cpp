#include <tgbot/tgbot.h>
#include "spdlog/spdlog.h"

using namespace TgBot;
namespace TgBot
{ // Fix for InputMedia types
    const std::string InputMediaPhoto::TYPE = "photo";
    const std::string InputMediaVideo::TYPE = "video";
    const std::string InputMediaAnimation::TYPE = "animation";
    const std::string InputMediaAudio::TYPE = "audio";
    const std::string InputMediaDocument::TYPE = "document";
}

InputMediaPhoto::Ptr MessageMedia(int64_t chatId, int messageId, const std::string& mediaId, const std::string& caption, InlineKeyboardMarkup::Ptr keyboard) {
    spdlog::debug("Building message media");

    InputMediaPhoto::Ptr media = std::make_shared<InputMediaPhoto>();
    media->media = mediaId;
    media->caption = caption;
    media->parseMode = "HTML";
    media->hasSpoiler = false;
    return media;
}