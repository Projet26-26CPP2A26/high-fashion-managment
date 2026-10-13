#pragma once
#include <QSqlDatabase>
#include <QString>

// Owns the SQLite connection. EmployeeRepository owns the employee rules.
class Database {
public:
    Database();
    ~Database();
    bool open(const QString &path);
    QSqlDatabase database() const {return connection;}
    QString error() const {return lastError;}
private:
    QSqlDatabase connection;
    QString connectionName,lastError;
};
