#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include <tgbot/tgbot.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"

#include "database/database.h"
#include "TGkeyboards/TGkeyboards.h"

#define ISUSERNEW 0 // удалить после добавления бд
#define ISUSERADMIN 1 // удалить после добавления бд
#define ISUSERPREMIUM 1 // удалить после добавления бд

using namespace std;
using namespace TgBot;
using namespace SQLite;

enum class UserStatus
{
    REGISTRATION,
    START,
    STUFF
};

namespace TgBot
{ // Fix for InputMedia types
    const std::string InputMediaPhoto::TYPE = "photo";
    const std::string InputMediaVideo::TYPE = "video";
    const std::string InputMediaAnimation::TYPE = "animation";
    const std::string InputMediaAudio::TYPE = "audio";
    const std::string InputMediaDocument::TYPE = "document";
}

int main()
{
    auto user_logs = spdlog::daily_logger_mt("user_logs", "logs/users.txt", 0, 0);
    user_logs->set_pattern("[%Y-%m-%d %H:%M:%S] %v");

    string token(getenv("TELEGRAM_BOT_TOKEN"));
    if (token.empty()) {
            throw runtime_error("TELEGRAM_BOT_TOKEN environment variable is not set");
        }
    Bot bot(token);

    bot.getEvents().onAnyMessage([&bot, &user_logs](Message::Ptr message) {
        printf("@%s: %s\n", message->from->username.c_str(), message->text.c_str());    // удалить перед релизом
        user_logs->info("@{}: {}", message->from->username, message->text.c_str());
        if (ISUSERNEW) {    //isUserNew(message->from->id)  // Проверка на нового пользователя, сейчас стоит заглушка
            //addNewUser(message->from->id, message->from->username)    // Добавление нового пользователя (его id и username) в статусе регистрации (мне будет проще, если запись будет по аналогии с enum class UserStatus записанным выше)
            InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Далее"});
            bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase("logo"),
            "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции.",
            nullptr, keyboard, "Markdown"
            );
        }
    });

    bot.getEvents().onCallbackQuery([&bot, &user_logs](CallbackQuery::Ptr query) {
        printf("@%s:: %s\n", query->from->username.c_str(), query->data.c_str());    // удалить перед релизом
        user_logs->info("@{}:: {}", query->from->username.c_str(), query->data.c_str());
        switch (UserStatus::START) { // UserStatus(query->from->id)  // Статус пользователя (используется для дерева диалогов), сейчас стоит заглушка
            case UserStatus::REGISTRATION:
                if (query->data == "Далее") {
                    bot.getApi().editMessageCaption(query->message->chat->id, query->message->messageId,
                        "В новой версии бота появились:\n• Удаление вотермарок\n• Быстрая загрузка\n• Возможность выбора интервала лекций\n• Подписки"
                    );
                    bot.getApi().sendDocument(query->message->chat->id, getMediaIdFromDatabase("degree"));
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Принять", "Отклонить"});
                    bot.getApi().sendAnimation(query->message->chat->id, getMediaIdFromDatabase("agreement"), 0, 0, 0, "",
                        "Перед началом работы с ботом, примите пользовательское соглашение", nullptr, keyboard
                    );
                }
                if (query->data == "Назад") {
                    bot.getApi().deleteMessage(query->message->chat->id, query->message->messageId);
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Принять", "Отклонить"});
                    bot.getApi().sendAnimation(query->message->chat->id, getMediaIdFromDatabase("agreement2"), 0, 0, 0, "",
                        "Я не пущу вас, пока не примите уже наконец пользовательское соглашение", nullptr, keyboard
                    );
                }
                if (query->data == "Принять") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Успех"});
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("succes");
                    media->caption = "Стадия принятия пройдена!";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                    bot.getApi().answerCallbackQuery(query->id, "[[A Great Deal]]");
                }
                if (query->data == "Отклонить") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboard({"Назад"});
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("refuse");
                    media->caption = "ƪ(˘⌣˘)ʃ";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                
                if (query->data == "Успех") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"СМ7-51Б", "СМ7-52Б", "Другая"});
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("branch");
                    media->caption = 
                        "В новой версии открываются филиалы.\n"
                        "Теперь вы можете выбрать свою группу, чтобы иметь материалы по вашей учебной программе";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Другая") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboardExtended({{"Назад", "Успех"}});
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("cowork");
                    media->caption = "Если вашей группы нет, вы можете присоединиться к одной из существующих, учебный план которой наиболее близок к вашему.\n"
                        "Вы можете открыть филиал своей группы если найдётся желающий покрывать разницу в учебных планах своими лекциями и семинарами\n\n"
                        "К этой работе предъявляются следующие требования:\n"
                        "• Материал должен быть выполнен на электронном носителе\n"
                        "• Пропускать лекции и семинары нельзя (в случае болезни стоит переписать у тех кто посетил)\n"
                        "• Выгружать материалы в хронологическом порядке и в формате .pdf по одной лекции по мере их появления\n\n"
                        "Владельцы филиалов имеют бесплатный доступ к платным функциям бота\n"
                        "Всех желающих готов ввести в курс дела, обращайтесь ко мне @Rozmusel\n";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data.rfind("СМ7-5", 0) == 0) {
                    //addUserGroup(query->from->id, query->data)  // добавляет пользователю выбнанную группу
                    //setUserStatus(query->from->id, 2)  // ставим статус общего меню с файлами
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("freedom");
                    media->caption = "Поздравляю, вы получили доступ ко всем материалам. Чтобы попасть в основное меню, воспользуйтесь командой /start";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId);
                    bot.getApi().answerCallbackQuery(query->id, "Воздух потрескивает от свободы");
                }
            break;
            case UserStatus::START:
                if (query->data == "Лекции") {

                }
                if (query->data == "Семинары") {

                }
                if (query->data == "Настройки") {
                    vector<string> buttons;
                    string text = "Пользователь: @" + query->from->username + "\n";
                    if (ISUSERPREMIUM) {    // isUserPremium(message->from->id)
                        buttons.push_back("Подписки");
                        text += "Статус: Premium\n Вам доступны все платные функции бота:\n";
                    } else {
                        buttons.push_back("Premium");
                        text += "Статус: Default\n Вам не доступны платные функции бота:\n";
                    }
                    buttons.push_back("Назад");
                    text += "• Удаление вотермарок\n• Быстрая загрузка\n• Возможность выбора интервала лекций\n• Подписки";
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("peter");
                    media->caption = text;
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Администраторская") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard({"Удалить последний файл", "Изменить файл", "Статистика", "Назад"});
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("admin");
                    media->caption = "Панель управления ботом";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Назад") {
                    vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
                    if (ISUSERADMIN) buttons.push_back("Администраторская");    // isUserAdmin(message->from->id)
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = getMediaIdFromDatabase("start");
                    media->caption = "Выберите опцию";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Подписки") {

                }
                if (query->data == "Премиум") {

                }
                if (query->data == "Удалить последний файл") {

                }
                if (query->data == "Изменить файл") {

                }
                if (query->data == "Назад") {

                }
                if (query->data == "Назад") {

                }
            break;
        }
        bot.getApi().answerCallbackQuery(query->id);
    });
    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) { // Стартовое меню
        if (ISUSERNEW) return;    //isUserNew(message->from->id)
        vector<string> buttons = {"Лекции", "Семинары", "Настройки"};
        if (ISUSERADMIN) buttons.push_back("Администраторская");
        InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(buttons);
        bot.getApi().sendPhoto(message->chat->id, getMediaIdFromDatabase("start"),
            "Выберите опцию",
            nullptr, keyboard, "Markdown"
        );
    });

    signal(SIGINT, [](int s)
           {
        printf("SIGINT got\n");
        exit(0); });

    try
    {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        bot.getApi().deleteWebhook();

        TgLongPoll longPoll(bot);
        while (true)
        {
            printf("Long poll started\n");
            longPoll.start();
        }
    }
    catch (exception &e)
    {
        printf("error: %s\n", e.what());
    }

    return 0;
}
