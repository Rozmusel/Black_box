#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <string.h>
#include <pthread.h>

#include "bot.h"
#include "pdf.h"
#include "archiver.h"
#include "path.h"

#define MAX_MSG_LEN 512
#define ADMIN "Rozmusel"

typedef struct {
    BOT* bot;
    uint64_t chat_id;
    char file_path[512];
} file_send_args_t;

void* send_file_async(void* args) {
    file_send_args_t* file_args = (file_send_args_t*)args;
    int result = bot_send_file(file_args->bot, file_args->chat_id, NULL, file_args->file_path);
    if (result) {
        bot_send_message(file_args->bot, file_args->chat_id, "ОШИБКА: Не удалось отправить файл", NoParseMode);
    }
    free(file_args);
    return NULL;
}

void callback(BOT* bot, message_t message);

const char* lectures[] = {"Вариационные исчисления", "ТФКПиОИ", "Материаловедение", "Метрология, стандартизация и сертификация", "Электронные устройства мехатронных и робототехнических систем", "Электротехника", "Политология", "Основы мехатроники и робототехники"};
const char* seminars[] = {"Вариационные исчисления", "ТФКПиОИ", "Иностранный язык", "Политология", "Электротехника"};
const char* lectures_ex[] = {"ВариационныеИсчисления", "ТФКПиОИ", "Материаловедение", "Метрология", "ЭлектронныеУстройства", "Электротехника", "Политология", "ОсновыМехатроники"};
const char* seminars_ex[] = {"ВариационныеИсчисления", "ТФКПиОИ", "ИностранныйЯзык", "Политология", "Электротехника"};


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
        if (message.document != NULL && message.document->file_name != NULL) {  // Ветка передачи файла боту
            printf("%s | %s\n", message.user.username, message.text ? message.text : message.document->file_name);
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
            
            if (message.caption != NULL && strncmp(message.caption, "/edit", 5) == 0) {
                result = sscanf(message.caption, "/edit %d", &number);
                if(!result) {
                    bot_send_message(bot, message.chat.id, "ОШИБКА: Нераспознанная команда /edit", NoParseMode);
                    return;
                } else {
                    if (number < 1){
                        bot_send_message(bot, message.chat.id, "ОШИБКА: Нарушение автосохранения", NoParseMode);
                        return;
                    }
                }
            } else {
                number = countdir(save_path) + 1;
            }
            spot_type(file_type, message.document->file_name); 
            spot_subject(file_subject, message.document->file_name);
            snprintf(edit_save_path, sizeof(edit_save_path), "%s/%s;%s_%d.pdf", save_path, file_type, file_subject, number); // Формируем полный путь сохранения
            result = bot_download_document(bot, message.document->file_id, edit_save_path);
            if (result) {
                bot_send_message(bot, message.chat.id, "ОШИБКА: Не удалось загрузить документ", NoParseMode);
            } else {
                bot_send_message(bot, message.chat.id, "Документ успешно сохранён", NoParseMode);
            } return;
        }
    }
    if (message.text != NULL && strcmp(message.text, "/start") == 0) {
        printf("%s | %s\n", message.user.username, message.text);
        const char* options[] = {"Лекции", "Семинары", "Прочее"};
        const char* opt_data[] = {"0:0", "0:1", "0:2"};
        bot_send_photo_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите опцию", options, opt_data, 3, "media/opening.jpg");
        return;
    }

    /* Обработка callback_data (кнопки) */
    if (message.callback_data != NULL) {
        int result = 0;
        printf("%s | %s\n", message.user.username, message.callback_data);
        
        /* Answer callback query IMMEDIATELY to avoid timeout (Telegram requires response within ~30 seconds) */
        if (message.callback_query_id != NULL) {
            bot_answer_callback_query(bot, message.callback_query_id, NULL);
        }
        
        if (message.callback_data[0] != '\0' && (isdigit((unsigned char)message.callback_data[0]) || message.callback_data[0] == 'c')) {
            switch (message.callback_data[0]) {
                case '0':
                    if (message.callback_data[2] != '\0') {
                        switch (message.callback_data[2]) {
                            case '0':
                                printf("%s | Выбран предмет: %s\n", message.user.username, "Лекции");
                                bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите лекционный предмет", lectures, (const char*[]){"1:0", "1:1", "1:2", "1:3", "1:4", "1:5", "1:6", "1:7"}, 8);
                            break;
                            case '1':
                                bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите семинарский предмет", seminars, (const char*[]){"2:0", "2:1", "2:2", "2:3", "2:4"}, 5);
                            break;
                            case '2': 
                                char msg[MAX_MSG_LEN];
                                snprintf(msg, MAX_MSG_LEN, "Здравствуй, %s. Этот раздел в будщем будет расширен. Пока что здесь пусто.", message.user.first_name ? message.user.first_name : message.user.username ? message.user.username : "[[НЕВОЗМОЖНЫЙ ПОЛЬЗОВАТЕЛЬ]]");
                                bot_send_message(bot, message.chat.id, msg, NoParseMode);
                            break;
                            default: bot_send_message(bot, message.chat.id, "Неверная опция", NoParseMode); break;
                        }
                    }
                    break;
                case '1':
                    switch (message.callback_data[2]) {
                        case '0': {
                            char *dir_path = "docs/Лекция/ВариационныеИсчисления";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Вариационные Исчисления %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:0:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            
                            break;
                        }
                        case '1': {
                            char *dir_path = "docs/Лекция/ТФКПиОИ";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "ТФКПиОИ %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:1:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            
                            break;
                        }
                        case '2': {
                            char *dir_path = "docs/Лекция/Материаловедение";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Материаловедение %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:2:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            
                            break;
                        }
                        case '3': {
                            char *dir_path = "docs/Лекция/Метрология";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Метрология %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:3:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            
                            break;
                        }
                        case '4': {
                            char *dir_path = "docs/Лекция/ЭлектронныеУстройства";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Электронные Устройства %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:4:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            
                            break;
                        }
                        case '5': {
                            char *dir_path = "docs/Лекция/Электротехника";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Электротехника %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:5:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '6': {
                            char *dir_path = "docs/Лекция/Политология";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Политология %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:6:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '7': {
                            char *dir_path = "docs/Лекция/ОсновыМехатроники";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Основы Мехатроники %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;0:7:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        default:
                            bot_send_message(bot, message.chat.id, "Неверная категория", NoParseMode);
                            break;
                    }
                    
                    break;
                case '2':
                    switch (message.callback_data[2]) {
                        case '0': {
                            char *dir_path = "docs/Семинар/ВариационныеИсчисления";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Вариационные Исчисления %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;1:0:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '1': {
                            char *dir_path = "docs/Семинар/ТФКПиОИ";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "ТФКПиОИ %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;1:1:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '2': {
                            char *dir_path = "docs/Семинар/ИностранныйЯзык";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Иностранный Язык %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;1:2:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '3': {
                            char *dir_path = "docs/Семинар/Политология";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Политология %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;1:3:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        case '4': {
                            char *dir_path = "docs/Семинар/Электротехника";
                            int count = countdir(dir_path);
                            char **buttons = malloc(count * sizeof(char*));
                            char **callback_data = malloc(count * sizeof(char*));
                            char file_name[512];
                            char cb_data[32];
                            for (int i = 0; i < count; i++) {
                                snprintf(file_name, sizeof(file_name), "Электротехника %d", i + 1);
                                snprintf(cb_data, sizeof(cb_data), "c:0;1:4:%d", i + 1);
                                buttons[i] = malloc(strlen(file_name) + 1);
                                callback_data[i] = malloc(strlen(cb_data) + 1);
                                strcpy(buttons[i], file_name);
                                strcpy(callback_data[i], cb_data);
                            }
                            bot_send_message_with_keyboard(bot, message.chat.id, NoParseMode, "Выберите файл для скачивания", (const char**)buttons, (const char**)callback_data, count);
                            for (int i = 0; i < count; i++) {
                                free(buttons[i]);
                                free(callback_data[i]);
                            }
                            free(buttons);
                            free(callback_data);
                            break;
                        }
                        default:
                            bot_send_message(bot, message.chat.id, "Неверная категория", NoParseMode);
                            break;
                    }
                    break;
                case 'c': 
                    switch(message.callback_data[2]) {
                        case '0': {
                            char file_path[512];
                            char subject_name[128];
                            char subject_type[32];
                            snprintf(subject_name, sizeof(subject_name), "%s", message.callback_data[4] == '0' ? lectures_ex[message.callback_data[6] - '0'] : seminars_ex[message.callback_data[6] - '0']);
                            snprintf(subject_type, sizeof(subject_type), "%s", message.callback_data[4] == '0' ? "Лекция" : "Семинар");
                            snprintf(file_path, sizeof(file_path), "docs/%s/%s/%s;%s_%s.pdf", subject_type, subject_name, subject_type, subject_name, &message.callback_data[8]);
                            printf("%s | Скачивание файла: %s\n", message.user.username, file_path);
                            
                            // Launch file sending in a separate thread to avoid blocking
                            file_send_args_t* args = malloc(sizeof(file_send_args_t));
                            if (args) {
                                args->bot = bot;
                                args->chat_id = message.chat.id;
                                strncpy(args->file_path, file_path, sizeof(args->file_path) - 1);
                                args->file_path[sizeof(args->file_path) - 1] = '\0';
                                
                                pthread_t thread_id;
                                if (pthread_create(&thread_id, NULL, send_file_async, args) != 0) {
                                    printf("ERROR: Failed to create thread for file sending\n");
                                    bot_send_message(bot, message.chat.id, "ОШИБКА: Не удалось отправить файл", NoParseMode);
                                    free(args);
                                } else {
                                    pthread_detach(thread_id);
                                }
                            }
                        break;
                        }
                        default:
                            bot_send_message(bot, message.chat.id, "Неверная категория", NoParseMode);
                            break;
                        }
                    break;
            }
        } else {
            char msg[MAX_MSG_LEN];
            snprintf(msg, MAX_MSG_LEN, "Вы выбрали: %s", message.callback_data);
            bot_send_message(bot, message.chat.id, msg, NoParseMode);
        }

        return;
    } else {
        printf("%s | Unknown message type\n", message.user.username);
        return;
    }
}
