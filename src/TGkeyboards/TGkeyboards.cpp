#include "TGkeyboards.h"

#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace TgBot;

InlineKeyboardMarkup::Ptr RowKeyboard(int count, ...) {
    va_list args;
    va_start(args, count);
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> row;

    for (int i = 0; i < count; ++i) {
        const char* buttonText = va_arg(args, const char*);
        InlineKeyboardButton::Ptr button(new InlineKeyboardButton);
        button->text = buttonText;
        button->callbackData = buttonText;
        row.push_back(button);
    }
    keyboard->inlineKeyboard.push_back(row);
    va_end(args);
    return keyboard;
}

InlineKeyboardMarkup::Ptr ColKeyboard(int count, ...) {
    va_list args;
    va_start(args, count);
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);

    for (int i = 0; i < count; ++i) {
        vector<InlineKeyboardButton::Ptr> row;
        const char* buttonText = va_arg(args, const char*);
        InlineKeyboardButton::Ptr button(new InlineKeyboardButton);
        button->text = buttonText;
        button->callbackData = buttonText;
        row.push_back(button);
        keyboard->inlineKeyboard.push_back(row);
    }
    va_end(args);
    return keyboard;
}

InlineKeyboardMarkup::Ptr RowKeyboardExtended(int count, ...) {
    va_list args;
    va_start(args, count);
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> row;

    for (int i = 0; i < count; ++i) {
        const char* buttonText = va_arg(args, const char*);
        const char* buttonCallback = va_arg(args, const char*);
        InlineKeyboardButton::Ptr button(new InlineKeyboardButton);
        button->text = buttonText;
        button->callbackData = buttonCallback;
        row.push_back(button);
    }
    keyboard->inlineKeyboard.push_back(row);
    va_end(args);
    return keyboard;
}