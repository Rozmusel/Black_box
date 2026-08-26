#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>
#include <set>

#include <tgbot/tgbot.h>
#include <tgbot/net/CurlHttpClient.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "databasics/database.h"
#include "TGkeyboards/TGkeyboards.h"
#include "TGPatches.h"
#include "logging.h"
#include "pdf/pdf.h"
#include "YaDisk/YaDisk.h"

using namespace std;
using namespace TgBot;

namespace fs = std::filesystem;

string getRequiredEnv(const char* name)
{
    const char* value = std::getenv(name);

    if (value == nullptr || *value == '\0') {
        throw runtime_error(
            string("Environment variable is not set: ") + name
        );
    }

    return value;
}

void deleteMessageIfExists(Bot &bot, int64_t chatId, int32_t messageId)
{
    if (messageId == 0)
        return;

    try
    {
        bot.getApi().deleteMessage(chatId, messageId);
    }
    catch (const exception &error)
    {
        spdlog::warn("Could not delete message {} in chat {}: {}", messageId, chatId, error.what());
    }
}

void notifyNewFileSubscribers(Bot &bot, Database &db, const string& token,
                              const string& subjectName, int8_t type,
                              const string& groupName, const string& filePath)
{
    for (const int64_t chatId : getSubjectSubscribers(db, subjectName, groupName, type)) {
        try {
            SendDocumentViaLocalServer("http://127.0.0.1:8081", token, chatId,
                                       filePath, "application/pdf",
                                       "Новый файл: " + subjectName, "HTML");
        }
        catch (const exception& error) {
            spdlog::warn("Could not notify subscriber {} about file '{}': {}", chatId, filePath, error.what());
        }
    }
}

void notifyUpdatedFileRecipients(Bot &bot, Database &db, const string& token,
                                 const string& subjectName, int8_t type,
                                 const string& groupName, const string& filePath)
{
    for (const int64_t chatId : getFileNotificationRecipients(db, subjectName, groupName, type)) {
        try {
            SendDocumentViaLocalServer("http://127.0.0.1:8081", token, chatId,
                                       filePath, "application/pdf",
                                       "Файл изменён: " + subjectName, "HTML");
        }
        catch (const exception& error) {
            spdlog::warn("Could not notify user {} about updated file '{}': {}", chatId, filePath, error.what());
        }
    }
}

enum UserStatus
{
    REGISTRATION,
    START,
    LECTURE,
    SEMINAR,
    SETTINGS,
    ADMINISTRATION,
    FEEDBACK
};
enum UserAccess
{
    DEFAULT,
    PREMIUM,
    ADMIN
};

#define PRICE 30000 // 300.00 RUB

void handleConsoleCommand(Bot &bot, const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help")
    {
        std::cout << "Available commands:\n";
        std::cout << "  send \"user_id\" \"message\"\n";
        return;
    }

    if (cmd == "send")
    {
        std::string userIdStr;
        iss >> userIdStr;
        if (userIdStr.empty())
        {
            std::cout << "Usage: send \"user_id\" \"message\"\n";
            return;
        }

        std::string message;
        std::getline(iss, message);
        if (!message.empty() && message[0] == ' ')
            message.erase(0, 1);

        if (message.size() >= 2 && message.front() == '"' && message.back() == '"')
        {
            message = message.substr(1, message.size() - 2);
        }

        const long long userId = std::stoll(userIdStr);
        auto msg = bot.getApi().sendMessage(userId, message);
        spdlog::info("Sent message to user {}: {}", msg->chat->id, msg->text);
        spdlog::info("Username: {} First Name: {} Last Name: {}", msg->chat->username, msg->chat->firstName, msg->chat->lastName);
        std::cout << "Message sent to user " << userId << "\n";
        return;
    }

    if (!cmd.empty())
    {
        std::cout << "Unknown command. Type help\n";
    }
}

int main()
{
    auto user_logs = spdlog::daily_logger_mt("user_logs", "logs/users/users.txt", 0, 0); // Файл для записи сообщений от пользователей с ежедневной ротацией в 00:00
    user_logs->set_pattern("[%Y-%m-%d %H:%M:%S] %v");                                    // Шаблон для записей в этом файле

    multisink_logger("console", "logs/console.txt"); // Файл логов по умолчанию дублируется в терминал

    auto feedback = spdlog::basic_logger_mt("feedback", "logs/feedback.txt");
    feedback->set_pattern("[%Y-%m-%d %H:%M:%S] %v");

    Database bd("database/bot.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    initDB(bd);

    string token;
    string YaToken;
    string providerToken;

    try
    {
        token = getRequiredEnv("TELEGRAM_BOT_TOKEN");
        YaToken = getRequiredEnv("YADISK_TOKEN");
        providerToken = getRequiredEnv("TELEGRAM_PAYMENT_PROVIDER_TOKEN");
    }
    catch (const exception& error)
    {
        cerr << "Configuration error: " << error.what() << endl;
        spdlog::critical("Configuration error: {}", error.what());
        spdlog::default_logger()->flush();
        return EXIT_FAILURE;
    }

    CurlHttpClient curlHttpClient;

    Bot bot(
        token,
        curlHttpClient,
        "http://127.0.0.1:8081");
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    thread consoleThread([&bot]()
                         {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty())
                continue;
            try
            {
                handleConsoleCommand(bot, line);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Command error: " << e.what() << "\n";
            }
        } });
    consoleThread.detach();

    thread delayedFiles([&bot, &token, &user_logs]()
                        {
        Database workerDb(
            "database/bot.db",
            SQLite::OPEN_READWRITE
        );

        while (true) {
            try {
                const vector<pair<int64_t, string>> files = getDelayedFiles(workerDb);

                for (const auto& file : files) {
                    spdlog::debug("Sending delayed file to user {}: {}", file.first, file.second);
                    const string sentFileId = SendDocumentViaLocalServer(
                        "http://127.0.0.1:8081",
                        token,
                        file.first,
                        file.second,
                        "application/pdf",
                        "",
                        "HTML"
                    );

                    if (!sentFileId.empty()) {
                        const string sourcePath = file.second.starts_with("temp/")
                            ? file.second.substr(5)
                            : file.second;
                        recordDownloadedFileByPath(workerDb, file.first, sourcePath);
                        deleteDelayedFile(workerDb, file.first, file.second);
                        fs::remove(fs::u8path(file.second));
                        spdlog::info("Sent delayed file to user {}: {}", file.first, file.second);
                        user_logs->info("Sent delayed file to user {}: {}", file.first, file.second);
                    }
                }
            }
            catch (const exception& error)
            {
                spdlog::error(
                    "Error in delayed files thread: {}",
                    error.what()
                );
            }

            this_thread::sleep_for(chrono::seconds(60));
        } });

    bot.getEvents().onAnyMessage([&bot, &user_logs, &feedback, &bd, &token](Message::Ptr message)
                                 {
        try {
            user_logs->info("{}: {}", message->from->username.c_str(), message->text.c_str());
            spdlog::info("{}: {}", message->from->username, message->text.c_str());
            string Id = checkId(bd, message->from->id);
                if (Id.empty()) {
                    spdlog::info("New user: {}", message->from->username);
                    string username = message->from->username.empty() ? message->from->firstName + " " + message->from->lastName : "@" +message->from->username;
                    addUser(bd, message->from->id, username);
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Далее"});
                bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase(bd , "logo"),
                "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции.",
                nullptr, keyboard, "Markdown"
                );
                string u = getUsername(bd, message->from->id);
                if (u == "@mss_gule") {
                    setUserAccess(bd, message->from->id, ADMIN);
                }
                if (u == "@cent1011") {
                    setUserAccess(bd, message->from->id, PREMIUM);
                }
                return;
                }
            if (UserState(bd, message->chat->id) == FEEDBACK) {
                if (message->text == "/start"){
                    setUserState(bd, message->chat->id, START);
                    return;
                }
                if (message->text.empty() == false) {
                    feedback->info("{}: {}", message->from->username.c_str(), message->text.c_str());
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Назад"});
                    auto reaction = make_shared<ReactionTypeEmoji>();
                    reaction->emoji = "✅";
                    bot.getApi().setMessageReaction(message->chat->id, message->messageId, {reaction});
                    bot.getApi().sendMessage(message->chat->id, "Спасибо за ваш отзыв!");
                    return;
                }
            }
            if (message->document != nullptr) {
                const auto file = bot.getApi().getFile(message->document->fileId);
                user_logs->info("{} sended file: {}", message->chat->username, message->document->fileName);
                spdlog::info("{} file saved to {}", message->chat->username, file->filePath);

                if (UserAccess(bd, message->from->id) == ADMIN && message->document->mimeType == "application/pdf") {
                    subject sub(bd, message->document->fileName, getGroupName(bd, message->chat->id));
                    bool upd = !message->caption.empty() && message->caption.find_first_not_of("0123456789") == std::string::npos;
                    if (upd == true) {
                        spdlog::info("Updating file: {} count {}", message->document->fileName, message->caption);
                        sub.count = stoi(message->caption);
                    }
                    string newFilePath = "files/" + getGroupName(bd, message->chat->id) + "/" + sub.name + " " + (sub.type == 0 ? "Лекция" : "Семинар") + " " + to_string(sub.count) + ".pdf";
                    const string groupName = getGroupName(bd, message->chat->id);
                    if (sub.count == 1) addGroupSubject(bd, sub.name, sub.type, getGroupName(bd, message->chat->id));
                    if (!fs::exists(fs::u8path("files/" + getGroupName(bd, message->chat->id)))) {
                        fs::create_directory(fs::u8path("files/" + getGroupName(bd, message->chat->id)));
                    }
                    if (fs::exists(fs::u8path(newFilePath))) {
                        spdlog::info("File already exists, removing: {}", newFilePath);
                        fs::remove(fs::u8path(newFilePath));
                    }
                    fs::rename(fs::u8path(file->filePath), fs::u8path(newFilePath));
                    string fileId = SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                                                  message->chat->id,
                                                                  newFilePath,
                                                                  "application/pdf",
                                                                  "", "HTML");
                    if (fileId.empty()) {
                        bot.getApi().sendMessage(message->chat->id, "Не удалось вернуть файл через локальный сервер");
                    }
                    if (upd == false) {
                        sub.insert(bd, newFilePath, fileId, groupName);
                        if (!fileId.empty()) {
                            notifyNewFileSubscribers(bot, bd, token, sub.name, sub.type, groupName, newFilePath);
                        }
                        bot.getApi().sendMessage(message->chat->id, "Файл успешно добавлен в базу данных");
                    } else {
                        sub.update(bd, fileId, newFilePath, groupName);
                        if (!fileId.empty()) {
                            notifyUpdatedFileRecipients(bot, bd, token, sub.name, sub.type, groupName, newFilePath);
                        }
                        bot.getApi().sendMessage(message->chat->id, "Файл успешно обновлён в базе данных");
                    }
                } else {
                    bot.getApi().sendMessage(message->chat->id, "Эмм... ладно, спасибо, я посмотрю на досуге");
                }
            }
            if (message->photo.empty() == false) {
                string fileId;
                while (fileId.empty()) {
                    fileId = bot.getApi().getFile(message->photo.back()->fileId)->filePath;
                    user_logs->info("{} sended photo: {}", message->chat->username, fileId);
                    spdlog::info("File saved to {}", fileId);
                }
                bot.getApi().sendMessage(message->chat->id, "Эмм... ладно, спасибо, я посмотрю на досуге");
            }
            
            
        }
        catch (exception &e) {
            bot.getApi().sendMessage(message->chat->id, "Возникла ошибка, попробуйте позже");
            spdlog::error(e.what());
        } });

    bot.getEvents().onCallbackQuery([&bot, &user_logs, &bd, &token, &YaToken, &providerToken](CallbackQuery::Ptr query) {
        try {
        bool callbackAnswered = false;
        user_logs->info("{}: {}", query->from->username.c_str(), query->data.c_str());
        spdlog::info("{}: {}", query->from->username.c_str(), query->data.c_str());
        if (checkId(bd, query->from->id) == "") {
            bot.getApi().sendMessage(query->from->id, "Предыдущие команды не работают, введите /start");
            return;
        }
        if (query->message->messageId != getLastMenuMessageId(bd, query->from->id) && UserState(bd, query->from->id) > REGISTRATION) {
            bot.getApi().answerCallbackQuery(query->id, "Это меню устарело, используйте актуальное");
            deleteMessageIfExists(bot, query->message->chat->id, query->message->messageId);
            spdlog::info("Deleted outdated menu message for user {}: {}", query->from->username, query->message->messageId);
            user_logs->info("Deleted outdated menu message for user {}", query->from->username);
            return;
        }
        if (query->data == "Назад" && UserState(bd, query->from->id) > START) setUserState(bd, query->from->id, START);
        switch (UserState(bd, query->from->id)) { // Статус пользователя (используется для дерева диалогов), сейчас стоит заглушка
            case REGISTRATION:
                if (query->data == "Далее") {
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId,
                        "В новой версии бота появились:\n"
                        "• Удаление вотермарок\n"
                        "• Быстрая загрузка\n"
                        "• Возможность выбора интервала лекций\n"
                        "• Подписки\n"
                        "• Уведомления об изменённых материалах\n"
                        "• Альтернативный способ загрузки"
                    );
                    bot.getApi().sendDocument(query->message->chat->id, getMediaIdFromDatabase(bd , "degree"));
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Принять", "Отклонить"});
                    bot.getApi().sendAnimation(query->message->chat->id, getMediaIdFromDatabase(bd , "agreement"), 0, 0, 0, "",
                        "Перед началом работы с ботом, примите пользовательское соглашение", nullptr, keyboard
                    );
                }
                if (query->data == "Назад") {
                    deleteMessageIfExists(bot, query->message->chat->id, query->message->messageId);
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Принять", "Отклонить"});
                    bot.getApi().sendAnimation(query->message->chat->id, getMediaIdFromDatabase(bd , "agreement2"), 0, 0, 0, "",
                        "Я не пущу вас, пока не примите уже наконец пользовательское соглашение", nullptr, keyboard
                    );
                }
                if (query->data == "Принять") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Успех"});
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "success"), "Вы приняли пользовательское соглашение"
                );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                    //setUserStatus(query->from->id, 1)  // ставим статус
                    bot.getApi().answerCallbackQuery(query->id, "[[A Great Deal]]");
                }
                if (query->data == "Отклонить") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Назад"});
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "refuse"), "ƪ(˘⌣˘)ʃ"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                
                if (query->data == "Успех") {
                    vector<string> buttons = getGroups(bd);
                    buttons.push_back("Другая");
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "branch"), "В новой версии открываются филиалы.\n"
                        "Теперь вы можете выбрать свою группу, чтобы иметь материалы по вашей учебной программе"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Другая") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboardExtended({{"Назад", "Успех"}});
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "cowork"), "Если вашей группы нет, вы можете присоединиться к одной из существующих, "
                        "учебный план которой наиболее близок к вашему.\n"
                        "Вы можете открыть филиал своей группы если найдётся желающий покрывать разницу "
                        "в учебных планах своими лекциями и семинарами\n\n"
                        "К этой работе предъявляются следующие требования:\n"
                        "• Материал должен быть выполнен на электронном носителе\n"
                        "• Пропускать лекции и семинары нельзя (в случае болезни стоит переписать у тех кто посетил)\n"
                        "• Выгружать материалы в хронологическом порядке и в формате .pdf по одной лекции по мере их появления\n\n"
                        "Владельцы филиалов имеют бесплатный доступ к платным функциям бота\n"
                        "Всех желающих готов ввести в курс дела, обращайтесь ко мне @Rozmusel\n"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data.starts_with("СМ7-5")) {
                    setUserGroup(bd, query->from->id, query->data.c_str());  // добавляет пользователю выбнанную группу
                    setUserState(bd, query->from->id, 1);  // ставим статус общего меню с файлами
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "freedom"), "Поздравляю, вы получили доступ ко всем материалам.\n"
                        "Чтобы попасть в основное меню, используйте /start\n"
                        "Советую использовать кнопку Menu для вызова команды /start и использовать её всякий раз, когда нужно опустить диалоговое окно вниз"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId);
                    bot.getApi().answerCallbackQuery(query->id, "Воздух потрескивает от свободы");
                }
            break;
            case START:
                if (query->data == "Лекции") {
                    setUserState(bd, query->from->id, LECTURE);
                    vector<pair<string,string>> buttons = getSubjectsByGroup(bd, getGroupName(bd, query->from->id), 0);
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите лекцию", "", keyboard);
                }
                if (query->data == "Семинары") {
                    setUserState(bd, query->from->id, SEMINAR);
                    vector<pair<string,string>> buttons = getSubjectsByGroup(bd, getGroupName(bd, query->from->id), 1);
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите семинар", "", keyboard);
                }
                if (query->data == "Настройки") {
                    setUserState(bd, query->from->id, SETTINGS);
                    vector<string> buttons;
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        buttons.push_back(UserAlternativeDownload(bd, query->from->id) ? "✅ Альтернативная загрузка" : "⬜ Альтернативная загрузка");
                        buttons.push_back(UserSubscription(bd, query->from->id) ? "✅ Подписка" : "⬜ Подписка");
                        buttons.push_back(UserNotification(bd, query->from->id) ? "✅ Уведомления" : "⬜ Уведомления");
                    }
                    buttons.push_back("Филиалы");
                    buttons.push_back("Отзывы");
                    string text = "Пользователь: @" + query->from->username + "\n";
                    buttons.push_back("Премиум");
                    buttons.push_back("Назад");
                    if (getUserFolder(bd, query->from->id).empty() == false) {
                        text += "\nПапка c вашими загруженными файлами: " + getUserFolder(bd, query->from->id);
                    }
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "peter"), text
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Администраторская") {
                    setUserState(bd, query->from->id, ADMINISTRATION);
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Удалить последний файл", "Изменить файл", "Логи", "Назад"});
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "admin"), "Панель управления ботом"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Назад") {
                    vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
                    if (UserAccess(bd, query->message->chat->id) == ADMIN) buttons.push_back("Администраторская");    // isUserAdmin(message->from->id)
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(
                        getMediaIdFromDatabase(bd , "start"), "Выберите опцию"
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
            break;
            case SEMINAR:
            case LECTURE:
            {
            const bool isLecture = UserState(bd, query->from->id) == LECTURE;
            const int8_t fileType = isLecture ? 0 : 1;
            const string subjectType = isLecture ? "лекция" : "семинар";
            const string subjectTypePlural = isLecture ? "лекции" : "семинары";
            const string subjectTypePluralTitle = isLecture ? "Лекции" : "Семинары";
            if (query->data == "subscribe" || query->data == "unsubscribe") {
                if (UserAccess(bd, query->from->id) < PREMIUM ||
                    (query->data == "subscribe" && !UserSubscription(bd, query->from->id))) {
                    bot.getApi().answerCallbackQuery(query->id, "Сначала включите подписку в настройках");
                    return;
                }
                const size_t firstArrow = query->message->caption.find("->");
                const size_t lastArrow = query->message->caption.rfind("->");
                if (firstArrow == string::npos || firstArrow == lastArrow)
                    throw runtime_error("Invalid subject caption");
                const string subjectName = query->message->caption.substr(firstArrow + 2, lastArrow - firstArrow - 2);
                const string groupName = query->message->caption.substr(lastArrow + 2);
                const bool enabled = UserSubjectSubscription(bd, query->from->id, subjectName, groupName, fileType) == 1;
                setSubjectSubscription(bd, query->from->id, subjectName, groupName, fileType, !enabled);
                bot.getApi().answerCallbackQuery(query->id, !enabled ? "Подписка оформлена" : "Подписка отменена");
                return;
            }
            if (query->data._Starts_with("list:")){
                string subject_name = query->data.substr(5, query->data.find(':', 5) - 5);
                string group_name = query->data.substr(query->data.rfind(':') + 1);
                int8_t count = getFileCount(bd, subject_name, fileType, group_name);
                vector<pair<string,string>> buttons;
                if (UserAccess(bd, query->message->chat->id) >= PREMIUM)
                    buttons.push_back({"⬜ Интервал " + subjectTypePlural, "interval_mode"});
                for (int8_t i = 1; i <= count; i++) {
                    buttons.push_back({subject_name + " " + subjectType + " " + to_string(i), "download:" + to_string(i)});
                }
                if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                    buttons.push_back({"Все " + subjectTypePlural, "all"});
                }
                if (UserAccess(bd, query->message->chat->id) >= PREMIUM && UserSubscription(bd, query->from->id)) {
                    buttons.push_back({
                        UserSubjectSubscription(bd, query->from->id, subject_name, group_name, fileType)
                            ? "Отписаться"
                            : "Подписаться",
                        UserSubjectSubscription(bd, query->from->id, subject_name, group_name, fileType)
                            ? "unsubscribe"
                            : "subscribe"});
                }
                buttons.push_back({"Назад", "Назад"});
                InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                string text = subjectType + "->" + subject_name + "->" + group_name;
                bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
            }
            if (query->data._Starts_with("download:")) {
                string subject_type = query->message->caption.substr(0, query->message->caption.find("->"));
                string subject_name = query->message->caption.substr(query->message->caption.find("->") + 2, query->message->caption.rfind("->") - query->message->caption.find("->") - 2);
                string group_name = query->message->caption.substr(query->message->caption.rfind("->") + 2);
                int8_t count = stoi(query->data.substr(9));
                if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                    if (UserAlternativeDownload(bd, query->from->id)) {
                        string filePath = getFilePath(bd, subject_name, fileType, count, group_name);
                        string dir = getUsername(bd, query->from->id).empty() ? to_string(query->from->id) : getUsername(bd, query->from->id);
                        string YaPath = "app:/" + dir + "/" + group_name + " " + subject_name + " " + subject_type + " " + to_string(count) + ".pdf";
                        string publicLink = uploadFileToYandexDisk(YaToken, filePath, YaPath);
                        bot.getApi().sendMessage(query->message->chat->id, "Файл доступен по ссылке: " + publicLink);
                        if (!publicLink.empty())
                            recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                    } else {
                        string fileId = getFileId(bd, subject_name, fileType, count, group_name);
                        auto sentMessage = bot.getApi().sendDocument(query->message->chat->id, fileId);
                        if (sentMessage->document != nullptr)
                            recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                    }
                } else {
                    bot.getApi().sendMessage(query->message->chat->id, "Файл будет отправлен через 5 минут");
                    const int64_t scheduledAt = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count() + 5;
                    string path = getFilePath(bd, subject_name, fileType, count, group_name);
                    string outputPath = "temp/" + path;
                    pdfAddWatermark(path, outputPath);
                    setDelayedFile(bd, query->message->chat->id, outputPath, scheduledAt);
                }
            }
            if (query->data == "all") {
                if (UserAccess(bd, query->message->chat->id) < PREMIUM) {
                    bot.getApi().sendMessage(query->message->chat->id, "Функция не доступна");
                    return;
                }
                string subject_type = query->message->caption.substr(0, query->message->caption.find("->"));
                string subject_name = query->message->caption.substr(query->message->caption.find("->") + 2, query->message->caption.rfind("->") - query->message->caption.find("->") - 2);
                string group_name = query->message->caption.substr(query->message->caption.rfind("->") + 2);
                int8_t count = getFileCount(bd, subject_name, fileType, group_name);
                if (count <= 1) {
                    bot.getApi().sendMessage(query->message->chat->id, "Недостаточно " + subjectTypePlural + " для объединения");
                    return;
                }
                vector<string> files;
                for (int8_t i = 1; i <= count; i++) {
                    files.push_back(getFilePath(bd, subject_name, fileType, i, group_name));
                }
                string filePath = "temp/" + subject_name + " " + subjectTypePluralTitle + " 1-" + std::to_string(count) + ".pdf";
                pdfMerge(files, filePath);
                if (UserAlternativeDownload(bd, query->from->id)) {
                    string dir = getUsername(bd, query->from->id).empty() ? to_string(query->from->id) : getUsername(bd, query->from->id);
                    string YaPath = "app:/" + dir + "/" + group_name + " " + subject_name + " " + subject_type + " 1-" + std::to_string(count) + ".pdf";
                    string publicLink = uploadFileToYandexDisk(YaToken, filePath, YaPath);
                    bot.getApi().sendMessage(query->message->chat->id, "Файл доступен по ссылке: " + publicLink);
                    if (!publicLink.empty()) {
                        recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                        fs::remove(fs::u8path(filePath));
                    } else {
                        bot.getApi().sendMessage(query->message->chat->id, "Не удалось загрузить файл на Яндекс.Диск");
                        spdlog::error("Failed to upload merged file to Yandex Disk: {}", filePath);
                    }
                } else {
                string response = SendDocumentViaLocalServer(
                        "http://127.0.0.1:8081",
                        token,
                        query->message->chat->id,
                        filePath,
                        "application/pdf",
                        "",
                        "HTML"
                    );
                if (!response.empty()) {
                    recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                    fs::remove(fs::u8path(filePath));
                } else {
                    bot.getApi().sendMessage(query->message->chat->id, "Не удалось отправить файл");
                    spdlog::error("Failed to send merged file: {}", filePath);
                }
            }
            }
            if (query->data == "interval_mode") {
                if (UserAccess(bd, query->message->chat->id) < PREMIUM) {
                    bot.getApi().sendMessage(query->message->chat->id, "Функция не доступна");
                    return;
                }
                string subject_type = query->message->caption.substr(0, query->message->caption.find("->"));
                string subject_name = query->message->caption.substr(query->message->caption.find("->") + 2, query->message->caption.rfind("->") - query->message->caption.find("->") - 2);
                string group_name = query->message->caption.substr(query->message->caption.rfind("->") + 2);
                int8_t count = getFileCount(bd, subject_name, fileType, group_name);
                vector<pair<string,string>> buttons;
                string callback_data = "list:" + subject_name + ":" + to_string(fileType) + ":" + group_name;
                buttons.push_back({"✅ Интервал " + subjectTypePlural, callback_data});
                for (int8_t i = 1; i <= count; i++) {
                    buttons.push_back({subject_name + " " + subject_type + " " + to_string(i), "interval:" + to_string(i)});
                }
                    buttons.push_back({"Все " + subjectTypePlural, "all"});
                buttons.push_back({"Назад", "Назад"});
                InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                string text = subjectType + "->" + subject_name + "->" + group_name;
                bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
            }
            if (query->data.starts_with("interval:")) {
                if (UserAccess(bd, query->message->chat->id) < PREMIUM) {
                    bot.getApi().sendMessage(query->message->chat->id, "Функция не доступна");
                    return;
                }
                string subject_type = query->message->caption.substr(0, query->message->caption.find("->"));
                string subject_name = query->message->caption.substr(query->message->caption.find("->") + 2, query->message->caption.rfind("->") - query->message->caption.find("->") - 2);
                string group_name = query->message->caption.substr(query->message->caption.rfind("->") + 2);
                int8_t count = getFileCount(bd, subject_name, fileType, group_name);
                int8_t first, second;
                vector<pair<string,string>> buttons;
                string callback_data = "list:" + subject_name + ":" + to_string(fileType) + ":" + group_name;
                buttons.push_back({"✅ Интервал " + subjectTypePlural, callback_data});
                if (query->data.find("->") == string::npos) {
                    int8_t c = stoi(query->data.substr(query->data.find(":") + 1));
                    for (int8_t i = 1; i <= count; i++) {
                        if (i < c) {
                            buttons.push_back({"❌ " + subject_name + " " + subject_type + " " + to_string(i) + " ❌", "interval:" + to_string(i) });
                        } else if (i == c) {
                            buttons.push_back({"❌ " + subject_name + " " + subject_type + " " + to_string(i) + " ❌", "interval:" + to_string(c) + "->" + to_string(i)});
                        } else {
                            buttons.push_back({subject_name + " " + subject_type + " " + to_string(i), "interval:" + to_string(c) + "->" + to_string(i)});
                        }
                    }
                    buttons.push_back({"Все " + subjectTypePlural, "all"});
                buttons.push_back({"Назад", "Назад"});
                InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                string text = subjectType + "->" + subject_name + "->" + group_name;
                bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                return;
                }
                first = stoi(query->data.substr(query->data.find(":") + 1, query->data.find("->") - query->data.find(":")));
                second = stoi(query->data.substr(query->data.find("->") + 2));
                if (first == second) {
                    bot.getApi().answerCallbackQuery(query->id, "Вы выбрали один и тот же файл");
                    return;
                }
                vector<string> files;
                for (int8_t i = first; i <= second; i++) {
                    files.push_back(getFilePath(bd, subject_name, fileType, i, group_name));
                }
                string filePath = "temp/" + subject_name + " " + subjectTypePluralTitle + " " + to_string(first) + "-" + std::to_string(second) + ".pdf";
                pdfMerge(files, filePath);
                if (UserAlternativeDownload(bd, query->from->id)) {
                    string dir = getUsername(bd, query->from->id).empty() ? to_string(query->from->id) : getUsername(bd, query->from->id);
                    string YaPath = "app:/" + dir + "/" + group_name + " " + subject_name + " " + subject_type + " " + to_string(first) + "-" + std::to_string(second) + ".pdf";
                    string publicLink = uploadFileToYandexDisk(YaToken, filePath, YaPath);
                    bot.getApi().sendMessage(query->message->chat->id, "Файл доступен по ссылке: " + publicLink);
                    if (!publicLink.empty()) {
                        recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                        fs::remove(fs::u8path(filePath));
                    } else {
                        bot.getApi().sendMessage(query->message->chat->id, "Не удалось загрузить файл на Яндекс.Диск");
                        spdlog::error("Failed to upload merged file to Yandex Disk: {}", filePath);
                    }
                } else {
                string response = SendDocumentViaLocalServer(
                        "http://127.0.0.1:8081",
                        token,
                        query->message->chat->id,
                        filePath,
                        "application/pdf",
                        "",
                        "HTML"
                    );
                if (!response.empty()) {
                    recordDownloadedFile(bd, query->message->chat->id, subject_name, group_name, fileType);
                    fs::remove(fs::u8path(filePath));
                } else {
                    bot.getApi().sendMessage(query->message->chat->id, "Не удалось отправить файл");
                    spdlog::error("Failed to send merged file: {}", filePath);
                }
            }
                
            }
            }
            break;
            case SETTINGS:
                if (query->data == "Филиалы") {
                    string text;
                    vector<string> buttons = getGroups(bd);
                    buttons.push_back("Назад");
                    if (UserAccess(bd, query->message->chat->id) <= PREMIUM) {
                        text = "Выберите филиал, к которому хотите присоединиться. Если вашей группы нет, но есть желающий покрывать разницу в программе, обратитесь ко мне @Rozmusel";
                    } else {
                        buttons.erase(remove(buttons.begin(), buttons.end(), getGroupName(bd, query->from->id)), buttons.end());
                        text = "Здесь вы можете управлять списком предметов в своём филиале. Предметы, которые вы загрузили, появятся в списке вашей группы автоматически. Если вы хотите добавить предметы из другой группы, зайдите в гурппу и нажмите на предмет";
                    }
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                }
                if (query->data.starts_with("СМ7-5")) {
                    string text;
                    if(UserAccess(bd, query->message->chat->id) <= PREMIUM) {
                        setUserGroup(bd, query->from->id, query->data.c_str());
                        bot.getApi().answerCallbackQuery(query->id, "Вы присоединились к филиалу " + query->data);
                    } else {
                        vector<pair<string,string>> buttons = compareGroupsBySubjects(bd, getGroupName(bd, query->from->id), query->data);
                        if (buttons.empty()) {
                            text = "В этом филиале пока нет предметов";
                        } else {
                            text = "Выберите предмет, чтобы добавить";
                        }
                        buttons.push_back({"Назад", "Назад"});
                        InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                        bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                    }
                }
                if (query->data.starts_with("delete") || query->data.starts_with("insert")) {
                    if (UserAccess(bd, query->message->chat->id) < ADMIN) {
                        bot.getApi().answerCallbackQuery(query->id, "Функция не доступна");
                        return;
                    }
                    executeCallback(bd, query->data);
                    size_t end = query->data.rfind(':');
                    size_t start = query->data.rfind(':', end - 1);
                    string group_name = query->data.substr(start + 1, end - start - 1);
                    vector<pair<string,string>> buttons = compareGroupsBySubjects(bd, getGroupName(bd, query->from->id), group_name);
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите предмет, чтобы добавить", "", keyboard);
                }
                if (query->data == "Отзывы") {
                    setUserState(bd, query->message->chat->id, FEEDBACK);
                    vector<string> buttons = {"Назад"};
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    bot.getApi().editMessageMedia(MessageMedia(
                        getMediaIdFromDatabase(bd , "feedback"), "Напишите свой отзыв и отправьте сообщение. Я буду рад услышать конструктивную критику"
                    ), query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data.find("Альтернативная загрузка") != string::npos) {
                    spdlog::info("User {} toggled alternative download", query->from->username);
                    changeUserAlternativeDownload(bd, query->from->id);
                    if (getUserFolder(bd, query->from->id).empty() == true) {
                        string username = "app:/" + (getUsername(bd, query->from->id).starts_with("@") ? getUsername(bd, query->from->id) : to_string(query->from->id));
                        setUserFolder(bd, query->from->id, publishFolder(YaToken, username));
                    }
                    vector<string> buttons;
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        buttons.push_back(UserAlternativeDownload(bd, query->from->id) ? "✅ Альтернативная загрузка" : "⬜ Альтернативная загрузка");
                        buttons.push_back(UserSubscription(bd, query->from->id) ? "✅ Подписка" : "⬜ Подписка");
                        buttons.push_back(UserNotification(bd, query->from->id) ? "✅ Уведомления" : "⬜ Уведомления");
                    }
                    buttons.push_back("Филиалы");
                    buttons.push_back("Отзывы");
                    string text = "Пользователь: @" + query->from->username + "\n";
                    buttons.push_back("Премиум");
                    buttons.push_back("Назад");
                    if (getUserFolder(bd, query->from->id).empty() == false) {
                        text += "\nПапка c вашими загруженными файлами: " + getUserFolder(bd, query->from->id);
                    }
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                }
                if (query->data.find("Подписка") != string::npos) {
                    if (UserAccess(bd, query->message->chat->id) < PREMIUM) {
                        bot.getApi().answerCallbackQuery(query->id, "Функция не доступна");
                        return;
                    }
                    changeUserSubscription(bd, query->from->id);
                    if (getUserFolder(bd, query->from->id).empty() == true) {
                        string username = "app:/" + (getUsername(bd, query->from->id).starts_with("@") ? getUsername(bd, query->from->id) : to_string(query->from->id));
                        setUserFolder(bd, query->from->id, publishFolder(YaToken, username));
                    }
                    vector<string> buttons;
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        buttons.push_back(UserAlternativeDownload(bd, query->from->id) ? "✅ Альтернативная загрузка" : "⬜ Альтернативная загрузка");
                        buttons.push_back(UserSubscription(bd, query->from->id) ? "✅ Подписка" : "⬜ Подписка");
                        buttons.push_back(UserNotification(bd, query->from->id) ? "✅ Уведомления" : "⬜ Уведомления");
                    }
                    buttons.push_back("Филиалы");
                    buttons.push_back("Отзывы");
                    string text = "Пользователь: @" + query->from->username + "\n";
                    buttons.push_back("Премиум");
                    buttons.push_back("Назад");
                    if (getUserFolder(bd, query->from->id).empty() == false) {
                        text += "\nПапка c вашими загруженными файлами: " + getUserFolder(bd, query->from->id);
                    }
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                }
                if (query->data.find("Уведомления") != string::npos) {
                    if (UserAccess(bd, query->message->chat->id) < PREMIUM) {
                        bot.getApi().answerCallbackQuery(query->id, "Функция не доступна");
                        return;
                    }
                    changeUserNotification(bd, query->from->id);
                    if (getUserFolder(bd, query->from->id).empty() == true) {
                        string username = "app:/" + (getUsername(bd, query->from->id).starts_with("@") ? getUsername(bd, query->from->id) : to_string(query->from->id));
                        setUserFolder(bd, query->from->id, publishFolder(YaToken, username));
                    }
                    vector<string> buttons;
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        buttons.push_back(UserAlternativeDownload(bd, query->from->id) ? "✅ Альтернативная загрузка" : "⬜ Альтернативная загрузка");
                        buttons.push_back(UserSubscription(bd, query->from->id) ? "✅ Подписка" : "⬜ Подписка");
                        buttons.push_back(UserNotification(bd, query->from->id) ? "✅ Уведомления" : "⬜ Уведомления");
                    }
                    buttons.push_back("Филиалы");
                    buttons.push_back("Отзывы");
                    string text = "Пользователь: @" + query->from->username + "\n";
                    buttons.push_back("Премиум");
                    buttons.push_back("Назад");
                    if (getUserFolder(bd, query->from->id).empty() == false) {
                        text += "\nПапка c вашими загруженными файлами: " + getUserFolder(bd, query->from->id);
                    }
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, text, "", keyboard);
                }
                if (query->data == "Премиум") {
                    vector<string> buttons;
                    string text;
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        buttons.push_back("Назад");
                        text = "Вам доступны все платные функции бота:\n";
                    } else {
                        buttons.push_back("💵 Оплатить 300₽");
                        buttons.push_back("Назад");
                        text = "Премиум подписка открывает доступ ко всем платным функциям бота на текущий семестр (до конца 2026 года):\n";
                    }
                    text += "• Удаление вотермарок\n"
                        "• Быстрая загрузка\n"
                        "• Возможность выбора интервала лекций\n"
                        "• Альтернативный способ загрузки";
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    auto media = MessageMedia(getMediaIdFromDatabase(bd , "premium"), text);
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "💵 Оплатить 300₽") {
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {
                        bot.getApi().answerCallbackQuery(query->id, "Вы уже приобрели премиум подписку");
                        return;
                    }
                    auto price = make_shared<LabeledPrice>();
                    price->label = "Премиум подписка на Black box";
                    price->amount = PRICE;


                    const string providerData = R"({"receipt":{"items":[{"description":"Премиум-доступ","quantity":"1.00","amount":{"value":"300.00","currency":"RUB"},"vat_code":1,"payment_mode":"full_payment","payment_subject":"service"}]}})";
                    bot.getApi().sendInvoice(
                        query->message->chat->id,
                        "Премиум-доступ Black box",
                        "Доступ ко всем платным функциям бота в текущем семестре (до конца 2026 года)",
                        "premium_2026",
                        providerToken,
                        "RUB",
                        {price},
                        providerData,
                        "",
                        0, 0, 0,
                        false, // needName
                        false, // needPhoneNumber
                        true,  // needEmail
                        false, // needShippingAddress
                        false, // sendPhoneNumberToProvider
                        true   // sendEmailToProvider
                    );
                    bot.getApi().answerCallbackQuery(query->id);
                    return;
                }
            break;
            case ADMINISTRATION:
                if (query->data == "Удалить последний файл") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended({{"Лекции", "Del:Lecture"}, {"Семинары", "Del:Seminar"}, {"Назад", "Назад"}});
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите тип файла для удаления", "", keyboard);
                }
                if (query->data == "Изменить файл") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Назад"});
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Чтобы изменить загруженный файл, одним сообщением пришлите новый файл и номер", "", keyboard);
                }
                if (query->data.starts_with("Del:")) {
                    vector<pair<string,string>> buttons = delSubjectsByFiles(bd, query->data.substr(4), getGroupName(bd, query->from->id));
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите предмет последняя запись которого будет удалена", "", keyboard);
                }
                if (query->data.starts_with("delete:")) {
                    deleteLastSubject(bd, query->data.substr(7));
                    size_t end = query->data.rfind(':');
                    size_t start = query->data.rfind(':', end - 1);
                    delSubjectsByFiles(bd, query->data.substr(start + 1, end - start - 1), getGroupName(bd, query->from->id));
                    vector<pair<string,string>> buttons = delSubjectsByFiles(bd, query->data.substr(start + 1, end - start - 1), getGroupName(bd, query->from->id));
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите предмет последняя запись которого будет удалена", "", keyboard);
                }
                if (query->data == "Логи") {
                    vector<string> butt = {"Полные логи", "Пользовательский лог", "Назад"};
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(butt);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите лог для просмотра", "", keyboard);
                }
                if (query->data == "Полные логи") {
                    spdlog::info("Sending full logs to user {}", query->from->username);
                    spdlog::default_logger()->flush();
                    SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                               query->message->chat->id,
                                               "logs/logs.txt", "text/plain");
                }
                if (query->data == "Пользовательский лог") {
                    vector<string> butt = {"Сегодня", "Вчера", "Неделя", "Назад"};
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(butt);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите период для просмотра пользовательского лога", "", keyboard);
                }
                if (query->data == "Сегодня") {
                    spdlog::info("Sending user logs for today to user {}", query->from->username);
                    user_logs->flush();
                    string filePath = "logs/users/users_" + stringDate(0) + ".txt";
                    if (!fs::exists(fs::u8path(filePath))) {
                        bot.getApi().sendMessage(query->message->chat->id, "Пользовательский лог за сегодня ещё не создан");
                        bot.getApi().answerCallbackQuery(query->id);
                        return;
                    }
                    SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                               query->message->chat->id,
                                               filePath, "text/plain");
                }
                if (query->data == "Вчера") {
                    spdlog::info("Sending user logs for yesterday to user {}", query->from->username);
                    user_logs->flush();
                    string filePath = "logs/users/users_" + stringDate(-24) + ".txt";
                    if (!fs::exists(fs::u8path(filePath))) {
                        bot.getApi().sendMessage(query->message->chat->id, "Пользовательский лог за вчера не найден");
                        bot.getApi().answerCallbackQuery(query->id);
                        return;
                    }
                    SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                               query->message->chat->id,
                                               filePath, "text/plain");
                }
                if (query->data == "Неделя") {
                    spdlog::info("Sending user logs for the last week to user {}", query->from->username);
                    user_logs->flush();
                    for (int i = 0; i < 7; i++) {
                        string filePath = "logs/users/users_" + stringDate(-24 * (i + 1)) + ".txt";
                        if (!fs::exists(fs::u8path(filePath))) {
                            bot.getApi().sendMessage(query->message->chat->id, "Пользовательский лог за " + stringDate(-24 * (i + 1)) + " не найден");
                            continue;
                        }
                        SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                                   query->message->chat->id,
                                                   filePath, "text/plain");
                    }
                }
            break;
        }
        if (!callbackAnswered) {
            bot.getApi().answerCallbackQuery(query->id);
        }
    }
    catch (exception &e) {
        bot.getApi().answerCallbackQuery(query->id, "Возникла ошибка, попробуйте позже");
        spdlog::error(e.what());
    } });
    bot.getEvents().onCommand("start", [&bot, &bd](Message::Ptr message) { // Стартовое меню
        spdlog::info("User {} started the bot", message->from->username);
        if (UserState(bd, message->from->id) == REGISTRATION)
            return;
        vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
        if (UserAccess(bd, message->chat->id) == ADMIN)
            buttons.push_back("Администраторская");
        InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
        auto msg = bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase(bd, "start"),
                               "Выберите опцию",
                               nullptr, keyboard, "Markdown");
        if (getLastMenuMessageId(bd, message->from->id) != 0)
            deleteMessageIfExists(bot, message->chat->id, getLastMenuMessageId(bd, message->from->id));
        deleteMessageIfExists(bot, message->chat->id, message->messageId);
        setLastMenuMessageId(bd, message->from->id, msg->messageId);
        setUserState(bd, message->from->id, START);
    });

    bot.getEvents().onPreCheckoutQuery(
        [&bot, &bd](PreCheckoutQuery::Ptr checkout)
        {
            bool valid =
                checkout->invoicePayload == "premium_2026" &&
                checkout->currency == "RUB" &&
                checkout->totalAmount == PRICE &&
                UserAccess(bd, checkout->from->id) < PREMIUM;

            bot.getApi().answerPreCheckoutQuery(
                checkout->id,
                valid,
                valid
                    ? ""
                    : "Невозможно обработать платёж"
            );
        }
    );

    bot.getEvents().onSuccessfulPayment([&bd, &bot](Message::Ptr message, SuccessfulPayment::Ptr payment)
                                        {
            spdlog::info("Successful payment: user={}, telegram_id={}, provider_id={}", message->from->id, payment->telegramPaymentChargeId, payment->providerPaymentChargeId);
            if (payment->invoicePayload != "premium_2026" ||
                payment->currency != "RUB" ||
                payment->totalAmount != PRICE) {
                return;
            }
            setUserAccess(bd, message->from->id, PREMIUM);
            setProviderData(bd, message->from->id,
                payment->providerPaymentChargeId);
            string text = "Вам доступны все платные функции бота:\n"
                "• Удаление вотермарок\n"
                "• Быстрая загрузка\n"
                "• Возможность выбора интервала лекций\n"
                "• Подписки на предметы\n"
                "• Альтернативный способ загрузки";
            InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Назад"});
            auto media = MessageMedia(getMediaIdFromDatabase(bd , "premium"), text);
            bot.getApi().editMessageMedia(media, message->chat->id, getLastMenuMessageId(bd, message->from->id), "", keyboard);
            });

    signal(SIGINT, [](int s)
           {
        spdlog::info("Завершение программы");
        spdlog::default_logger()->flush();
        exit(0); });

    try
    {
        spdlog::info("{} успешно запущен", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true)
        {
            spdlog::trace("Long poll started");
            longPoll.start();
        }
    }
    catch (exception &e)
    {
        spdlog::critical(e.what());
    }

    return 0;
}

