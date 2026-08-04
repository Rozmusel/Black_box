#pragma once

#include <iostream>
#include <cstdarg>
#include <tgbot/tgbot.h>

using namespace std;
using namespace TgBot;

InlineKeyboardMarkup::Ptr RowKeyboard(int count, ...);
InlineKeyboardMarkup::Ptr ColKeyboard(int count, ...);
InlineKeyboardMarkup::Ptr RowKeyboardExtended(int count, ...);