#include "TGkeyboards.h"
#include "spdlog/spdlog.h"

#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace TgBot;

InlineKeyboardMarkup::Ptr RowKeyboard(const vector<string>& texts) {
    spdlog::debug("Building row keyboard with {} buttons", texts.size());

    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> row;

    for (const auto& text : texts) {
        InlineKeyboardButton::Ptr button(new InlineKeyboardButton);
        button->text = text;
        button->callbackData = text;
        row.push_back(button);
    }
    keyboard->inlineKeyboard.push_back(row);
    return keyboard;
}

InlineKeyboardMarkup::Ptr ColKeyboard(const vector<string>& texts) {
    spdlog::debug("Building col keyboard with {} buttons", texts.size());

    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);

    for (const auto& text : texts) {
        vector<InlineKeyboardButton::Ptr> row;
        InlineKeyboardButton::Ptr button(new InlineKeyboardButton);
        button->text = text;
        button->callbackData = text;
        row.push_back(button);
        keyboard->inlineKeyboard.push_back(row);
    }
    return keyboard;
}

InlineKeyboardMarkup::Ptr RowKeyboardExtended(const vector<pair<string,string>>& buttons) {
    spdlog::debug("Building row extended keyboard with {} buttons", buttons.size());
    
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> row;

    for (const auto& button : buttons) {
        InlineKeyboardButton::Ptr buttonPtr(new InlineKeyboardButton);
        buttonPtr->text = button.first;
        buttonPtr->callbackData = button.second;
        row.push_back(buttonPtr);
    }
    keyboard->inlineKeyboard.push_back(row);
    return keyboard;
}