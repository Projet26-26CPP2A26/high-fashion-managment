#include "oracleconnection.h"
#include <QDebug>
#include <QSqlError>

namespace {
const QString connectionName = "aurea_oracle";

// Variable d'environnement, ou valeur par défaut si elle est absente.
QString setting(const char *name, const QString &defaultValue)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? defaultValue : value;
}
}

OracleConnection &OracleConnection::instance()
{
    static OracleConnection singleton;   // créé au premier appel, unique pour tout le programme
    return singleton;
}

bool OracleConnection::createConnection()
{
    const bool ok = open();
    if (ok) qDebug() << "Connexion établie";
    else qDebug() << "Échec de la connexion :" << m_message;
    return ok;
}

QString OracleConnection::dsn() const { return setting("AUREA_ORACLE_DSN", "Source_Projet2A"); }
QString OracleConnection::user() const { return setting("AUREA_ORACLE_USER", "Iyed"); }

bool OracleConnection::isOpen() const
{
    return database().isOpen();
}

QSqlDatabase OracleConnection::database() const
{
    return QSqlDatabase::database(connectionName, false);   // false : ne pas l'ouvrir ici
}

bool OracleConnection::open()
{
    m_message.clear();
    if (isOpen()) return true;
    if (!QSqlDatabase::isDriverAvailable("QODBC")) {
        m_message = "Le pilote Qt « QODBC » est introuvable : utilisez un kit Qt qui contient plugins/sqldrivers/qsqlodbc.dll.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::contains(connectionName) ? database()
                                                             : QSqlDatabase::addDatabase("QODBC", connectionName);
    db.setDatabaseName(dsn());   // nom de la source ODBC
    db.setUserName(user());
    db.setPassword(setting("AUREA_ORACLE_PASSWORD", "Esprit18"));
    if (!db.open()) {
        m_message = describe(db.lastError().text());
        return false;
    }
    return true;
}

void OracleConnection::close()
{
    if (QSqlDatabase::contains(connectionName))
        database().close();
}

void OracleConnection::shutdown()
{
    close();
    QSqlDatabase::removeDatabase(connectionName);
}

// Ajoute une explication en français aux erreurs Oracle / ODBC les plus courantes.
QString OracleConnection::describe(const QString &error)
{
    QString hint;
    if (error.contains("IM014"))
        hint = "Architecture incompatible : l'application, le pilote Oracle et la source ODBC doivent tous être en 64 bits.";
    else if (error.contains("IM002"))
        hint = "Source ODBC introuvable : vérifiez son nom dans « Sources de données ODBC (64 bits) ».";
    else if (error.contains("ORA-01017"))
        hint = "Utilisateur ou mot de passe Oracle refusé.";
    else if (error.contains("ORA-28000"))
        hint = "Le compte Oracle est verrouillé.";
    else if (error.contains("ORA-125") || error.contains("ORA-12154") || error.contains("ORA-12170"))
        hint = "Service Oracle injoignable : vérifiez que la base XE et son listener sont démarrés.";
    return hint.isEmpty() ? error : error + "\n→ " + hint;
}
