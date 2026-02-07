#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>
#include <unistd.h> // sleep

#include <curl/curl.h>
#include <json-c/json.h>

#include <wchar.h>
#include <sys/stat.h>
#include <libgen.h>

#ifndef _BOT_H_
#define _BOT_H_


#include <stdint.h>

#include <curl/curl.h>


typedef enum {
	NoParseMode = 0,
	MarkdownV2 = 1,
	HTML = 2,
	Markdown = 3
} parse_mode_t;


typedef struct {
	CURL* curl;
	char* token;
	uint64_t last_update_id;
} BOT;


typedef struct {
	uint64_t id;
	char* first_name;
	char* last_name;
	char* username;
} user_t;


typedef struct {
	uint64_t id;
	char* type;
} chat_t;

typedef struct {
	char* file_id;
	char* file_unique_id;
	char* file_name;
	char* mime_type;
	int file_size;
} document_t;

typedef struct {
	char* callback_data;
	char* callback_query_id;
	user_t user;
	chat_t chat;
	char* text;
	document_t* document;
} message_t;

typedef struct {
	uint64_t update_id;
	message_t message;
} update_t;





BOT* bot_create();
void bot_delete(BOT* bot);

void bot_start(BOT* bot, void (*callback)(BOT*, message_t));
uint64_t bot_get_updates(BOT* bot, update_t* updates);
int bot_send_message(BOT* bot, uint64_t chat_id, char* text, parse_mode_t parse_mode);
int bot_send_message_with_keyboard(BOT* bot, uint64_t chat_id, parse_mode_t parse_mode, char* text, const char** buttons, size_t button_count);
int bot_answer_callback_query(BOT* bot, const char* callback_query_id, const char* text);
int bot_send_photo(BOT* bot, uint64_t chat_id, const char* text, const char* photo_path);
int bot_send_file(BOT* bot, uint64_t chat_id, const char* text, const char* file_path);
int bot_send_photo_with_keyboard(BOT* bot, uint64_t chat_id, parse_mode_t parse_mode, const char* text, const char** buttons, size_t button_count, const char* photo_path);
int bot_download_document(BOT* bot, const char* file_id, const char* save_path);
int bot_send_files_group(BOT* bot, uint64_t chat_id, const char* text, const char** file_paths, size_t file_count);
#endif // !_BOT_H_
