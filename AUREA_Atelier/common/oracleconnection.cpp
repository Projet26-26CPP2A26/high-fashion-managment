#include "oracleconnection.h"
#include <QSqlQuery>
#include <QStringList>
#include <QSysInfo>
#include <QtGlobal>

namespace {
const QString kPrivateName = QStringLiteral("aurea_oracle");
bool g_useDefaultConnection = false;

// Variable d'environnement, ou valeur par défaut si elle est absente ou vide.
QString envOr(const char *name, const QString &fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}
}

OracleConnection &OracleConnection::instance() {
    static OracleConnection singleton;   // Meyers : créé au premier appel, détruit à la fin du programme
    return singleton;
}

OracleConnection::Settings OracleConnection::settings() {
    Settings s;
    s.dsn = envOr("AUREA_ORACLE_DSN", QStringLiteral("Source_Projet2A"));
    s.user = envOr("AUREA_ORACLE_USER", QStringLiteral("hsan"));
    s.hasPassword = !qEnvironmentVariable("AUREA_ORACLE_PASSWORD").isEmpty();
    return s;
}

bool OracleConnection::driverAvailable() {
    return QSqlDatabase::isDriverAvailable(QStringLiteral("QODBC"));
}

QString OracleConnection::connectionName() {
    return g_useDefaultConnection ? QString::fromLatin1(QSqlDatabase::defaultConnection) : kPrivateName;
}

void OracleConnection::useDefaultConnection(bool enable) {
    g_useDefaultConnection = enable;
}

bool OracleConnection::isOpen() const {
    if (!QSqlDatabase::contains(connectionName())) return false;
    return QSqlDatabase::database(connectionName(), false).isOpen();
}

QSqlDatabase OracleConnection::database() const {
    if (!QSqlDatabase::contains(connectionName())) return QSqlDatabase();
    return QSqlDatabase::database(connectionName(), false);
}

bool OracleConnection::open() {
    m_error = QSqlError();
    m_message.clear();
    if (isOpen()) return true;

    if (!driverAvailable()) {
        m_message = QStringLiteral(
            "Le pilote Qt « QODBC » est introuvable.\n"
            "Pilotes SQL disponibles : %1.\n"
            "Vérifiez que le kit Qt utilisé contient plugins/sqldrivers/qsqlodbc.dll "
            "(installateur Qt, composant du kit MinGW ou MSVC 64 bits).")
                        .arg(QSqlDatabase::drivers().join(QStringLiteral(", ")));
        m_error = QSqlError(QStringLiteral("QODBC"), m_message, QSqlError::ConnectionError);
        return false;
    }

    const Settings s = settings();
    QString password = qEnvironmentVariable("AUREA_ORACLE_PASSWORD");
    if (password.isEmpty()) {
        m_message = QStringLiteral(
            "Le mot de passe Oracle n'est pas configuré.\n"
            "Définissez la variable d'environnement AUREA_ORACLE_PASSWORD "
            "(Qt Creator : Projets → Exécuter → Environnement d'exécution), puis relancez.");
        m_error = QSqlError(QStringLiteral("AUREA_ORACLE_PASSWORD"), m_message, QSqlError::ConnectionError);
        return false;
    }

    bool opened = false;
    {   // portée limitée : aucune poignée QSqlDatabase ne survit à cette fonction
        QSqlDatabase db = QSqlDatabase::contains(connectionName())
                              ? QSqlDatabase::database(connectionName(), false)
                              : QSqlDatabase::addDatabase(QStringLiteral("QODBC"), connectionName());
        db.setDatabaseName(s.dsn);   // nom de DSN : QODBC le distingue d'une chaîne « DRIVER=...; »
        db.setUserName(s.user);
        db.setPassword(password);
        opened = db.open();
        db.setPassword(QString());   // le mot de passe ne reste pas dans l'objet de connexion
        if (!opened) {
            m_error = db.lastError();
            m_message = describe(m_error);
        }
    }
    password.clear();
    return opened;
}

void OracleConnection::close() {
    if (!QSqlDatabase::contains(connectionName())) return;
    QSqlDatabase db = QSqlDatabase::database(connectionName(), false);
    if (db.isOpen()) db.close();
}

void OracleConnection::shutdown() {
    if (!QSqlDatabase::contains(connectionName())) return;
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName(), false);
        if (db.isOpen()) db.close();
    }   // la poignée locale est détruite avant removeDatabase()
    QSqlDatabase::removeDatabase(connectionName());
}

QString OracleConnection::describe(const QSqlError &error) {
    const QString raw = error.text().trimmed();
    QString hint;
    const auto has = [&](const char *text) { return raw.contains(QLatin1String(text), Qt::CaseInsensitive); };
    if (has("IM014") || has("architecture mismatch"))
        hint = QStringLiteral("Architecture incompatible : l'application, le pilote Oracle et la source ODBC doivent tous être en 64 bits. "
                              "Utilisez un kit Qt 64 bits et créez/testez la source dans « Sources de données ODBC (64 bits) ».");
    else if (has("IM002") || has("Data source name not found"))
        hint = QStringLiteral("Source ODBC introuvable : vérifiez le nom du DSN (AUREA_ORACLE_DSN) dans « Sources de données ODBC (64 bits) », "
                              "onglet DSN utilisateur ou système.");
    else if (has("ORA-01017"))
        hint = QStringLiteral("Utilisateur ou mot de passe Oracle refusé : vérifiez AUREA_ORACLE_USER et AUREA_ORACLE_PASSWORD.");
    else if (has("ORA-28000"))
        hint = QStringLiteral("Le compte Oracle est verrouillé.");
    else if (has("ORA-28001"))
        hint = QStringLiteral("Le mot de passe Oracle a expiré.");
    else if (has("ORA-12541") || has("ORA-12514") || has("ORA-12154") || has("ORA-12560") || has("ORA-12170"))
        hint = QStringLiteral("Service Oracle injoignable : vérifiez que la base XE et son listener sont démarrés et que le nom de service du DSN est correct.");
    return hint.isEmpty() ? raw : raw + QStringLiteral("\n→ ") + hint;
}

QString OracleConnection::diagnostics() const {
    const Settings s = settings();
    QStringList lines;
    lines << QStringLiteral("Qt %1, application %2 bits (%3)")
                 .arg(QString::fromLatin1(qVersion()))
                 .arg(int(sizeof(void *) * 8))
                 .arg(QSysInfo::buildCpuArchitecture());
    lines << QStringLiteral("Pilotes SQL Qt : %1").arg(QSqlDatabase::drivers().join(QStringLiteral(", ")));
    lines << QStringLiteral("Pilote QODBC : %1").arg(driverAvailable() ? QStringLiteral("disponible") : QStringLiteral("ABSENT"));
    lines << QStringLiteral("DSN : %1 — utilisateur : %2 — mot de passe : %3")
                 .arg(s.dsn, s.user, s.hasPassword ? QStringLiteral("défini") : QStringLiteral("ABSENT"));
    if (isOpen()) {
        QSqlDatabase db = database();
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT banner FROM v$version WHERE ROWNUM = 1")) && q.next())
            lines << QStringLiteral("Serveur : %1").arg(q.value(0).toString());
    }
    return lines.join(QLatin1Char('\n'));
}
