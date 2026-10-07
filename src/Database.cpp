#include "Database.h"

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "cannot allocate database";
        sqlite3_close(db_);
        throw std::runtime_error("Cannot open database: " + msg);
    }
    exec("PRAGMA foreign_keys = ON");
    createSchema();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::createSchema() {
    exec(R"(
        CREATE TABLE IF NOT EXISTS products (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            name          TEXT    NOT NULL UNIQUE,
            price         REAL    NOT NULL CHECK (price >= 0),
            quantity      INTEGER NOT NULL CHECK (quantity >= 0),
            reorder_level INTEGER NOT NULL DEFAULT 5,
            gst_percent   REAL    NOT NULL DEFAULT 18
        );
        CREATE TABLE IF NOT EXISTS bills (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            customer   TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),
            subtotal   REAL NOT NULL DEFAULT 0,
            discount   REAL NOT NULL DEFAULT 0,
            gst        REAL NOT NULL DEFAULT 0,
            total      REAL NOT NULL DEFAULT 0
        );
        -- product_id is a snapshot (no FK) so old bills survive product deletion
        CREATE TABLE IF NOT EXISTS bill_items (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            bill_id     INTEGER NOT NULL REFERENCES bills(id) ON DELETE CASCADE,
            product_id  INTEGER NOT NULL,
            name        TEXT    NOT NULL,
            quantity    INTEGER NOT NULL,
            unit_price  REAL    NOT NULL,
            gst_percent REAL    NOT NULL,
            line_total  REAL    NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_bill_items_bill ON bill_items(bill_id);
    )");
}

Statement::Statement(Database& db, const std::string& sql) : db_(db.handle()) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        throw std::runtime_error(std::string("SQL prepare error: ") + sqlite3_errmsg(db_));
}

Statement::~Statement() {
    sqlite3_finalize(stmt_);
}

Statement& Statement::bindInt(int idx, int v)    { sqlite3_bind_int(stmt_, idx, v); return *this; }
Statement& Statement::bindDouble(int idx, double v) { sqlite3_bind_double(stmt_, idx, v); return *this; }
Statement& Statement::bindText(int idx, const std::string& v) {
    sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT);
    return *this;
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)  return true;
    if (rc == SQLITE_DONE) return false;
    if (rc == SQLITE_CONSTRAINT)
        throw std::runtime_error(std::string("Constraint violated: ") + sqlite3_errmsg(db_));
    throw std::runtime_error(std::string("SQL step error: ") + sqlite3_errmsg(db_));
}

void Statement::run() {
    while (step()) {}
}

std::string Statement::getText(int col) const {
    const unsigned char* t = sqlite3_column_text(stmt_, col);
    return t ? reinterpret_cast<const char*>(t) : "";
}