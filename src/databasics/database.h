#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <optional>

using namespace std;
using namespace SQLite;

string getMediaIdFromDatabase(Database &db, const string& name);

int UserState(Database &db, int64_t chat_id, optional<string> username = nullopt);
void setUserState(Database &db, int64_t chat_id, int8_t state);
int UserAccess(Database &db, int64_t chat_id);
void setUserGroup(Database &db, int64_t chat_id, const string& group);
void initDB(Database &db);