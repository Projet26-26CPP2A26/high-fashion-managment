#include "connection.h"
#include <QSqlDatabase>

bool Connection::createconnection()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName("Source_Projet2A");  // esm el ODBC mte3ek
    db.setUserName("ton_user");             // user Oracle
    db.setPassword("ton_password");         // password Oracle
    return db.open();
}