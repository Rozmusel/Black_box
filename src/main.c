#include <stdio.h>
#include <locale.h>
#include <string.h>

#include "bot.h"
#include "pdf.h"
#include "archiver.h"

#pragma execution_character_set("utf-8")

#define MAX_MSG_LEN 512


void callback(BOT* bot, message_t message);


int main(void) {

    setlocale(LC_ALL, "rus");

    BOT* bot = bot_create();

    if (bot == NULL) return -1;

    bot_start(bot, callback);

    bot_delete(bot);

    return 0;
}


void callback(BOT* bot, message_t message) { // Проверка работы основных функций бота
    if (message.text != NULL && strcmp(message.text, "/backup") == 0) {
        char input[] = "docs";
        char output[] = "docs.zip";
        dir_archiver(input, output);
        bot_send_file(bot, message.chat.id, "Резервная копия документов", "docs.zip");
        return;
    }
    else if (message.text != NULL && strcmp(message.text, "/merge") == 0) {
        const char* file_paths[] = { "docs/file1.pdf", "docs/file2.pdf", "merged_file.pdf", "docs/file3.pdf", "watermarked_file.pdf" };
        pdf_merge(file_paths[0], file_paths[1], file_paths[2]);
        pdf_add_watermark(file_paths[3], file_paths[4]);
        bot_send_files_group(bot, message.chat.id, "Группа файлов", (const char* []) { "merged_file.pdf", "watermarked_file.pdf" }, 2);
        return;
    }
    else if (message.text != NULL) {
        printf("%s | %s\n", message.user.username, message.text);
        bot_send_message(bot, message.chat.id, "Инициализирована _проверка_ *функций* бота", MarkdownV2);
        bot_send_photo(bot, message.chat.id, "Фото", "media/photo.jpg");
        bot_send_file(bot, message.chat.id, message.text, "media/photo.jpg");
        bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Для проверки клавиатуры выберите одну из кнопок", (const char* []) { "Один", "Два", "Три" }, 3);
        bot_send_photo_with_keyboard(bot, message.chat.id, NoParseMode, "Фото с клавиатурой", (const char* []) { "А", "Б", "В" }, 3, "media/photo.jpg");
    }
    else if (message.callback_data != NULL) {
        printf("%s | %s\n", message.user.username, message.callback_data);
        if (message.callback_query_id != NULL) {
            bot_answer_callback_query(bot, message.callback_query_id, NULL);
        }
        bot_send_message(bot, message.chat.id, "Нажатие клавиатуры считано", NoParseMode);
        bot_send_message(bot, message.chat.id, message.callback_data, NoParseMode);
        bot_send_message(bot, message.chat.id, "Пришлите три документа. Первые два будут объеденены и отправлены в группе с третьим на который будет наложена вотермарка. Файлы назовите file1, file2 и file3, затем напишите команду /merge", NoParseMode);
    }
    else if (message.document != NULL) {
        printf("%s | %s\n", message.user.username, message.document->file_name);
        char save_path[256];
        snprintf(save_path, sizeof(save_path), "docs/%s", message.document->file_name);
        bot_download_document(bot, message.document->file_id, save_path);
    }
    else {
        printf("%s | Unknown message type\n", message.user.username);
        return;
    }
}
