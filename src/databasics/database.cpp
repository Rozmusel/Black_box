#include <SQLiteCpp/SQLiteCpp.h>
#include "spdlog/spdlog.h"
#include "database.h"
#include <filesystem>

using namespace std;
using namespace SQLite;

void subject::insert(Database &bd, string file_path, string file_id, string group_name) {
        SQLite::Statement query(bd, "INSERT INTO files (file_id, file_path, group_name, name, type, count) VALUES (?, ?, ?, ?, ?, ?)");
        query.bind(1, file_id);
        query.bind(2, file_path);
        query.bind(3, group_name);
        query.bind(4, name);
        query.bind(5, type);
        query.bind(6, count);
        query.exec();
}
void subject::update(Database &bd, string file_id, string file_path, string group_name) {
    if (count > getFileCount(bd, name, type, group_name)) throw runtime_error("File is not exist");

    SQLite::Statement update_query(bd, "UPDATE files SET file_id = ?, file_path = ? "
                                     "WHERE name = ? AND type = ? AND group_name = ? AND count = ?");
    update_query.bind(1, file_id);
    update_query.bind(2, file_path);
    update_query.bind(3, name);
    update_query.bind(4, type);
    update_query.bind(5, group_name);
    update_query.bind(6, count);
    update_query.exec();
    spdlog::debug("Updated file: '{}' of type {} with count {} in group '{}'", name, type, count, group_name);
}

subject::subject(Database &bd, string file_name, string group_name) {
        name = file_name.substr(0, file_name.find("_"));
        file_name.find("Лекция") == string::npos ? type = 1 : type = 0;
        count = getFileCount(bd, name, type, group_name);
        count++;
        spdlog::debug("Created subject: name='{}', type={}, count={}", name, type, count);
    }

    
string getMediaIdFromDatabase(Database &db, const string& name) {
    SQLite::Statement query(db, "SELECT file_id FROM media WHERE name = ?");
        query.bind(1, name);
        if (query.executeStep()) {
            return query.getColumn(0).getString();
        }
        throw runtime_error("No file_id found for '" + name + "'");
}

int addUser(Database &db, int64_t chat_id, const string& username) {
    SQLite::Statement query(db, "INSERT INTO users (chat_id, username) VALUES (?, ?)");
    query.bind(1, chat_id);
    query.bind(2, username);
    query.exec();
    spdlog::info("Добавлен пользователь: {}", username);
    return 0;
}

int UserState(Database& db, int64_t chat_id) {
    SQLite::Statement query(db, "SELECT state FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getInt();
    }
    throw runtime_error("Chat not found");
}

int UserAccess(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "SELECT access FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
            spdlog::debug("User access level: {}", query.getColumn(0).getInt());
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
    spdlog::debug("Setting group '{}' for chat_id {}", group, chat_id);
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
        "access INTEGER DEFAULT 0, "
        "alter_download INTEGER DEFAULT 0, "
        "subscription INTEGER DEFAULT 0, "
        "notification INTEGER DEFAULT 0, "
        "last_menu_message_id INTEGER DEFAULT 0, "
        "folder TEXT DEFAULT '', "
        "provider_data TEXT DEFAULT ''"
        ")"
    );
    db.exec(
        "CREATE TABLE IF NOT EXISTS files ("
        "file_id TEXT PRIMARY KEY, "
        "file_path TEXT, "
        "group_name TEXT, "
        "name TEXT, "
        "type INTEGER, "
        "count INTEGER DEFAULT 0"
        ")"
    );
    db.exec(
        "CREATE TABLE IF NOT EXISTS groups ("
        "name TEXT PRIMARY KEY"
        ")"
    );
    db.exec(
        "CREATE TABLE IF NOT EXISTS delays ("
        "chat_id INTEGER, "
        "file_path TEXT, "
        "scheduled_at INTEGER DEFAULT 0,"
        "PRIMARY KEY (chat_id, file_path)"
        ")"
    );
    db.exec(
        "CREATE TABLE IF NOT EXISTS subscriptions ("
        "chat_id INTEGER, "
        "subject_name TEXT, "
        "type INTEGER, "
        "group_name TEXT, "
        "PRIMARY KEY (chat_id, subject_name, type, group_name)"
        ")"
    );
    db.exec(
        "CREATE TABLE IF NOT EXISTS downloaded_files ("
        "chat_id INTEGER, "
        "subject_name TEXT, "
        "type INTEGER, "
        "group_name TEXT, "
        "PRIMARY KEY (chat_id, subject_name, type, group_name)"
        ")"
    );
    bool legacyHistoryMigrated = false;
    try {
        db.exec(
            "INSERT OR IGNORE INTO downloaded_files (chat_id, subject_name, type, group_name) "
            "SELECT chat_id, subject_name, type, group_name FROM file_deliveries "
            "WHERE subject_name IS NOT NULL AND type IS NOT NULL AND group_name IS NOT NULL"
        );
        legacyHistoryMigrated = true;
    } catch (...) {}
    if (legacyHistoryMigrated) {
        db.exec("DROP TABLE IF EXISTS file_deliveries");
    }
    for (const auto& group : getGroups(db)) {
        db.exec(
            "CREATE TABLE IF NOT EXISTS \"" + group + "\" ("
            "subject_name TEXT, "
            "type INTEGER, "
            "group_name TEXT, "
            "PRIMARY KEY (subject_name, type, group_name)"
            ")"
        );
    }

}

int8_t getFileCount(Database &db, const string& name, int8_t type, const string& group_name) {
    spdlog::debug("Fetching file count for '{}' with type {}", name, type);
    SQLite::Statement query(db, "SELECT COUNT(*) FROM files WHERE name = ? AND type = ? AND group_name = ?");
    query.bind(1, name);
    query.bind(2, type);
    query.bind(3, group_name);
    if (query.executeStep()) {
        return query.getColumn(0).getInt();
    }
    return 0;
}

string getGroupName(Database &db, int64_t chat_id) {
    spdlog::debug("Fetching group name for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT group_name FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    throw runtime_error("Group not found");
}

void addGroupSubject(Database &db, const string& name, int64_t type, const string& group_name) {
    spdlog::debug("Adding subject '{}' of type {} to group '{}'", name, type, group_name);

    SQLite::Statement insert(
        db,
        "INSERT OR IGNORE INTO \"" + group_name +
        "\" (subject_name, type, group_name) VALUES (?, ?, ?)"
    );

    insert.bind(1, name);
    insert.bind(2, type);
    insert.bind(3, group_name);
    insert.exec();
}

vector<string> getGroups(Database &db) {
    spdlog::debug("Fetching all groups from the database");
    SQLite::Statement query(db, "SELECT name FROM groups");
    vector<string> groups;
    while (query.executeStep()) {
        groups.push_back(query.getColumn(0).getString());
    }
    return groups;
}

vector<pair<string,string>> getSubjectsByGroup(Database &db, const string& group_name, int8_t type) {
    spdlog::debug("Fetching subjects for group '{}'", group_name);
    SQLite::Statement query(db, "SELECT subject_name, group_name FROM \"" + group_name + "\" WHERE type = ?");
    query.bind(1, type);
    vector<pair<string,string>> subjects;
    string text;
    string callback_data;
    while (query.executeStep()) {
        if (query.getColumn(1).getString() == group_name) {
            text = query.getColumn(0).getString() + " " + (type == 0 ? "Лекция" : "Семинар");
        } else {
            text = query.getColumn(0).getString() + " " + (type == 0 ? "Лекция" : "Семинар") + " (" + query.getColumn(1).getString() + ")";
        }
        callback_data = "list:" + query.getColumn(0).getString() + ":" + to_string(type) + ":" + query.getColumn(1).getString();
        subjects.push_back({text, callback_data});
    }
    return subjects;
}

vector<pair<string,string>> compareGroupsBySubjects(Database &db, const string& main_group, const string& side_group) {
    spdlog::debug("Comparing groups '{}' and '{}'", main_group, side_group);
    SQLite::Statement query(
    db,
    "SELECT side.*, "
    "EXISTS ("
        "SELECT 1 "
        "FROM \"" + main_group + "\" AS main "
        "WHERE main.group_name <> ? "
        "AND main.subject_name = side.subject_name "
        "AND main.type = side.type"
    ") AS exists_in_main "
    "FROM \"" + side_group + "\" AS side "
    "WHERE side.group_name = ?"
);

query.bind(1, main_group);
query.bind(2, side_group);
    vector<pair<string,string>> subjects;
    while (query.executeStep()) {
        bool exists = query.getColumn("exists_in_main").getInt() == 1;

        string subjectName = query.getColumn("subject_name").getString();
        string type = to_string(query.getColumn("type").getInt());

        string text = subjectName + " " + (query.getColumn("type").getInt() == 0 ? "Лекция" : "Семинар");

        string callbackData = (exists ? "delete:" : "insert:") + subjectName + ":" + type + ":" + side_group + ":" + main_group;

        subjects.push_back({(exists ? "✅ " : "⬜ ") + text, callbackData});
    }
    return subjects;
}

void executeCallback(Database &db, const string& callback_data) {
    spdlog::debug("Executing callback with data: {}", callback_data);
    if (callback_data.empty()) return;

    string action = callback_data.substr(0, callback_data.find(':'));
    string subject_name, group_name, main_group;
    int type;

    size_t first_colon = callback_data.find(':');
    size_t second_colon = callback_data.find(':', first_colon + 1);
    size_t third_colon = callback_data.find(':', second_colon + 1);

    if (first_colon == string::npos || second_colon == string::npos || third_colon == string::npos) {
        throw runtime_error("Invalid callback data format");
    }

    subject_name = callback_data.substr(first_colon + 1, second_colon - first_colon - 1);
    type = stoi(callback_data.substr(second_colon + 1, third_colon - second_colon - 1));
    group_name = callback_data.substr(third_colon + 1, callback_data.find(':', third_colon + 1) - third_colon - 1);
    main_group = callback_data.substr(callback_data.find_last_of(':') + 1);

    if (action == "insert") {
        SQLite::Statement insert_query(db, "INSERT INTO \"" + main_group + "\" (subject_name, type, group_name) VALUES (?, ?, ?)");
        insert_query.bind(1, subject_name);
        insert_query.bind(2, type);
        insert_query.bind(3, group_name);
        insert_query.exec();
    } else if (action == "delete") {
        SQLite::Statement delete_query(db, "DELETE FROM \"" + main_group + "\" WHERE subject_name = ? AND type = ? AND group_name = ?");
        delete_query.bind(1, subject_name);
        delete_query.bind(2, type);
        delete_query.bind(3, group_name);
        delete_query.exec();
    } else {
        throw runtime_error("Unknown action in callback data");
    }
}

string checkId(Database &db, int64_t chat_id) {
    spdlog::debug("Checking if chat_id {} exists in the database", chat_id);
    SQLite::Statement query(db, "SELECT chat_id FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    return "";
}

vector<pair<string,string>> delSubjectsByFiles(Database &db, const string& type, const string& group_name) {
    spdlog::debug("Fetching last file version for deletion of type '{}' in group '{}'", type, group_name);

    SQLite::Statement query(db,
        "SELECT name, type, MAX(count) AS last_count "
        "FROM files "
        "WHERE type = ? AND group_name = ? "
        "GROUP BY name, type"
    );

    query.bind(1, type == "Lecture" ? 0 : 1);
    query.bind(2, group_name);

    vector<pair<string,string>> subjects;
    while (query.executeStep()) {
        const string name = query.getColumn("name").getString();
        const int file_type = query.getColumn("type").getInt();
        const int last_count = query.getColumn("last_count").getInt();

        subjects.push_back({
            name + " " + (file_type == 0 ? "Лекция" : "Семинар") + " " + to_string(last_count),
            "delete:" + name + ":" + (file_type == 0 ? "Lecture" : "Seminar") + ":" + group_name
        });
    }
    return subjects;
}

void deleteLastSubject(Database &db, const string& callback_data) {
    size_t first_colon = callback_data.find(':');
    size_t second_colon = callback_data.find(':', first_colon + 1);

    if (first_colon == string::npos || second_colon == string::npos) {
        throw runtime_error("Invalid callback data format for deletion");
    }

    string subject_name = callback_data.substr(0, first_colon);
    string type = callback_data.substr(first_colon + 1, second_colon - first_colon - 1);
    string group_name = callback_data.substr(second_colon + 1);

    SQLite::Statement delete_query(
        db,
        "DELETE FROM files WHERE file_id = ("
        "SELECT file_id FROM files "
        "WHERE name = ? AND type = ? AND group_name = ? "
        "ORDER BY count DESC LIMIT 1"
        ")"
    );
    delete_query.bind(1, subject_name);
    delete_query.bind(2, type == "Lecture" ? 0 : 1);
    delete_query.bind(3, group_name);
    delete_query.exec();
    filesystem::remove(filesystem::u8path("files/" + group_name + "/" + subject_name + " " + (type == "Lecture" ? "Лекция" : "Семинар") + " " + to_string(getFileCount(db, subject_name, type == "Lecture" ? 0 : 1, group_name) + 1) + ".pdf"));
    spdlog::debug("Deleted last file version of '{}' of type {} in group '{}'", subject_name, type, group_name);
}

int UserSubscription(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "SELECT subscription FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) return query.getColumn(0).getInt();
    throw runtime_error("Chat not found");
}

void changeUserSubscription(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "UPDATE users SET subscription = CASE subscription WHEN 0 THEN 1 ELSE 0 END WHERE chat_id = ?");
    query.bind(1, chat_id);
    query.exec();
}

int UserNotification(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "SELECT notification FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) return query.getColumn(0).getInt();
    throw runtime_error("Chat not found");
}

void changeUserNotification(Database &db, int64_t chat_id) {
    SQLite::Statement query(db, "UPDATE users SET notification = CASE notification WHEN 0 THEN 1 ELSE 0 END WHERE chat_id = ?");
    query.bind(1, chat_id);
    query.exec();
}

int UserSubjectSubscription(Database &db, int64_t chat_id, const string& subject_name, const string& group_name, int8_t type) {
    SQLite::Statement query(db, "SELECT 1 FROM subscriptions WHERE chat_id = ? AND subject_name = ? AND group_name = ? AND type = ?");
    query.bind(1, chat_id);
    query.bind(2, subject_name);
    query.bind(3, group_name);
    query.bind(4, type);
    return query.executeStep() ? 1 : 0;
}

void setSubjectSubscription(Database &db, int64_t chat_id, const string& subject_name, const string& group_name, int8_t type, bool enabled) {
    if (enabled) {
        SQLite::Statement query(db, "INSERT OR IGNORE INTO subscriptions (chat_id, subject_name, type, group_name) VALUES (?, ?, ?, ?)");
        query.bind(1, chat_id);
        query.bind(2, subject_name);
        query.bind(3, type);
        query.bind(4, group_name);
        query.exec();
    } else {
        SQLite::Statement query(db, "DELETE FROM subscriptions WHERE chat_id = ? AND subject_name = ? AND group_name = ? AND type = ?");
        query.bind(1, chat_id);
        query.bind(2, subject_name);
        query.bind(3, group_name);
        query.bind(4, type);
        query.exec();
    }
}

vector<int64_t> getSubjectSubscribers(Database &db, const string& subject_name, const string& group_name, int8_t type) {
    SQLite::Statement query(db,
        "SELECT s.chat_id, u.group_name "
        "FROM subscriptions s JOIN users u ON u.chat_id = s.chat_id "
        "WHERE u.subscription = 1 "
        "AND s.subject_name = ? AND s.group_name = ? AND s.type = ?"
    );
    query.bind(1, subject_name);
    query.bind(2, group_name);
    query.bind(3, type);
    vector<int64_t> result;
    while (query.executeStep()) {
        const int64_t chatId = query.getColumn(0).getInt64();
        const string userGroup = query.getColumn(1).getString();
        SQLite::Statement subjectQuery(
            db,
            "SELECT 1 FROM \"" + userGroup + "\" "
            "WHERE subject_name = ? AND type = ? AND group_name = ? LIMIT 1"
        );
        subjectQuery.bind(1, subject_name);
        subjectQuery.bind(2, type);
        subjectQuery.bind(3, group_name);

        if (subjectQuery.executeStep()) {
            result.push_back(chatId);
        } else {
            SQLite::Statement deleteQuery(
                db,
                "DELETE FROM subscriptions "
                "WHERE chat_id = ? AND subject_name = ? AND type = ? AND group_name = ?"
            );
            deleteQuery.bind(1, chatId);
            deleteQuery.bind(2, subject_name);
            deleteQuery.bind(3, type);
            deleteQuery.bind(4, group_name);
            deleteQuery.exec();
            spdlog::info(
                "Canceled stale subscription: user={}, subject='{}', group='{}', type={}",
                chatId, subject_name, group_name, type
            );
        }
    }
    return result;
}

vector<int64_t> getFileNotificationRecipients(Database &db, const string& subject_name, const string& group_name, int8_t type) {
    SQLite::Statement query(db,
        "SELECT d.chat_id FROM downloaded_files d "
        "JOIN users u ON u.chat_id = d.chat_id "
        "WHERE u.notification = 1 AND u.access >= 1 "
        "AND d.subject_name = ? AND d.group_name = ? AND d.type = ?"
    );
    query.bind(1, subject_name);
    query.bind(2, group_name);
    query.bind(3, type);
    vector<int64_t> result;
    while (query.executeStep()) result.push_back(query.getColumn(0).getInt64());
    return result;
}

void recordDownloadedFile(Database &db, int64_t chat_id, const string& subject_name, const string& group_name, int8_t type) {
    SQLite::Statement query(db,
        "INSERT OR IGNORE INTO downloaded_files (chat_id, subject_name, type, group_name) VALUES (?, ?, ?, ?)"
    );
    query.bind(1, chat_id);
    query.bind(2, subject_name);
    query.bind(3, type);
    query.bind(4, group_name);
    query.exec();
}

void recordDownloadedFileByPath(Database &db, int64_t chat_id, const string& file_path) {
    SQLite::Statement query(db, "SELECT name, type, group_name FROM files WHERE file_path = ?");
    query.bind(1, file_path);
    if (query.executeStep()) {
        recordDownloadedFile(db, chat_id, query.getColumn(0).getString(),
                             query.getColumn(2).getString(),
                             static_cast<int8_t>(query.getColumn(1).getInt()));
    }
}

string getFileId(Database &db, const string& name, int8_t type, int8_t count, const string& group_name) {
    spdlog::debug("Fetching file_id for '{}' of type {} with count {} in group '{}'", name, type, count, group_name);
    SQLite::Statement query(db, "SELECT file_id FROM files WHERE name = ? AND type = ? AND group_name = ? AND count = ?");
    query.bind(1, name);
    query.bind(2, type);
    query.bind(3, group_name);
    query.bind(4, count);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    throw runtime_error("File not found");
}

int UserAlternativeDownload(Database &db, int64_t chat_id) {
    spdlog::debug("Checking alternative download status for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT alter_download FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getInt();
    }
    throw runtime_error("Chat not found");
}

vector<pair<int64_t, string>> getDelayedFiles(Database &db) {
    spdlog::debug("Fetching delayed files that are due to be sent. Current time: {}", chrono::duration_cast<chrono::minutes>(chrono::system_clock::now().time_since_epoch()).count());
    SQLite::Statement query(
        db,
        "SELECT chat_id, file_path "
        "FROM delays "
        "WHERE scheduled_at > 0 "
        "AND scheduled_at <= strftime('%s', 'now') / 60"
    );

    vector<pair<int64_t, string>> delayedFiles;

    while (query.executeStep()) {
        delayedFiles.push_back({
            query.getColumn("chat_id").getInt64(),
            query.getColumn("file_path").getString()
        });
    }
    spdlog::debug("Found {} delayed files to send", delayedFiles.size());
    return delayedFiles;
}

void deleteDelayedFile(Database &db, int64_t chat_id, const string& file_path) {
    SQLite::Statement delete_query(
        db,
        "DELETE FROM delays WHERE chat_id = ? AND file_path = ?"
    );
    delete_query.bind(1, chat_id);
    delete_query.bind(2, file_path);
    delete_query.exec();
    spdlog::debug("Deleted delayed file '{}' for chat_id {}", file_path, chat_id);
}

string getFilePath(Database &db, const string& name, int8_t type, int8_t count, const string& group_name) {
    spdlog::debug("Fetching file path for '{}' of type {} with count {} in group '{}'", name, type, count, group_name);
    SQLite::Statement query(db, "SELECT file_path FROM files WHERE name = ? AND type = ? AND group_name = ? AND count = ?");
    query.bind(1, name);
    query.bind(2, type);
    query.bind(3, group_name);
    query.bind(4, count);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    throw runtime_error("File path not found");
}

string getFileIdByPath(Database &db, const string& file_path) {
    SQLite::Statement query(db, "SELECT file_id FROM files WHERE file_path = ?");
    query.bind(1, file_path);
    if (query.executeStep()) return query.getColumn(0).getString();
    throw runtime_error("File not found by path");
}

void setDelayedFile(Database &db, int64_t chat_id, const string& file_path, int64_t scheduled_at) {
    SQLite::Statement insert_query(
        db,
        "INSERT OR REPLACE INTO delays (chat_id, file_path, scheduled_at) VALUES (?, ?, ?)"
    );
    insert_query.bind(1, chat_id);
    insert_query.bind(2, file_path);
    insert_query.bind(3, scheduled_at);
    insert_query.exec();
    spdlog::debug("Set delayed file '{}' for chat_id {} with scheduled time {}", file_path, chat_id, scheduled_at);
}

int64_t getLastMenuMessageId(Database &db, int64_t chat_id) {
    spdlog::debug("Fetching last menu message ID for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT last_menu_message_id FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getInt64();
    }
    throw runtime_error("Chat not found");
}

void setLastMenuMessageId(Database &db, int64_t chat_id, int64_t message_id) {
    spdlog::debug("Setting last menu message ID {} for chat_id {}", message_id, chat_id);
    SQLite::Statement update_query(db, "UPDATE users SET last_menu_message_id = ? WHERE chat_id = ?");
    update_query.bind(1, message_id);
    update_query.bind(2, chat_id);
    update_query.exec();
}

void changeUserAlternativeDownload(Database &db, int64_t chat_id) {
    spdlog::debug("Toggling alternative download status for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT alter_download FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        int current_status = query.getColumn(0).getInt();
        int new_status = current_status == 0 ? 1 : 0;
        SQLite::Statement update_query(db, "UPDATE users SET alter_download = ? WHERE chat_id = ?");
        update_query.bind(1, new_status);
        update_query.bind(2, chat_id);
        update_query.exec();
        spdlog::debug("{}| Альтернативная загрузка: {}", chat_id, new_status == 1 ? "Включена" : "Выключена");
    } else {
        throw runtime_error("Chat not found");
    }
}

int getUserAlternativeDownload(Database &db, int64_t chat_id) {
    spdlog::debug("Fetching alternative download status for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT alter_download FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getInt();
    }
    throw runtime_error("Chat not found");
}

string getUsername(Database &db, int64_t chat_id) {
    spdlog::debug("Fetching username for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT username FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    throw runtime_error("Chat not found");
}

void setUserFolder(Database &db, int64_t chat_id, const string& folder) {
    spdlog::debug("Setting folder '{}' for chat_id {}", folder, chat_id);
    SQLite::Statement update_query(db, "UPDATE users SET folder = ? WHERE chat_id = ?");
    update_query.bind(1, folder);
    update_query.bind(2, chat_id);
    update_query.exec();
}

string getUserFolder(Database &db, int64_t chat_id) {
    spdlog::debug("Fetching folder for chat_id {}", chat_id);
    SQLite::Statement query(db, "SELECT folder FROM users WHERE chat_id = ?");
    query.bind(1, chat_id);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    throw runtime_error("Chat not found");
}

void setUserAccess(Database &db, int64_t chat_id, int access) {
    spdlog::debug("Setting access level {} for chat_id {}", access, chat_id);
    SQLite::Statement update_query(db, "UPDATE users SET access = ? WHERE chat_id = ?");
    update_query.bind(1, access);
    update_query.bind(2, chat_id);
    update_query.exec();
    spdlog::debug("Changed access level to {} for chat_id {}", access, chat_id);
}

void setProviderData(Database &db, int64_t chat_id, const string& provider_data) {
    spdlog::debug("Setting provider data '{}' for chat_id {}", provider_data, chat_id);
    SQLite::Statement update_query(db, "UPDATE users SET provider_data = ? WHERE chat_id = ?");
    update_query.bind(1, provider_data);
    update_query.bind(2, chat_id);
    update_query.exec();
    spdlog::debug("Changed provider data to '{}' for chat_id {}", provider_data, chat_id);
}

