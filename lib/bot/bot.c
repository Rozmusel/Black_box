#define _POSIX_C_SOURCE 200809L

#include <bot.h>
#include <stdlib.h>
#include <string.h>


char* parse_modes[] = { "NoParseMode", "MarkdownV2", "HTML", "Markdown" };


typedef struct {
	char* data;
	size_t size;
} response_t;


static size_t write_callback(char* data, size_t size, size_t nmemb, void* clientp) {
	const size_t realsize = size * nmemb;
	response_t* resp = (response_t*)clientp;
	const size_t new_resp_size = resp->size + realsize + 1;

	char* ptr = realloc(resp->data, new_resp_size);
	if (ptr == NULL) {
		printf("ERROR: Error during memory reallocation for telegram response\n");
		return 0;
	}

	resp->data = ptr;
	memcpy(&(resp->data[resp->size]), data, realsize);
	resp->size += realsize;
	resp->data[resp->size] = '\0';

	return realsize;
}


static void get_method_url(char* url, size_t url_size, char* token, char* method) {
	snprintf(url, url_size, "https://api.telegram.org/bot%s/%s?", token, method);
}


static int add_url_param_str(CURL* curl, char* url, size_t url_size, char* key, char* value) {
	char* val = curl_easy_escape(curl, value, strlen(value));
	if (val == NULL) {
		printf("ERROR: Error during escaping of special characters\n");
		return EBADMSG;
	}

	const size_t data_size = strlen(key) + strlen(val) + 2 + 1; // len of key + len of val + "=&" + '\0'
	char* data = malloc(data_size);
	if (data == NULL) {
		printf("ERROR: Error during memory allocation for the url parameter\n");
		return ENOMEM;
	}

	snprintf(data, data_size, "%s=%s&", key, val);
	curl_free(val);

	if (strlen(url) + strlen(data) + 1 > url_size) {
		free(data);
		return ENOMEM;
	}
	strcat(url, data);

	free(data);

	return 0;
}

static int add_inline_keyboard(CURL* curl, char* url, size_t url_size, const char** buttons, const char** callback_data, size_t button_count) {
	json_object* keyboard_markup = json_object_new_object();
	json_object* keyboard = json_object_new_array();
	for (size_t i = 0; i < button_count; ++i) {
		json_object* button_row = json_object_new_array();
		json_object* button = json_object_new_object();
		json_object_object_add(button, "text", json_object_new_string(buttons[i]));
        // Use provided callback_data or fall back to button text
        const char* cb_data = (callback_data != NULL) ? callback_data[i] : buttons[i];
        json_object_object_add(button, "callback_data", json_object_new_string(cb_data));
		json_object_array_add(button_row, button);
		json_object_array_add(keyboard, button_row);
	}
	json_object_object_add(keyboard_markup, "inline_keyboard", keyboard);
	const char* keyboard_str = json_object_to_json_string(keyboard_markup);
	int result = add_url_param_str(curl, url, url_size, "reply_markup", (char*)keyboard_str);
	json_object_put(keyboard_markup);
	return result;
}

static int add_url_param_uint(CURL* curl, char* url, size_t url_size, char* key, uint64_t value) {
	char val[21]; // max uint64_t - 18446744073709551615 (20 digits) and '\0'

	snprintf(val, sizeof(val), "%llu", (unsigned long long)value);

	return add_url_param_str(curl, url, url_size, key, val);
}


static user_t parse_user(json_object* json_user) {
	const char* first_name = json_object_get_string(json_object_object_get(json_user, "first_name"));
	const char* last_name = json_object_get_string(json_object_object_get(json_user, "last_name"));
	const char* username = json_object_get_string(json_object_object_get(json_user, "username"));

	user_t user = {
		json_object_get_uint64(json_object_object_get(json_user, "id")),
		NULL,
		NULL,
		NULL
	};

	if (first_name != NULL) {
		const size_t first_name_size = strlen(first_name) + 1;
		user.first_name = malloc(first_name_size);
		if (user.first_name != NULL) memcpy(user.first_name, first_name, first_name_size);
		else printf("ERROR: Error during memory allocation for the user.first_name\n");
	}

	if (last_name != NULL) {
		const size_t last_name_size = strlen(last_name) + 1;
		user.last_name = malloc(last_name_size);
		if (user.last_name != NULL) memcpy(user.last_name, last_name, last_name_size);
		else printf("ERROR: Error during memory allocation for the user.last_name\n");
	}

	if (username != NULL) {
		const size_t username_size = strlen(username) + 1;
		user.username = malloc(username_size);
		if (user.username != NULL) memcpy(user.username, username, username_size);
		else printf("ERROR: Error during memory allocation for the user.username\n");
	}

	return user;
}


static chat_t parse_chat(json_object* json_chat) {
	const char* type = json_object_get_string(json_object_object_get(json_chat, "type"));

	chat_t chat = {
		json_object_get_uint64(json_object_object_get(json_chat, "id")),
		NULL,
	};

	if (type != NULL) {
		const size_t type_size = strlen(type) + 1;
		chat.type = malloc(type_size);
		if (chat.type != NULL) memcpy(chat.type, type, type_size);
		else printf("ERROR: Error during memory allocation for the chat.type\n");
	}

	return chat;
}


static document_t* parse_document(json_object* json_doc) {
	setlocale(LC_ALL, "C.UTF-8");
    if (!json_doc) return NULL;
    document_t* doc = malloc(sizeof(document_t));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(document_t));
    
    const char* file_id = json_object_get_string(json_object_object_get(json_doc, "file_id"));
    const char* file_unique_id = json_object_get_string(json_object_object_get(json_doc, "file_unique_id"));
    const char* file_name = json_object_get_string(json_object_object_get(json_doc, "file_name"));
    const char* mime_type = json_object_get_string(json_object_object_get(json_doc, "mime_type"));
    int file_size = json_object_get_int(json_object_object_get(json_doc, "file_size"));

    // Более безопасное присваивание для file_id
    if (file_id) {
        size_t len = strlen(file_id) + 1;
        doc->file_id = malloc(len);
        if (doc->file_id) {
            strncpy(doc->file_id, file_id, len);
            doc->file_id[len - 1] = '\0';
        } else {
            doc->file_id = NULL;
        }
    } else {
        doc->file_id = NULL;
    }
    // Более безопасное присваивание для file_unique_id
    if (file_unique_id) {
        size_t len = strlen(file_unique_id) + 1;
        doc->file_unique_id = malloc(len);
        if (doc->file_unique_id) {
            strncpy(doc->file_unique_id, file_unique_id, len);
            doc->file_unique_id[len - 1] = '\0';
        } else {
            doc->file_unique_id = NULL;
        }
    } else {
        doc->file_unique_id = NULL;
    }
    // Более безопасное присваивание для file_name
    if (file_name) {
        size_t len = strlen(file_name) + 1;
        doc->file_name = malloc(len);
        if (doc->file_name) {
            strncpy(doc->file_name, file_name, len);
            doc->file_name[len - 1] = '\0';
        } else {
            doc->file_name = NULL;
        }
    } else {
        doc->file_name = NULL;
    }
    // Более безопасное присваивание для mime_type
    if (mime_type) {
        size_t len = strlen(mime_type) + 1;
        doc->mime_type = malloc(len);
        if (doc->mime_type) {
            strncpy(doc->mime_type, mime_type, len);
            doc->mime_type[len - 1] = '\0';
        } else {
            doc->mime_type = NULL;
        }
    } else {
        doc->mime_type = NULL;
    }
    doc->file_size = file_size;
    return doc;
}

static message_t parse_message(json_object* json_message) {
    user_t user = parse_user(json_object_object_get(json_message, "from"));
    chat_t chat = parse_chat(json_object_object_get(json_message, "chat"));

    const char* text = json_object_get_string(json_object_object_get(json_message, "text"));
    const char* caption = json_object_get_string(json_object_object_get(json_message, "caption"));

    message_t message = {0};
    message.user = user;
    message.chat = chat;
    message.text = NULL;
    message.caption = NULL;
    message.callback_data = NULL;
    message.callback_query_id = NULL;
    message.document = NULL;

    if (text != NULL) {
        const size_t text_size = strlen(text) + 1;
        message.text = malloc(text_size);
        if (message.text != NULL) memcpy(message.text, text, text_size);
        else printf("ERROR: Error during memory allocation for the message.text\n");
    }

    if (caption != NULL) {
        const size_t caption_size = strlen(caption) + 1;
        message.caption = malloc(caption_size);
        if (message.caption != NULL) memcpy(message.caption, caption, caption_size);
        else printf("ERROR: Error during memory allocation for the message.caption\n");
    }

    // Парсим документ, если есть
    json_object* json_doc = json_object_object_get(json_message, "document");
    if (json_doc) {
        message.document = parse_document(json_doc);
    }

    return message;
}


static update_t parse_update(json_object* json_update) {
	message_t message = { 0 };  // Initialize to zero

	// Check for callback_query first
	json_object* callback_query = json_object_object_get(json_update, "callback_query");
	if (callback_query != NULL) {
		// Parse the callback data
		const char* callback_data = json_object_get_string(json_object_object_get(callback_query, "data"));
		if (callback_data != NULL) {
			const size_t callback_data_size = strlen(callback_data) + 1;
			message.callback_data = malloc(callback_data_size);
			if (message.callback_data != NULL) {
				memcpy(message.callback_data, callback_data, callback_data_size);
			}
			else {
				printf("ERROR: Error during memory allocation for callback_data\n");
			}
		}

        // Parse callback_query_id
        const char* callback_query_id = json_object_get_string(json_object_object_get(callback_query, "id"));
        if (callback_query_id != NULL) {
            const size_t id_size = strlen(callback_query_id) + 1;
            message.callback_query_id = malloc(id_size);
            if (message.callback_query_id != NULL) {
                memcpy(message.callback_query_id, callback_query_id, id_size);
            } else {
                printf("ERROR: Error during memory allocation for callback_query_id\n");
            }
        }

		// Parse user and chat from callback_query
		message.user = parse_user(json_object_object_get(callback_query, "from"));
		json_object* message_obj = json_object_object_get(callback_query, "message");
		if (message_obj != NULL) {
			message.chat = parse_chat(json_object_object_get(message_obj, "chat"));
		}
	}
	else {
		// Regular message parsing
		json_object* msg = json_object_object_get(json_update, "message");
		if (msg != NULL) {
			message = parse_message(msg);
		}
	}

	update_t update = {
		json_object_get_uint64(json_object_object_get(json_update, "update_id")),
		message
	};

	return update;
}


BOT* bot_create() {
	BOT* bot = malloc(sizeof(BOT));
	if (bot == NULL) {
		printf("ERROR: Error during memory allocation to create a bot structure\n");
		return NULL;
	}

	bot->curl = curl_easy_init();
	if (bot->curl == NULL) {
		printf("ERROR: Error during initialization of the curl structure\n");
		free(bot);
		return NULL;
	}

    const char* env_token = getenv("TELEGRAM_BOT_TOKEN");
    if (!env_token) {
        printf("ERROR: TELEGRAM_BOT_TOKEN environment variable is not set\n");
        bot_delete(bot);
        return NULL;
    }

    bot->token = strdup(env_token);
    if (!bot->token) {
        printf("ERROR: Error during memory allocation for token\n");
        bot_delete(bot);
        return NULL;
    }

	CURLcode curl_code = curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during setting the curl flag CURLOPT_WRITEFUNCTION (%s)\n", curl_easy_strerror(curl_code));
		bot_delete(bot);
		return NULL;
	}

	bot->last_update_id = 0;

	// Initialize mutex for thread-safe CURL operations
	if (pthread_mutex_init(&bot->curl_mutex, NULL) != 0) {
		printf("ERROR: Error during pthread_mutex_init\n");
		bot_delete(bot);
		return NULL;
	}

	return bot;
}


void bot_delete(BOT* bot) {
	pthread_mutex_destroy(&bot->curl_mutex);
	curl_easy_cleanup(bot->curl);
	free(bot->token);
	free(bot);
}


void bot_start(BOT* bot, void (*callback)(BOT*, message_t)) {
	while (1) {
		update_t updates[100]; // max telegram bot api response - 100 update_t (information from telegram bot api specification)
		uint64_t updates_count = bot_get_updates(bot, updates);

		for (uint64_t i = 0; i < updates_count; ++i) {
			update_t update = updates[i];
			(*callback)(bot, update.message);
			bot->last_update_id = (bot->last_update_id > update.update_id) ? bot->last_update_id : update.update_id;
		}

		sleep(1);
	}
}


uint64_t bot_get_updates(BOT* bot, update_t* updates) {
	CURLcode curl_code = CURLE_OK;
	response_t buffer = { NULL, 0 };

	pthread_mutex_lock(&bot->curl_mutex);

	curl_code = curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);
	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during setting the curl flag CURLOPT_WRITEDATA (%s)\n", curl_easy_strerror(curl_code));
		pthread_mutex_unlock(&bot->curl_mutex);
		return 0;
	}

	const uint64_t uint_param_count = 2;
	const uint64_t param_count = uint_param_count;
	const size_t url_size =
		strlen("https://api.telegram.org/bot/?") +
		strlen(bot->token) +
		strlen("getUpdates") +
		2 * param_count +
		strlen("offset") +
		strlen("timeout") +
		20 * uint_param_count
		+ 1;

	char* url = malloc(url_size);
	if (url == NULL) {
		printf("ERROR: Error during memory allocation for url\n");
		pthread_mutex_unlock(&bot->curl_mutex);
		return 0;
	}

	get_method_url(url, url_size, bot->token, "getUpdates");

	if (add_url_param_uint(bot->curl, url, url_size, "offset", bot->last_update_id + 1)) {
		printf("ERROR: Error while adding the 'offset' parameter to the request url\n");
		free(url);
		pthread_mutex_unlock(&bot->curl_mutex);
		return 0;
	}

	if (add_url_param_uint(bot->curl, url, url_size, "timeout", 1)) {
		printf("ERROR: Error while adding the 'timeout' parameter to the request url\n");
		free(url);
		pthread_mutex_unlock(&bot->curl_mutex);
		return 0;
	}

	curl_code = curl_easy_setopt(bot->curl, CURLOPT_URL, url);
	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during setting the curl flag CURLOPT_URL (%s)\n", curl_easy_strerror(curl_code));
		free(url);
		pthread_mutex_unlock(&bot->curl_mutex);
		return 0;
	}

	curl_code = curl_easy_perform(bot->curl);
	
	pthread_mutex_unlock(&bot->curl_mutex);
	
	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
		free(url);
		return 0;
	}
	free(url);

	json_object* obj = json_tokener_parse(buffer.data);

	if (json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
		int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
		const char* description = json_object_get_string(json_object_object_get(obj, "description"));
		printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description);
		json_object_put(obj);
		return 0;
	}

	uint64_t updates_count = 0;
	json_object* json_updates = json_object_object_get(obj, "result");
	while (1) {
		json_object* json_update = json_object_array_get_idx(json_updates, updates_count++);
		if (json_update == NULL) break;

		updates[updates_count - 1] = parse_update(json_update);
	}

	json_object_put(obj);

	return --updates_count;
}


int bot_send_message(BOT* bot, uint64_t chat_id, char* text, parse_mode_t parse_mode) {
  CURLcode curl_code = CURLE_OK;
  int result = 0;
  response_t buffer = { NULL, 0 };

  curl_code = curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);
  if (curl_code != CURLE_OK) {
    printf("ERROR: Error during setting the curl flag CURLOPT_WRITEDATA (%s)\n", curl_easy_strerror(curl_code));
    return EBADMSG;
  }

  char* encoded_text = curl_easy_escape(bot->curl, text, strlen(text));
  if (encoded_text == NULL) {
    printf("ERROR: Error during escaping of special characters\n");
    return EBADMSG;
  }
  char* reply_markup_json = "{\"remove_keyboard\":true}";
  char* reply_markup_escaped = curl_easy_escape(bot->curl, reply_markup_json, strlen(reply_markup_json));
  if (reply_markup_escaped == NULL) {
    printf("ERROR: Error during escaping of reply_markup\n");
    curl_free(encoded_text);
    return EBADMSG;
  }
  const uint64_t uint_param_count = 1;
  const uint64_t str_param_count = 1;
  const uint64_t param_count = uint_param_count + str_param_count + (parse_mode != NoParseMode ? 1 : 0);
  const size_t url_size =
    strlen("https://api.telegram.org/bot/?") +
    strlen(bot->token) +
    strlen("sendMessage") +
    2 * param_count +
    strlen("chat_id") +
    strlen("text") +
    (parse_mode != NoParseMode ? strlen("parse_mode") : 0) +
    20 * uint_param_count +
    strlen(encoded_text) +
    (parse_mode != NoParseMode ? strlen(parse_modes[parse_mode]) : 0) +
    strlen("reply_markup") +
    strlen(reply_markup_escaped) +
    2; // +1 for '&', +1 for '\0'
  curl_free(reply_markup_escaped);

  char* url = malloc(url_size);
  if (url == NULL) {
    printf("ERROR: Error during memory allocation for url\n");
    return ENOMEM;
  }

  get_method_url(url, url_size, bot->token, "sendMessage");

  result = add_url_param_uint(bot->curl, url, url_size, "chat_id", chat_id);
  if (result != 0) {
    printf("ERROR: Error while adding the 'chat_id' parameter to the request url\n");
    free(url);
    return result;
  }

  result = add_url_param_str(bot->curl, url, url_size, "text", text);
  if (result != 0) {
    printf("ERROR: Error while adding the 'text' parameter to the request url\n");
    free(url);
    return result;
  }

  if (parse_mode != NoParseMode) {
    result = add_url_param_str(bot->curl, url, url_size, "parse_mode", parse_modes[parse_mode]);
    if (result != 0) {
      printf("ERROR: Error while adding the 'parse_mode' parameter to the request url\n");
      free(url);
      return result;
    }
  }

  result = add_url_param_str(bot->curl, url, url_size, "reply_markup", "{\"remove_keyboard\":true}");
  if (result != 0) {
    printf("ERROR: Error while removing the 'reply_markup' parameter to the request url\n");
    free(url);
    return result;
  }

  curl_code = curl_easy_setopt(bot->curl, CURLOPT_URL, url);
  if (curl_code != CURLE_OK) {
    printf("ERROR: Error during setting the curl flag CURLOPT_URL (%s)\n", curl_easy_strerror(curl_code));
    free(url);
    return EBADMSG;
  }

  curl_code = curl_easy_perform(bot->curl);
  if (curl_code != CURLE_OK) {
    printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
    free(url);
    return EBADMSG;
  }
  free(url);

  json_object* obj = json_tokener_parse(buffer.data);
  if (json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
    int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
    const char* description = json_object_get_string(json_object_object_get(obj, "description"));
    printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description);
    json_object_put(obj);
    return EBADMSG;
  }
  json_object_put(obj);

  return 0;
}

int bot_send_message_with_keyboard(BOT* bot, uint64_t chat_id, parse_mode_t parse_mode, char* text, const char** buttons, const char** callback_data, size_t button_count) {
	CURLcode curl_code = CURLE_OK;
	response_t buffer = { NULL, 0 };

	char url[512];
	snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", bot->token);

	struct curl_httppost* form = NULL;
	struct curl_httppost* last = NULL;

	char chat_id_str[32];
	snprintf(chat_id_str, sizeof(chat_id_str), "%llu", (unsigned long long)chat_id);

	// Формируем JSON клавиатуры
	json_object* keyboard_markup = json_object_new_object();
	json_object* keyboard = json_object_new_array();
	for (size_t i = 0; i < button_count; ++i) {
		json_object* button_row = json_object_new_array();
		json_object* button = json_object_new_object();
		json_object_object_add(button, "text", json_object_new_string(buttons[i]));
		const char* cb_data = (callback_data != NULL) ? callback_data[i] : buttons[i];
		json_object_object_add(button, "callback_data", json_object_new_string(cb_data));
		json_object_array_add(button_row, button);
		json_object_array_add(keyboard, button_row);
	}
	json_object_object_add(keyboard_markup, "inline_keyboard", keyboard);
	const char* keyboard_str = json_object_to_json_string(keyboard_markup);

	curl_formadd(&form, &last, CURLFORM_COPYNAME, "chat_id", CURLFORM_COPYCONTENTS, chat_id_str, CURLFORM_END);
	curl_formadd(&form, &last, CURLFORM_COPYNAME, "text", CURLFORM_COPYCONTENTS, text, CURLFORM_END);
	if (parse_mode != NoParseMode) {
		curl_formadd(&form, &last, CURLFORM_COPYNAME, "parse_mode", CURLFORM_COPYCONTENTS, parse_modes[parse_mode], CURLFORM_END);
	}
	curl_formadd(&form, &last, CURLFORM_COPYNAME, "reply_markup", CURLFORM_COPYCONTENTS, keyboard_str, CURLFORM_END);

	curl_easy_setopt(bot->curl, CURLOPT_URL, url);
	curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);

	curl_code = curl_easy_perform(bot->curl);

	curl_formfree(form);
	json_object_put(keyboard_markup);

	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
		return EBADMSG;
	}

	json_object* obj = json_tokener_parse(buffer.data);
	if (!obj || json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
		int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
		const char* description = json_object_get_string(json_object_object_get(obj, "description"));
		printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description ? description : "");
		if (obj) json_object_put(obj);
		return EBADMSG;
	}
	json_object_put(obj);

	return 0;

}

int bot_answer_callback_query(BOT* bot, const char* callback_query_id, const char* text) {
    CURLcode curl_code = CURLE_OK;
    response_t buffer = { NULL, 0 };
    int result = 0;

    pthread_mutex_lock(&bot->curl_mutex);

    curl_code = curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);
    if (curl_code != CURLE_OK) {
        printf("ERROR: Error during setting the curl flag CURLOPT_WRITEDATA (%s)\n", curl_easy_strerror(curl_code));
        pthread_mutex_unlock(&bot->curl_mutex);
        return EBADMSG;
    }

    const uint64_t str_param_count = 2; // callback_query_id, text
    const size_t url_size =
        strlen("https://api.telegram.org/bot/?") +
        strlen(bot->token) +
        strlen("answerCallbackQuery") +
        2 * str_param_count +
        strlen("callback_query_id") +
        strlen("text") +
        strlen(callback_query_id) +
        (text ? strlen(text) : 0) +
        1;

    char* url = malloc(url_size);
    if (url == NULL) {
        printf("ERROR: Error during memory allocation for url\n");
        pthread_mutex_unlock(&bot->curl_mutex);
        return ENOMEM;
    }

    get_method_url(url, url_size, bot->token, "answerCallbackQuery");

    result = add_url_param_str(bot->curl, url, url_size, "callback_query_id", (char*)callback_query_id);
    if (result != 0) {
        printf("ERROR: Error while adding the 'callback_query_id' parameter to the request url\n");
        free(url);
        pthread_mutex_unlock(&bot->curl_mutex);
        return result;
    }

    if (text != NULL) {
        result = add_url_param_str(bot->curl, url, url_size, "text", (char*)text);
        if (result != 0) {
            printf("ERROR: Error while adding the 'text' parameter to the request url\n");
            free(url);
            pthread_mutex_unlock(&bot->curl_mutex);
            return result;
        }
    }

    curl_code = curl_easy_setopt(bot->curl, CURLOPT_URL, url);
    if (curl_code != CURLE_OK) {
        printf("ERROR: Error during setting the curl flag CURLOPT_URL (%s)\n", curl_easy_strerror(curl_code));
        free(url);
        pthread_mutex_unlock(&bot->curl_mutex);
        return EBADMSG;
    }

    curl_code = curl_easy_perform(bot->curl);
    
    pthread_mutex_unlock(&bot->curl_mutex);
    
    if (curl_code != CURLE_OK) {
        printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
        free(url);
        return EBADMSG;
    }
    free(url);

    json_object* obj = json_tokener_parse(buffer.data);
    if (json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
        int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
        const char* description = json_object_get_string(json_object_object_get(obj, "description"));
        printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description);
        json_object_put(obj);
        return EBADMSG;
    }
    json_object_put(obj);

    return 0;
}


int bot_send_photo(BOT* bot, uint64_t chat_id, const char* text, const char* photo_path) {
    CURLcode curl_code = CURLE_OK;
    response_t buffer = { NULL, 0 };

    char url[512];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", bot->token);

    struct curl_httppost* form = NULL;
    struct curl_httppost* last = NULL;

    char chat_id_str[32];
    snprintf(chat_id_str, sizeof(chat_id_str), "%llu", (unsigned long long)chat_id);

    curl_formadd(&form, &last, CURLFORM_COPYNAME, "chat_id", CURLFORM_COPYCONTENTS, chat_id_str, CURLFORM_END);
    curl_formadd(&form, &last, CURLFORM_COPYNAME, "photo", CURLFORM_FILE, photo_path, CURLFORM_END);
    if (text) {
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "caption", CURLFORM_COPYCONTENTS, text, CURLFORM_END);
    }

    curl_easy_setopt(bot->curl, CURLOPT_URL, url);
    curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);

    curl_code = curl_easy_perform(bot->curl);

    curl_formfree(form);

    if (curl_code != CURLE_OK) {
        printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
        return EBADMSG;
    }

    json_object* obj = json_tokener_parse(buffer.data);
    if (!obj || json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
        int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
        const char* description = json_object_get_string(json_object_object_get(obj, "description"));
        printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description ? description : "");
        if (obj) json_object_put(obj);
        return EBADMSG;
    }
    json_object_put(obj);

    return 0;
}

int bot_send_file(BOT* bot, uint64_t chat_id, const char* text, const char* file_path) {
	CURLcode curl_code = CURLE_OK;
	response_t buffer = { NULL, 0 };

	char url[512];
	snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendDocument", bot->token);

	struct curl_httppost* form = NULL;
	struct curl_httppost* last = NULL;

	char chat_id_str[32];
	snprintf(chat_id_str, sizeof(chat_id_str), "%llu", (unsigned long long)chat_id);

	curl_formadd(&form, &last, CURLFORM_COPYNAME, "chat_id", CURLFORM_COPYCONTENTS, chat_id_str, CURLFORM_END);
	curl_formadd(&form, &last, CURLFORM_COPYNAME, "document", CURLFORM_FILE, file_path, CURLFORM_END);
	if (text) {
		curl_formadd(&form, &last, CURLFORM_COPYNAME, "caption", CURLFORM_COPYCONTENTS, text, CURLFORM_END);
	}

	pthread_mutex_lock(&bot->curl_mutex);

	curl_easy_setopt(bot->curl, CURLOPT_URL, url);
	curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);

	curl_code = curl_easy_perform(bot->curl);

	curl_formfree(form);

	pthread_mutex_unlock(&bot->curl_mutex);

	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
		return EBADMSG;
	}

	json_object* obj = json_tokener_parse(buffer.data);
	if (!obj || json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
		int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
		const char* description = json_object_get_string(json_object_object_get(obj, "description"));
		printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description ? description : "");
		if (obj) json_object_put(obj);
		return EBADMSG;
	}
	json_object_put(obj);

	return 0;

}
int bot_send_photo_with_keyboard(BOT* bot, uint64_t chat_id, parse_mode_t parse_mode, const char* text, const char** buttons, const char** callback_data, size_t button_count, const char* photo_path) {
    CURLcode curl_code = CURLE_OK;
    response_t buffer = { NULL, 0 };

    char url[512];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", bot->token);

    struct curl_httppost* form = NULL;
    struct curl_httppost* last = NULL;

    char chat_id_str[32];
    snprintf(chat_id_str, sizeof(chat_id_str), "%llu", (unsigned long long)chat_id);

    // Формируем JSON клавиатуры
    json_object* keyboard_markup = json_object_new_object();
    json_object* keyboard = json_object_new_array();
    for (size_t i = 0; i < button_count; ++i) {
        json_object* button_row = json_object_new_array();
        json_object* button = json_object_new_object();
        json_object_object_add(button, "text", json_object_new_string(buttons[i]));
        const char* cb_data = (callback_data != NULL) ? callback_data[i] : buttons[i];
        json_object_object_add(button, "callback_data", json_object_new_string(cb_data));
        json_object_array_add(button_row, button);
        json_object_array_add(keyboard, button_row);
    }
    json_object_object_add(keyboard_markup, "inline_keyboard", keyboard);
    const char* keyboard_str = json_object_to_json_string(keyboard_markup);

    curl_formadd(&form, &last, CURLFORM_COPYNAME, "chat_id", CURLFORM_COPYCONTENTS, chat_id_str, CURLFORM_END);
    curl_formadd(&form, &last, CURLFORM_COPYNAME, "photo", CURLFORM_FILE, photo_path, CURLFORM_END);
    if (text) {
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "caption", CURLFORM_COPYCONTENTS, text, CURLFORM_END);
    }
    if (parse_mode != NoParseMode) {
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "parse_mode", CURLFORM_COPYCONTENTS, parse_modes[parse_mode], CURLFORM_END);
    }
    curl_formadd(&form, &last, CURLFORM_COPYNAME, "reply_markup", CURLFORM_COPYCONTENTS, keyboard_str, CURLFORM_END);

    curl_easy_setopt(bot->curl, CURLOPT_URL, url);
    curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);

    curl_code = curl_easy_perform(bot->curl);

    curl_formfree(form);
    /* Clear previous multipart/form state to avoid affecting subsequent requests */
    curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, NULL);

    json_object_put(keyboard_markup);

    if (curl_code != CURLE_OK) {
        printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
        return EBADMSG;
    }

    json_object* obj = json_tokener_parse(buffer.data);
    if (!obj || json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
        int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
        const char* description = json_object_get_string(json_object_object_get(obj, "description"));
        printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description ? description : "");
        if (obj) json_object_put(obj);
        return EBADMSG;
    }
    json_object_put(obj);

    return 0;
}

static int mkdirs_for_file(const char* path) {
    if (!path) return EINVAL;
    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return ENAMETOOLONG;
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';

    // Удаляем имя файла, оставляем только директорию
    char* p = strrchr(tmp, '/');
    if (!p) return 0; // Нет директорий, только файл в текущей папке
    *p = '\0';

    // Создаём директории по частям
    for (char* s = tmp + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            mkdir(tmp, 0755);
            *s = '/';
        }
    }
    mkdir(tmp, 0755);
    return 0;
}
int bot_send_files_group(BOT* bot, uint64_t chat_id, const char* text, const char** file_paths, size_t file_count) {
	if (!bot || !file_paths || file_count == 0) return EINVAL;
	CURLcode curl_code = CURLE_OK;
	response_t buffer = { NULL, 0 };

	char url[512];
	snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMediaGroup", bot->token);

	struct curl_httppost* form = NULL;
	struct curl_httppost* last = NULL;

	char chat_id_str[32];
	snprintf(chat_id_str, sizeof(chat_id_str), "%llu", (unsigned long long)chat_id);
	curl_formadd(&form, &last, CURLFORM_COPYNAME, "chat_id", CURLFORM_COPYCONTENTS, chat_id_str, CURLFORM_END);

	// Формируем JSON массив media
	json_object* media_array = json_object_new_array();
	char field_name[32];
	for (size_t i = 0; i < file_count; ++i) {
		snprintf(field_name, sizeof(field_name), "file%zu", i + 1);
		json_object* media_obj = json_object_new_object();
		json_object_object_add(media_obj, "type", json_object_new_string("document"));
		char attach_name[64];
		snprintf(attach_name, sizeof(attach_name), "attach://%s", field_name);
		json_object_object_add(media_obj, "media", json_object_new_string(attach_name));
		if (i == file_count - 1 && text) {
			json_object_object_add(media_obj, "caption", json_object_new_string(text));
		}
		json_object_array_add(media_array, media_obj);
		// Добавляем файл в форму
		curl_formadd(&form, &last, CURLFORM_COPYNAME, field_name, CURLFORM_FILE, file_paths[i], CURLFORM_END);
	}
	const char* media_json = json_object_to_json_string(media_array);
	curl_formadd(&form, &last, CURLFORM_COPYNAME, "media", CURLFORM_COPYCONTENTS, media_json, CURLFORM_END);

	curl_easy_setopt(bot->curl, CURLOPT_URL, url);
	curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, form);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);

	curl_code = curl_easy_perform(bot->curl);

	curl_formfree(form);
	/* Clear previous multipart/form state to avoid affecting subsequent requests */
	curl_easy_setopt(bot->curl, CURLOPT_HTTPPOST, NULL);

	json_object_put(media_array);

	if (curl_code != CURLE_OK) {
		printf("ERROR: Error during https request execution (%s)\n", curl_easy_strerror(curl_code));
		return EBADMSG;
	}

	json_object* obj = json_tokener_parse(buffer.data);
	if (!obj || json_object_get_boolean(json_object_object_get(obj, "ok")) == 0) {
		int32_t error_code = json_object_get_int(json_object_object_get(obj, "error_code"));
		const char* description = json_object_get_string(json_object_object_get(obj, "description"));
		printf("ERROR: The response from the telegram server is false (%i - %s)\n", error_code, description ? description : "");
		if (obj) json_object_put(obj);
		return EBADMSG;
	}
	json_object_put(obj);
	return 0;
}
static size_t file_write_callback(char* data, size_t size, size_t nmemb, void* userp) {
    FILE* fp = (FILE*)userp;
    if (!fp) return 0;
    return fwrite(data, size, nmemb, fp);
}

int bot_download_document(BOT* bot, const char* file_id, const char* save_path) {
    if (!file_id || !save_path) return EINVAL;
    CURLcode curl_code = CURLE_OK;
    response_t buffer = { NULL, 0 };
    char url[512];

    // 1. Получить file_path через getFile
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getFile?file_id=%s", bot->token, file_id);
    curl_easy_setopt(bot->curl, CURLOPT_URL, url);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(bot->curl, CURLOPT_WRITEDATA, (void*)&buffer);
    curl_code = curl_easy_perform(bot->curl);
    if (curl_code != CURLE_OK) {
        printf("ERROR: getFile request failed (%s)\n", curl_easy_strerror(curl_code));
        return EBADMSG;
    }
    json_object* obj = json_tokener_parse(buffer.data);
    if (!obj || !json_object_get_boolean(json_object_object_get(obj, "ok"))) {
        printf("ERROR: getFile response not ok\n");
        if (obj) json_object_put(obj);
        return EBADMSG;
    }
    json_object* result = json_object_object_get(obj, "result");
    const char* file_path = json_object_get_string(json_object_object_get(result, "file_path"));
    if (!file_path) {
        printf("ERROR: file_path not found in getFile response\n");
        json_object_put(obj);
        return EBADMSG;
    }
    json_object_put(obj);

    // 2. Скачать файл по file_path с отдельным curl
    char file_url[512];
    snprintf(file_url, sizeof(file_url), "https://api.telegram.org/file/bot%s/%s", bot->token, file_path);
    mkdirs_for_file(save_path); // Ensure all parent directories exist
    FILE* fp = fopen(save_path, "wb");
    if (!fp) {
        printf("ERROR: Cannot open file for writing: %s\n", save_path);
        perror("fopen");
        return ENOENT;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        printf("ERROR: curl_easy_init failed\n");
        return ENOMEM;
    }
    curl_easy_setopt(curl, CURLOPT_URL, file_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (curl_code != CURLE_OK) {
        printf("ERROR: Download file failed (%s)\n", curl_easy_strerror(curl_code));
        return EBADMSG;
    }
    return 0;
}
