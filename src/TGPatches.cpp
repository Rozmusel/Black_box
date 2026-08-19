#include <tgbot/tgbot.h>
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <curl/curl.h>
#ifdef _WIN32
#include <windows.h>
#endif

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

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userdata)
{
    size_t totalSize = size * nmemb;
    if (userdata) {
        auto* response = static_cast<std::string*>(userdata);
        response->append(static_cast<char*>(contents), totalSize);
    }
    return totalSize;
}

static std::string extractJsonString(const std::string& json, const std::string& key)
{
    std::string pattern = std::string("\"") + key + "\":\"";
    auto pos = json.find(pattern);
    if (pos == std::string::npos) {
        return {};
    }
    pos += pattern.size();
    std::string value;
    value.reserve(64);
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') {
            break;
        }
        if (c == '\\' && pos < json.size()) {
            char esc = json[pos++];
            switch (esc) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(esc); break;
            }
        } else {
            value.push_back(c);
        }
    }
    return value;
}

static bool extractJsonBool(const std::string& json, const std::string& key)
{
    std::string patternTrue = std::string("\"") + key + "\":true";
    std::string patternFalse = std::string("\"") + key + "\":false";
    auto posTrue = json.find(patternTrue);
    auto posFalse = json.find(patternFalse);
    if (posTrue != std::string::npos && (posFalse == std::string::npos || posTrue < posFalse)) {
        return true;
    }
    if (posFalse != std::string::npos && (posTrue == std::string::npos || posFalse < posTrue)) {
        return false;
    }
    return false;
}

static std::filesystem::path makePathFromString(const std::string& filePath)
{
#ifdef _WIN32
    // Support UTF-8 file names on Windows.
    std::filesystem::path path = std::filesystem::u8path(filePath);
    if (std::filesystem::exists(path)) {
        return path;
    }
    try {
        std::wstring utf8Wide = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(filePath);
        std::filesystem::path utf8Path(utf8Wide);
        if (std::filesystem::exists(utf8Path)) {
            return utf8Path;
        }
    } catch (...) {
    }
    try {
        std::wstring acpWide = std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>().from_bytes(filePath);
        std::filesystem::path acpPath(acpWide);
        if (std::filesystem::exists(acpPath)) {
            return acpPath;
        }
    } catch (...) {
    }
    return path;
#else
    return std::filesystem::u8path(filePath);
#endif
}

static std::string pathToUtf8String(const std::filesystem::path& path)
{
#ifdef _WIN32
    auto wide = path.wstring();
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(wide);
#else
    return path.string();
#endif
}

static std::filesystem::path resolveExistingPath(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path)) {
        return path;
    }

    auto absPath = std::filesystem::absolute(path);
    if (std::filesystem::exists(absPath)) {
        return absPath;
    }

    auto cwd = std::filesystem::current_path();
    for (auto dir = cwd; !dir.empty(); dir = dir.parent_path()) {
        auto candidate = dir / path;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (path.has_filename()) {
            auto filenameCandidate = dir / path.filename();
            if (std::filesystem::exists(filenameCandidate)) {
                return filenameCandidate;
            }
        }
    }

    return path;
}

std::string SendDocumentViaLocalServer(const std::string& apiUrl,
                                          const std::string& token,
                                          long long chatId,
                                          const std::string& filePath,
                                          const std::string& mimeType,
                                          const std::string& caption,
                                          const std::string& parseMode)
{
    std::filesystem::path path = makePathFromString(filePath);
    path = resolveExistingPath(path);

    if (!std::filesystem::exists(path)) {
        spdlog::error("File does not exist: {}", filePath);
        try {
            spdlog::error("Current working directory: {}", pathToUtf8String(std::filesystem::current_path()));
        } catch (...) {
            spdlog::error("Current working directory: <unable to convert>");
        }
        return "";
    }

    std::filesystem::path absolutePath = std::filesystem::absolute(path);
    spdlog::info("Resolved document path: {}", pathToUtf8String(absolutePath));

    static bool curlInitialized = false;
    if (!curlInitialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            spdlog::error("curl_global_init failed");
            return std::string();
        }
        curlInitialized = true;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        spdlog::error("curl_easy_init failed");
        return std::string();
    }

    std::string requestUrl = apiUrl;
    if (!requestUrl.empty() && requestUrl.back() == '/') {
        requestUrl.pop_back();
    }
    requestUrl += "/bot" + token + "/sendDocument";

    std::string chatIdString = std::to_string(chatId);
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    curl_mime* mime = curl_mime_init(curl);
    if (!mime) {
        spdlog::error("curl_mime_init failed");
        curl_easy_cleanup(curl);
        return std::string();
    }

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "chat_id");
    curl_mime_data(part, chatIdString.c_str(), CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "document");
    std::string fullPath = pathToUtf8String(absolutePath);
    curl_mime_filedata(part, fullPath.c_str());
    if (!mimeType.empty()) {
        curl_mime_type(part, mimeType.c_str());
    }

    if (!caption.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "caption");
        curl_mime_data(part, caption.c_str(), CURL_ZERO_TERMINATED);
    }

    if (!parseMode.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "parse_mode");
        curl_mime_data(part, parseMode.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);
    if (!ok) {
        spdlog::error("curl sendDocument failed: {}", curl_easy_strerror(res));
        spdlog::error("local server response: {}", response);
    } else {
        spdlog::debug("sendDocument local server response: {}", response);
    }

    std::string fileId;
    if (ok) {
        if (!extractJsonBool(response, "ok")) {
            spdlog::error("sendDocument response marked not ok: {}", response);
        } else {
            fileId = extractJsonString(response, "file_id");
            if (fileId.empty()) {
                spdlog::error("Failed to extract file_id from response: {}", response);
            } else {
                spdlog::info("SendDocumentViaLocalServer returned file_id: {}", fileId);
            }
        }
    }

    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    return fileId;
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