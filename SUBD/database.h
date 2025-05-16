#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

class Database {
public:
    static Database& instance();
    QSqlDatabase& getDb();

private:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void createTables();
    QSqlDatabase db;
};

#endif // DATABASE_H
