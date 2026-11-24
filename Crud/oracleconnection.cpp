#include "oracleconnection.h"
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>

OracleConnection::OracleConnection() {}

OracleConnection &OracleConnection::instance()
{
    static OracleConnection connection;
    return connection;
}

bool OracleConnection::open()
{
    message.clear();
    QString dsn = qEnvironmentVariable("AUREA_ORACLE_DSN");
    QString user = qEnvironmentVariable("AUREA_ORACLE_USER");
    QString password = qEnvironmentVariable("AUREA_ORACLE_PASSWORD");
    if (dsn.isEmpty())
        dsn = "Source_Projet2A";
    if (user.isEmpty())
        user = "Iyed";
    if (password.isEmpty())
        password = "aurea";
    if (!QSqlDatabase::isDriverAvailable("QODBC")) {
        message = "Le pilote Qt QODBC est introuvable dans ce kit Qt.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(dsn);
    db.setUserName(user);
    db.setPassword(password);
    if (!db.open()) {
        message = "Connexion Oracle impossible (DSN " + dsn + ", utilisateur " + user + ").\n" + db.lastError().text();
        return false;
    }
    return true;
}

void OracleConnection::close()
{
    {
        QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

QString OracleConnection::errorMessage() const
{
    return message;
}

int OracleConnection::nextId(const QString &table)
{
    QSqlQuery query;
    if (query.exec("SELECT COALESCE(MAX(id), 0) + 1 FROM " + table) && query.next())
        return query.value(0).toInt();
    return 1;
}
