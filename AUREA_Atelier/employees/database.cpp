#include "database.h"
#include "oracleconnection.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

Database::Database():connectionName(QUuid::createUuid().toString()) {}
Database::~Database() {
    if(usesOracle) {connection=QSqlDatabase(); return;}   // la connexion Oracle appartient au singleton (fermée par OracleConnection::shutdown())
    connection.close(); connection=QSqlDatabase(); QSqlDatabase::removeDatabase(connectionName);
}
bool Database::openOracle() {
    lastError.clear();
    OracleConnection &oracle=OracleConnection::instance();
    if(!oracle.open()) {lastError=oracle.errorMessage(); return false;}
    connection=oracle.database(); usesOracle=true;
    return true;
}
bool Database::open(const QString &path) {
    lastError.clear();
    if(path!=QStringLiteral(":memory:")&&!QDir().mkpath(QFileInfo(path).absolutePath())) {
        lastError=QStringLiteral("Impossible de créer le dossier de données."); return false;
    }
    connection=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connectionName);
    connection.setDatabaseName(path);
    if(!connection.open()) {lastError=connection.lastError().text(); return false;}
    QSqlQuery query(connection);
    for(const QString &sql:QStringList{"PRAGMA foreign_keys=ON","PRAGMA busy_timeout=5000"}) {
        if(!query.exec(sql)) {lastError=query.lastError().text(); return false;}
    }
    return true;
}
