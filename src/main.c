#include <stdio.h>
#include <locale.h>
#include <string.h>

#include "bot.h"
#include "pdf.h"
#include "archiver.h"
#include "path.h"

#define MAX_MSG_LEN 512
#define ADMIN "Rozmusel"


void callback(BOT* bot, message_t message);


int main(void) {

    setlocale(LC_ALL, "rus");

    BOT* bot = bot_create();

    if (bot == NULL) return -1;

    bot_start(bot, callback);

    bot_delete(bot);

    return 0;
}


void callback(BOT* bot, message_t message) {    // Обработка сообщений
    if (message.user.username != NULL && strcmp(message.user.username, ADMIN) == 0) {   // Дополнительные ветки диалога с админом
        printf("%s | %s\n", message.user.username, message.text ? message.text : message.document->file_name);
        if (message.document != NULL && message.document->file_name != NULL) {  // Ветка передачи файла боту
            char save_path[512] = {0};
            char edit_save_path[512] = {0};
            char file_type[512] = {0};
            char file_subject[512] = {0};
            int number = -1;
            int result = 0;

            result = spot_save_path(save_path, message.document->file_name); // Извлечение пути сохранения из имени файла
            if(!result){
                bot_send_message(bot, message.chat.id, "ОШИБКА: Имя файла содержит неверный формат", NoParseMode);
                return;
            }
            
            if (message.text != NULL && strncmp(message.text, "/edit", 5) == 0) {
                result = sscanf(message.text, "/edit %d", &number);
                if(!result) {
                    bot_send_message(bot, message.chat.id, "ОШИБКА: Нераспознанная команда /edit", NoParseMode);
                    return;
                } else {
                    number = countdir(save_path) + 1;
                    if (number < 1){
                        bot_send_message(bot, message.chat.id, "ОШИБКА: Нарушение автосохранения", NoParseMode);
                        return;
                    }
                }
            }
            spot_type(file_type, message.document->file_name); 
            spot_subject(file_subject, message.document->file_name);
            number = countdir(save_path) + 1;
            snprintf(edit_save_path, sizeof(edit_save_path), "%s/%s;%s_%d.pdf", save_path, file_type, file_subject, number); // Формируем полный путь сохранения
            result = bot_download_document(bot, message.document->file_id, edit_save_path);
            if (result) {
                bot_send_message(bot, message.chat.id, "ОШИБКА: Не удалось загрузить документ", NoParseMode);
            } else {
                bot_send_message(bot, message.chat.id, "Документ успешно сохранён", NoParseMode);
            } return;
        }
    } else {
        printf("%s | Unknown message type\n", message.user.username);
        return;
    }
}
