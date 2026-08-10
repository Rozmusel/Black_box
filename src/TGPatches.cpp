#include <tgbot/tgbot.h>
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std;
using namespace TgBot;

namespace TgBot
{ // Fix for InputMedia types
    const std::string InputMediaPhoto::TYPE = "photo";
    const std::string InputMediaVideo::TYPE = "video";
    const std::string InputMediaAnimation::TYPE = "animation";
    const std::string InputMediaAudio::TYPE = "audio";
    const std::string InputMediaDocument::TYPE = "document";
}

InputMediaPhoto::Ptr MessageMedia(int64_t chatId, int messageId, const string& mediaId, const string& caption, InlineKeyboardMarkup::Ptr keyboard) {
    spdlog::debug("Building message media");

    InputMediaPhoto::Ptr media = std::make_shared<InputMediaPhoto>();
    media->media = mediaId;
    media->caption = caption;
    media->parseMode = "HTML";
    media->hasSpoiler = false;
    return media;
}

InputFile::Ptr LoadFile(const string filePath, const string mimeType) {
    InputFile::Ptr file_id;
    #ifdef _WIN32
    file_id = std::make_shared<TgBot::InputFile>();

    file_id->mimeType = "application/pdf";
    file_id->fileName = "тест.pdf";

    std::filesystem::path path = u8"тест.pdf";

    std::ifstream file(path, std::ios::binary);

    std::ostringstream ss;
    ss << file.rdbuf();

    file_id->data = ss.str();
    #else
    file_id = InputFile::fromFile(filePath, mimeType);
    #endif
    return file_id;
}