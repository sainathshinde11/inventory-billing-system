#pragma once
#include <string>
#include <stdexcept>
#include "sqlite3.h"

// RAII wrapper around an SQLite connection.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return db_; }
    void exec(const std::string& sql);
    void begin()    { exec("BEGIN"); }
    void commit()   { exec("COMMIT"); }
    void rollback() { exec("ROLLBACK"); }
    int  changes() const { return sqlite3_changes(db_); }
    int  lastInsertId() const { return static_cast<int>(sqlite3_last_insert_rowid(db_)); }

private:
    sqlite3* db_ = nullptr;
    void createSchema();
};

// RAII wrapper around a prepared statement (parameterised queries -> no SQL injection).
class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bindInt(int idx, int v);
    Statement& bindDouble(int idx, double v);
    Statement& bindText(int idx, const std::string& v);

    bool step();   // true if a row is available, false when finished
    void run();    // execute a non-SELECT statement

    int         getInt(int col) const    { return sqlite3_column_int(stmt_, col); }
    double      getDouble(int col) const { return sqlite3_column_double(stmt_, col); }
    std::string getText(int col) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;
};