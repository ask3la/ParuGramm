#include "database.h"
#include <QDebug>
#include <QDir>

Database& Database::instance() {
    static Database db;
    return db;
}

Database::Database() {
    db = QSqlDatabase::addDatabase("QSQLITE", "chatDB");
    QString dbPath = QDir::currentPath() + "/chat.db";
    qDebug() << "Database path:" << dbPath;
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qDebug() << "Database error:" << db.lastError().text();
    } else {
        qDebug() << "Database initialized successfully";
    }
    createTables();
}

Database::~Database() {
    db.close();
}

QSqlDatabase& Database::getDb() {
    return db;
}

void Database::createTables() {
    QSqlQuery query(db);
    if (query.exec("CREATE TABLE IF NOT EXISTS rooms ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "name TEXT NOT NULL, "
                   "password TEXT)")) {
        qDebug() << "Created table 'rooms'";
    } else {
        qDebug() << "Error creating table 'rooms':" << query.lastError().text();
    }

    if (query.exec("CREATE TABLE IF NOT EXISTS messages ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "room_id INTEGER, "
                   "sender_id INTEGER, "
                   "message TEXT, "
                   "file_path TEXT, "
                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)")) {
        qDebug() << "Created table 'messages'";
    } else {
        qDebug() << "Error creating table 'messages':" << query.lastError().text();
    }
}
