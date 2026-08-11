#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

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
using namespace SQLite;

enum UserStatus
{
    REGISTRATION,
    START,
    STUFF
};
enum UserAccess
{
    DEFAULT,
    PREMIUM,
    ADMIN
};

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

    bot.getEvents().onAnyMessage([&bot, &user_logs, &bd](Message::Ptr message)
                                 {
        try {
            user_logs->info("@{}: {}", message->from->username.c_str(), message->text.c_str());
            spdlog::info("@{}: {}", message->from->username, message->text.c_str());
            if (UserState(bd, message->from->id, message->from->username) == 0) {
                InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Далее"});
                bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase(bd , "logo"),
                "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции.",
                nullptr, keyboard, "Markdown"
                );
            }
            //const auto file = bot.getApi().getFile(message->document->fileId);
            //spdlog::info("path: {}, id: {}", file->filePath, file->fileId);
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
                        "• Альтернативные способы загрузки"
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
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"СМ7-51Б", "СМ7-52Б", "Другая"});
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
                if (query->data.rfind("СМ7-5", 0) == 0) {
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
                    bot.getApi().answerCallbackQuery(query->id, "Формируем файл и отправляем...");
                    callbackAnswered = true;
                    if (!SendDocumentViaLocalServer("http://127.0.0.1:8081", token,
                                                    query->message->chat->id,
                                                    "тест.pdf",
                                                    "application/pdf",
                                                    "", "HTML")) {
                        bot.getApi().sendMessage(query->message->chat->id, "Не удалось отправить файл через локальный сервер.");
                    }
                }
                if (query->data == "Семинары") {

                }
                if (query->data == "Настройки") {
                    vector<string> buttons = {"Уведомления"};
                    string text = "Пользователь: @" + query->from->username + "\n";
                    if (UserAccess(bd, query->message->chat->id) >= PREMIUM) {    // isUserPremium(message->from->id)
                        buttons.push_back("Подписки");
                        text += "Статус: Premium\n Вам доступны все платные функции бота:\n";
                    } else {
                        buttons.push_back("Premium");
                        text += "Статус: Default\n Вам не доступны платные функции бота:\n";
                    }
                    buttons.push_back("Назад");
                    text += "• Удаление вотермарок\n"
                            "• Быстрая загрузка\n"
                            "• Возможность выбора интервала лекций\n"
                            "• Подписки\n"
                            "• Уведомления об изменённых материалах\n"
                            "• Альтернативные способы загрузки";
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = MessageMedia(query->message->chat->id, query->message->messageId,
                        getMediaIdFromDatabase(bd , "peter"), text, keyboard
                    );
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Администраторская") {
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
                if (query->data == "Уведомления") {

                }
                if (query->data == "Подписки") {

                }
                if (query->data == "Премиум") {

                }
                if (query->data == "Удалить последний файл") {

                }
                if (query->data == "Изменить файл") {

                }
                if (query->data == "Логи") {

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
