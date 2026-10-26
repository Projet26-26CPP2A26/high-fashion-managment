#include "clientdatabase.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

bool openClientDatabase()
{
    // Connexion wahda barka (Singleton)
    QSqlDatabase db = QSqlDatabase::contains(QSqlDatabase::defaultConnection)
                          ? QSqlDatabase::database(QSqlDatabase::defaultConnection, false)
                          : QSqlDatabase::addDatabase("QODBC");

    if (db.isOpen())
        return true;

    db.setDatabaseName("projetCpp");   // nafs esm el DSN (Partie B)
    db.setUserName("meniyar");              // BADDELHA
    db.setPassword("TON_PASSWORD");          // BADDELHA

    if (!db.open()) {
        qDebug() << "Erreur connexion Oracle:" << db.lastError().text();
        return false;
    }
    return true;
}