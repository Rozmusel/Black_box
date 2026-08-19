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

using namespace std;
using namespace TgBot;

namespace fs = std::filesystem;

enum UserStatus
{
    REGISTRATION,
    START,
    LECTURE,
    SEMINAR,
    SETTINGS,
    ADMINISTRATION
};
enum UserAccess
{
    DEFAULT,
    PREMIUM,
    ADMIN
};

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

    Database bd("database/bot.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    initDB(bd);

    string token(getenv("TELEGRAM_BOT_TOKEN"));
    if (token.empty())
    {
        throw runtime_error("TELEGRAM_BOT_TOKEN environment variable is not set");
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
    std::thread consoleThread([&bot]()
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

    bot.getEvents().onAnyMessage([&bot, &user_logs, &bd, &token](Message::Ptr message)
                                 {
        try {
            user_logs->info("@{}: {}", message->from->username.c_str(), message->text.c_str());
            spdlog::info("@{}: {}", message->from->username, message->text.c_str());
            if (message->document != nullptr) {
                const auto file = bot.getApi().getFile(message->document->fileId);
                user_logs->info("@{} sended file: {}", message->chat->username, message->document->fileName);
                spdlog::info("@{} file saved to {}", message->chat->username, file->filePath);

                if (UserAccess(bd, message->from->id) == ADMIN && message->document->mimeType == "application/pdf") {
                    subject sub(bd, message->document->fileName, getGroupName(bd, message->chat->id));
                    bool upd = !message->caption.empty() && message->caption.find_first_not_of("0123456789") == std::string::npos;
                    if (upd == true) {
                        spdlog::info("Updating file: {} count {}", message->document->fileName, message->caption);
                        sub.count = stoi(message->caption);
                    }
                    string newFilePath = "files/" + getGroupName(bd, message->chat->id) + "/" + sub.name + " " + (sub.type == 0 ? "Лекция" : "Семинар") + " " + to_string(sub.count) + ".pdf";
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
                        sub.insert(bd, newFilePath, fileId, getGroupName(bd, message->chat->id));
                        bot.getApi().sendMessage(message->chat->id, "Файл успешно добавлен в базу данных");
                    } else {
                        sub.update(bd, fileId, newFilePath, getGroupName(bd, message->chat->id));
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
                    user_logs->info("@{} sended photo: {}", message->chat->username, fileId);
                    spdlog::info("File saved to {}", fileId);
                }
                bot.getApi().sendMessage(message->chat->id, "Эмм... ладно, спасибо, я посмотрю на досуге");
            }
            
            if (UserState(bd, message->from->id, message->from->username) == REGISTRATION) {
                InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Далее"});
                bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase(bd , "logo"),
                "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции.",
                nullptr, keyboard, "Markdown"
                );
                return;
            }
            
        }
        catch (exception &e) {
            bot.getApi().sendMessage(message->chat->id, "Возникла ошибка, попробуйте позже");
            spdlog::error(e.what());
        } });

    bot.getEvents().onCallbackQuery([&bot, &user_logs, &bd, &token](CallbackQuery::Ptr query)
                                    {
        try {
        bool callbackAnswered = false;
        user_logs->info("@{}: {}", query->from->username.c_str(), query->data.c_str());
        spdlog::info("@{}: {}", query->from->username.c_str(), query->data.c_str());
        if (checkId(bd, query->from->id) == "") {
            bot.getApi().answerCallbackQuery(query->id, "Предыдущие команды не работают, введите /start");
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
                    bot.getApi().deleteMessage(query->message->chat->id, query->message->messageId);
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Принять", "Отклонить"});
                    bot.getApi().sendAnimation(query->message->chat->id, getMediaIdFromDatabase(bd , "agreement2"), 0, 0, 0, "",
                        "Я не пущу вас, пока не примите уже наконец пользовательское соглашение", nullptr, keyboard
                    );
                }
                if (query->data == "Принять") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Успех"});
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "success"), "Вы приняли пользовательское соглашение", keyboard
                );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                    //setUserStatus(query->from->id, 1)  // ставим статус
                    bot.getApi().answerCallbackQuery(query->id, "[[A Great Deal]]");
                }
                if (query->data == "Отклонить") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Назад"});
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "refuse"), "ƪ(˘⌣˘)ʃ", keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                
                if (query->data == "Успех") {
                    vector<string> buttons = getGroups(bd);
                    buttons.push_back("Другая");
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "branch"), "В новой версии открываются филиалы.\n"
                        "Теперь вы можете выбрать свою группу, чтобы иметь материалы по вашей учебной программе", keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Другая") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboardExtended({{"Назад", "Успех"}});
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "cowork"), "Если вашей группы нет, вы можете присоединиться к одной из существующих, "
                        "учебный план которой наиболее близок к вашему.\n"
                        "Вы можете открыть филиал своей группы если найдётся желающий покрывать разницу "
                        "в учебных планах своими лекциями и семинарами\n\n"
                        "К этой работе предъявляются следующие требования:\n"
                        "• Материал должен быть выполнен на электронном носителе\n"
                        "• Пропускать лекции и семинары нельзя (в случае болезни стоит переписать у тех кто посетил)\n"
                        "• Выгружать материалы в хронологическом порядке и в формате .pdf по одной лекции по мере их появления\n\n"
                        "Владельцы филиалов имеют бесплатный доступ к платным функциям бота\n"
                        "Всех желающих готов ввести в курс дела, обращайтесь ко мне @Rozmusel\n", keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data.starts_with("СМ7-5")) {
                    setUserGroup(bd, query->from->id, query->data.c_str());  // добавляет пользователю выбнанную группу
                    setUserState(bd, query->from->id, 1);  // ставим статус общего меню с файлами
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "freedom"), "Поздравляю, вы получили доступ ко всем материалам.\n"
                        "Чтобы попасть в основное меню, воспользуйтесь командой /start", nullptr
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId);
                    bot.getApi().answerCallbackQuery(query->id, "Воздух потрескивает от свободы");
                }
            break;
            case START:
                if (query->data == "Лекции") {
                }
                if (query->data == "Семинары") {

                }
                if (query->data == "Настройки") {
                    setUserState(bd, query->from->id, SETTINGS);
                    vector<string> buttons = {"Филиалы"};
                    string text = "Пользователь: @" + query->from->username + "\n";
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {    // isUserPremium(message->from->id)
                        buttons.push_back("Подписки");
                        buttons.push_back("Уведомления");
                        buttons.push_back("Альтернативная загрузка");
                        text += "Статус: Premium\n Вам доступны все платные функции бота:\n";
                    } else {
                        buttons.push_back("Премиум");
                        text += "Статус: Default\n Вам не доступны платные функции бота:\n";
                    }
                    buttons.push_back("Назад");
                    text += "• Удаление вотермарок\n"
                            "• Быстрая загрузка\n"
                            "• Возможность выбора интервала лекций\n"
                            "• Подписки\n"
                            "• Уведомления об изменённых материалах\n"
                            "• Альтернативный способ загрузки";
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "peter"), text, keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Администраторская") {
                    setUserState(bd, query->from->id, ADMINISTRATION);
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Удалить последний файл", "Изменить файл", "Логи", "Назад"});
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "admin"), "Панель управления ботом", keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Назад") {
                    vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
                    if (UserAccess(bd, query->message->chat->id) == ADMIN) buttons.push_back("Администраторская");    // isUserAdmin(message->from->id)
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "start"), "Выберите опцию", keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
            break;
            case LECTURE:
            break;
            case SEMINAR:
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
                    executeCallback(bd, query->data);
                    size_t end = query->data.rfind(':');
                    size_t start = query->data.rfind(':', end - 1);
                    string group_name = query->data.substr(start + 1, end - start - 1);
                    vector<pair<string,string>> buttons = compareGroupsBySubjects(bd, getGroupName(bd, query->from->id), group_name);
                    buttons.push_back({"Назад", "Назад"});
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboardExtended(buttons);
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId, "Выберите предмет, чтобы добавить", "", keyboard);
                }
                if (query->data == "Уведомления") {

                }
                if (query->data == "Подписки") {

                }
                if (query->data == "Премиум") {

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
        bot.getApi().sendMessage(query->from->id, "Возникла ошибка, попробуйте позже");
        spdlog::error(e.what());
    } });
    bot.getEvents().onCommand("start", [&bot, &bd](Message::Ptr message) { // Стартовое меню
        if (UserState(bd, message->from->id, message->from->username) == REGISTRATION)
            return;
        vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
        if (UserAccess(bd, message->chat->id) == ADMIN)
            buttons.push_back("Администраторская");
        InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
        bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase(bd, "start"),
                               "Выберите опцию",
                               nullptr, keyboard, "Markdown");
        setUserState(bd, message->from->id, START);
    });

    signal(SIGINT, [](int s)
           {
        spdlog::info("Завершение программы");
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
