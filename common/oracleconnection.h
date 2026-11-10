#pragma once
#include <QSqlDatabase>
#include <QString>

// Connexion Oracle (ODBC, pilote Qt « QODBC ») partagée par toute l'application.
//
// Singleton : constructeur privé, une seule instance obtenue par instance() / createInstance(),
// copie interdite. Paramètres de l'atelier 2 : source ODBC « Source_Projet2A », utilisateur Iyed.
// On peut les changer avec les variables AUREA_ORACLE_DSN, AUREA_ORACLE_USER, AUREA_ORACLE_PASSWORD.
class OracleConnection {
public:
    static OracleConnection &instance();
    static OracleConnection &createInstance() { return instance(); }   // nom utilisé dans l'atelier
    bool createConnection();                                           // open() + message dans la console

    OracleConnection(const OracleConnection &) = delete;
    OracleConnection &operator=(const OracleConnection &) = delete;

    bool open();                      // ouvre la connexion (rien à faire si elle est déjà ouverte)
    bool isOpen() const;
    void close();                     // ferme, mais open() peut rouvrir
    void shutdown();                  // ferme définitivement (à la fin du programme)
    QSqlDatabase database() const;    // la connexion Qt, à passer aux QSqlQuery
    QString dsn() const;
    QString user() const;
    QString errorMessage() const { return m_message; }   // explication du dernier échec

private:
    OracleConnection() = default;
    static QString describe(const QString &error);
    QString m_message;
};
