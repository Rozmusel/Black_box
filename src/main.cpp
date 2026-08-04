#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <tgbot/tgbot.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include "database/database.h"
#include "TGkeyboards/TGkeyboards.h"

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
    string token(getenv("TELEGRAM_BOT_TOKEN"));
    if (token.empty())
    {
        printf("Fatal error: TELEGRAM_BOT_TOKEN environment variable is not set\n");
        exit(0);
    }
    Bot bot(token);

    string file_id;

    bot.getEvents().onAnyMessage([&bot, &file_id](Message::Ptr message) {
        printf("@%s: %s\n", message->from->username.c_str(), message->text.c_str());
        if (1) {    //isUserNew(message->from->id)  // Проверка на нового пользователя, сейчас стоит заглушка
            //addNewUser(message->from->id, message->from->username)    // Добавление нового пользователя (его id и username) в статусе регистрации (мне будет проще, если запись будет по аналогии с enum class UserStatus записанным выше)
            InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(1, "Далее");
            file_id = getMediaIdFromDatabase("logo");
            if (file_id.empty()) {
                printf("Fatal error: check database files for media, logo\n");
                exit(0);
            }
            bot.getApi().sendPhoto(message->chat->id, file_id,
            "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции.",
            nullptr, keyboard, "Markdown"
            );
        }
    });

    bot.getEvents().onCallbackQuery([&bot, &file_id](CallbackQuery::Ptr query) {
        switch (UserStatus::REGISTRATION) { // UserStatus(query->from->id)  // Статус пользователя (используется для дерева диалогов), сейчас стоит заглушка
            case UserStatus::REGISTRATION:
                if (query->data.rfind("Далее", 0) == 0) {
                    if(query->data == "Далее") {
                        InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                        media->media = file_id;
                        media->caption = "Добро пожаловать в обновлённую версию бота!\nБот стал быстрее и удобнее, а также получил новые функции";
                        media->parseMode = "HTML";
                        media->hasSpoiler = false;
                        bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId);
                        file_id = getMediaIdFromDatabase("degree");
                        if (file_id.empty()) {
                            printf("Fatal error: check database files for media, degree\n");
                            exit(0);
                        }
                        bot.getApi().sendDocument(query->message->chat->id, file_id);
                        InlineKeyboardMarkup::Ptr keyboard = RowKeyboard(2, "Принять", "Отклонить");
                        file_id = getMediaIdFromDatabase("agreement");
                        if (file_id.empty()) {
                            printf("Fatal error: check database files for media, agreement\n");
                            exit(0);
                        }
                        bot.getApi().sendAnimation(query->message->chat->id, file_id, 0, 0, 0, "",
                            "Перед началом работы с ботом, примите пользовательское соглашение", nullptr, keyboard
                        );
                    } else {
                        bot.getApi().deleteMessage(query->message->chat->id, query->message->messageId);
                        InlineKeyboardMarkup::Ptr keyboard = RowKeyboard(2, "Принять", "Отклонить");
                        file_id = getMediaIdFromDatabase("agreement2");
                        if (file_id.empty()) {
                            printf("Fatal error: check database files for media, agreement2\n");
                            exit(0);
                        }
                        bot.getApi().sendAnimation(query->message->chat->id, file_id, 0, 0, 0, "",
                            "Я не пущу вас, пока не примите уже наконец пользовательское соглашение", nullptr, keyboard
                        );
                    }
                }
                if (query->data == "Принять") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(1, "Успех");
                    file_id = getMediaIdFromDatabase("succes");
                    if (file_id.empty()) {
                        printf("Fatal error: check database files for media, succes\n");
                        exit(0);
                    }
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = file_id;
                    media->caption = "Стадия принятия пройдена!";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                    bot.getApi().answerCallbackQuery(query->id, "[[A Great Deal]]");
                }
                if (query->data == "Отклонить") {
                    file_id = getMediaIdFromDatabase("refuse");
                    if (file_id.empty()) {
                        printf("Fatal error: check database files for media, refuse\n");
                        exit(0);
                    }
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboardExtended(1, "Вернуться", "Далее_2");
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = file_id;
                    media->caption = "ƪ(˘⌣˘)ʃ";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                
                if (query->data == "Успех") {
                    InlineKeyboardMarkup::Ptr keyboard = ColKeyboard(3, "СМ7-51Б", "СМ7-52Б", "Другая");
                    file_id = getMediaIdFromDatabase("branch");
                    if (file_id.empty()) {
                        printf("Fatal error: check database files for media, branch\n");
                        exit(0);
                    }
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = file_id;
                    media->caption = 
                        "В новой версии открываются филиалы.\n"
                        "Теперь вы можете выбрать свою группу, чтобы иметь материалы по вашей учебной программе";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId, "", keyboard);
                }
                if (query->data == "Другая") {
                    InlineKeyboardMarkup::Ptr keyboard = RowKeyboardExtended(1, "Назад", "Успех");
                    file_id = getMediaIdFromDatabase("cowork");
                    if (file_id.empty()) {
                        printf("Fatal error: check database files for media, cowork\n");
                        exit(0);
                    }
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = file_id;
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
                    file_id = getMediaIdFromDatabase("freedom");
                    if (file_id.empty()) {
                        printf("Fatal error: check database files for media, freedom\n");
                        exit(0);
                    }
                    InputMediaPhoto::Ptr media = make_shared<InputMediaPhoto>();
                    media->media = file_id;
                    media->caption = "Поздравляю, вы получили доступ ко всем материалам. Чтобы попасть в основное меню, воспользуйтесь командой /start";
                    media->parseMode = "HTML";
                    media->hasSpoiler = false;
                    bot.getApi().editMessageMedia(media, query->message->chat->id, query->message->messageId);
                    bot.getApi().answerCallbackQuery(query->id, "Воздух потрескивает от свободы");
                }
            break;
        }
        bot.getApi().answerCallbackQuery(query->id);
    });
    bot.getEvents().onCommand("start", [&bot, &file_id](Message::Ptr message) { // Стартовое меню
        if (1) return;    //isUserNew(message->from->id)
        file_id = getMediaIdFromDatabase("start");
        if (file_id.empty()) {
            printf("Fatal error: check database files for media, start\n");
            exit(0);
        }
        bot.getApi().sendPhoto(message->chat->id, file_id, "Стартовое меню");
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
