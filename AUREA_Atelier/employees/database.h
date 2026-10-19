#pragma once
#include <QSqlDatabase>
#include <QString>

// Owns the SQLite connection. EmployeeRepository owns the employee rules.
class Database {
public:
    Database();
    ~Database();
    bool open(const QString &path);
    bool openOracle();   // connexion Oracle (singleton OracleConnection, DSN Source_Projet2A) ; le schéma vient de oracle/01_schema.sql
    QSqlDatabase database() const {return connection;}
    QString error() const {return lastError;}
private:
    QSqlDatabase connection;
    QString connectionName,lastError;
    bool usesOracle=false;
};
