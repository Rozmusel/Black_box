#include <SQLiteCpp/SQLiteCpp.h>
#include "spdlog/spdlog.h"
#include <optional>
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
    spdlog::info("Updated file '{}' of type {} with count {} in group '{}'", name, type, count, group_name);
}

subject::subject(Database &bd, string file_name, string group_name) {
        name = file_name.substr(0, file_name.find("_"));
        file_name.find("Лекция") == string::npos ? type = 1 : type = 0;
        count = getFileCount(bd, name, type, group_name);
        count++;
    }

    
string getMediaIdFromDatabase(Database &db, const string& name) {
    SQLite::Statement query(db, "SELECT file_id FROM media WHERE name = ?");
        query.bind(1, name);
        if (query.executeStep()) {
            return query.getColumn(0).getString();
        }
        throw runtime_error("No file_id found for '" + name + "'");
}

int UserState(Database &db, int64_t chat_id, optional<string> username) {
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
        "notification INTEGER DEFAULT 0, "
        "subscrition INTEGER DEFAULT 0"
        ")"
    );
    //db.exec(
    //    "DROP TABLE IF EXISTS files"
    //);
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
    for (const auto& group : getGroups(db)) {
        db.exec(
            "CREATE TABLE IF NOT EXISTS \"" + group + "\" ("
            "subject_name TEXT PRIMARY KEY, "
            "type INTEGER, "
            "group_name TEXT"
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
    SQLite::Statement insert(db, "INSERT OR IGNORE INTO \"" + group_name + "\" (subject_name, type, group_name) VALUES (?, ?, ?)");
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

vector<string> getSubjectsByGroup(Database &db, const string& group_name) {
    spdlog::debug("Fetching subjects for group '{}'", group_name);
    SQLite::Statement query(db, "SELECT subject_name FROM \"" + group_name + "\"");
    vector<string> subjects;
    while (query.executeStep()) {
        subjects.push_back(query.getColumn(0).getString()+ " " + (query.getColumn(1).getInt() == 0 ? "Лекция" : "Семинар"));
    }
    return subjects;
}

vector<pair<string,string>> compareGroupsBySubjects(Database &db, const string& main_group, const string& side_group) {
    spdlog::debug("Comparing groups '{}' and '{}'", main_group, side_group);
    SQLite::Statement query(db, "SELECT \"" + side_group + "\".*, EXISTS (SELECT 1 FROM \"" + main_group + "\" WHERE \"" + side_group + "\".subject_name = \"" + main_group + "\".subject_name AND \"" + side_group + "\".type = \"" + main_group + "\".type AND \"" + side_group + "\".group_name = \"" + main_group + "\".group_name) AS exists_in_main FROM \"" + side_group + "\"");
    vector<pair<string,string>> subjects;
    while (query.executeStep()) {
        bool exists = query.getColumn("exists_in_main").getInt() == 1;
        string text = query.getColumn("subject_name").getString() + " " + (query.getColumn("type").getInt() == 0 ? "Лекция" : "Семинар");
        if (exists) {
            subjects.push_back({"✅ " + text, "delete:" + query.getColumn("subject_name").getString() + ":" + to_string(query.getColumn("type").getInt()) + ":" + query.getColumn("group_name").getString() + ":" + main_group});
        } else {
            subjects.push_back({"⬜ " + text, "insert:" + query.getColumn("subject_name").getString() + ":" + to_string(query.getColumn("type").getInt()) + ":" + query.getColumn("group_name").getString() + ":" + main_group});
        }
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