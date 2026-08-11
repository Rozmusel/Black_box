#include <SQLiteCpp/SQLiteCpp.h>
#include "spdlog/spdlog.h"
#include <optional>

using namespace std;
using namespace SQLite;

string getMediaIdFromDatabase(Database &db, const string& name) {
    SQLite::Statement query(db, "SELECT file_id FROM media WHERE name = ?");
        query.bind(1, name);
        if (query.executeStep()) {
            return query.getColumn(0).getString();
        }
        throw runtime_error("No file_id found for '" + name + "'");
}

int UserState(Database &db, int64_t chat_id, optional<string> username = nullopt) {
    SQLite::Statement query(db, "SELECT state FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) return query.getColumn(0);

    SQLite::Statement insert(db, "INSERT INTO Users(chat_id, username) VALUES(?, ?)");
    insert.bind(1, chat_id);
    if (username) insert.bind(2, username.value());
    insert.exec();
    spdlog::info("New user added: @{}", username.value());
    return 0;
}

int UserAccess(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "SELECT access FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
            return query.getColumn(0).getInt();
        }
        throw runtime_error("Chat not founded");
}

void setUserState(Database &db, int64_t chat_id, int8_t state) {
    SQLite::Statement query(db, "UPDATE users SET state = ? WHERE chat_id = ?");
    query.bind(1, state);
    query.bind(2, chat_id);
    query.exec();
    spdlog::debug("Users state was changed to {}", state);
}

void setUserGroup(Database &db, int64_t chat_id, const string& group) {
    SQLite::Statement query(db, "UPDATE users SET group_name = ? WHERE chat_id = ?");
    query.bind(1, group);
    query.bind(2, chat_id);
    query.exec();
}

void initDB(Database &db) {
    db.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "chat_id INTEGER PRIMARY KEY, "
        "username TEXT, "
        "group_name TEXT, "
        "state INTEGER DEFAULT 0, "
        "access INTEGER DEFAULT 0"
        ")"
    );
}