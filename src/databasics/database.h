#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <optional>

using namespace std;
using namespace SQLite;

class subject {
    public:
    string name;
    int8_t type;
    int8_t count;

    void insert(Database &bd, string file_path, string file_id, string group_name);
    void update(Database &bd, string file_id, string file_path, string group_name);

    subject(Database &bd, string file_name, string group_name);
};

string getMediaIdFromDatabase(Database &db, const string& name);

int UserState(Database &db, int64_t chat_id, optional<string> username = nullopt);
void setUserState(Database &db, int64_t chat_id, int8_t state);
int UserAccess(Database &db, int64_t chat_id);
void setUserGroup(Database &db, int64_t chat_id, const string& group);
void initDB(Database &db);
int8_t getFileCount(Database &db, const string& name, int8_t type, const string& group_name);
string getGroupName(Database &db, int64_t chat_id);
void addGroupSubject(Database &db, const string& name, int64_t type, const string& group_name);
vector<string> getGroups(Database &db);
vector<string> getSubjectsByGroup(Database &db, const string& group_name);
vector<pair<string,string>> compareGroupsBySubjects(Database &db, const string& main_group, const string& side_group);
string checkId(Database &db, int64_t chat_id);
void executeCallback(Database &db, const string& callback_data);
vector<pair<string,string>> delSubjectsByFiles(Database &db, const string& type, const string& group_name);
void deleteLastSubject(Database &db, const string& subject_name);