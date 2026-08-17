#pragma once

#include <cstdarg>
#include <tgbot/tgbot.h>

using namespace std;
using namespace TgBot;

InlineKeyboardMarkup::Ptr RowKeyboard(const vector<string>& texts);
InlineKeyboardMarkup::Ptr ColKeyboard(const vector<string>& texts);
InlineKeyboardMarkup::Ptr RowKeyboardExtended(const vector<pair<string,string>>& buttons);
InlineKeyboardMarkup::Ptr ColKeyboardExtended(const vector<pair<string,string>>& buttons);