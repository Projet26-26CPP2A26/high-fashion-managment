#pragma once
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

// Connexion Oracle (ODBC, pilote Qt « QODBC ») partagée par toute l'application.
//
// Singleton de Meyers : constructeur privé, accès par instance() qui renvoie une référence,
// copie et affectation supprimées. L'objet ne garde AUCUN QSqlDatabase en membre : la connexion
// Qt est retrouvée par son nom à chaque appel, donc aucune référence ne peut rester pendante.
//
// Configuration (variables d'environnement, jamais dans le code ni dans Git) :
//   AUREA_ORACLE_PASSWORD  mot de passe Oracle (OBLIGATOIRE)
//   AUREA_ORACLE_DSN       source de données ODBC        (défaut : Source_Projet2A)
//   AUREA_ORACLE_USER      utilisateur Oracle            (défaut : hsan)
class OracleConnection {
public:
    static OracleConnection &instance();
    // Noms utilisés dans le cours de l'atelier (Singleton « createInstance() », connexion « createConnect() »).
    static OracleConnection &createInstance() { return instance(); }
    bool createConnect() { return open(); }

    OracleConnection(const OracleConnection &) = delete;
    OracleConnection &operator=(const OracleConnection &) = delete;

    struct Settings {
        QString dsn, user;
        bool hasPassword = false;   // le mot de passe lui-même n'est jamais exposé
    };
    static Settings settings();

    // Le pilote QODBC est-il présent dans l'installation de Qt (plugins/sqldrivers) ?
    static bool driverAvailable();
    static QString connectionName();
    // Mode « tout Oracle » (option --oracle) : la connexion Oracle devient la connexion Qt PAR DÉFAUT, utilisée par les modules
    // Clients et Commandes (QSqlQuery sans base explicite). À appeler AVANT le premier open().
    static void useDefaultConnection(bool enable);

    // Ouvre la connexion (sans effet si elle est déjà ouverte). En cas d'échec : errorMessage().
    bool open();
    bool isOpen() const;
    // Ferme la connexion mais la garde enregistrée (open() peut la rouvrir).
    void close();
    // Ferme puis désenregistre la connexion. À appeler une seule fois, quand plus aucun QSqlQuery
    // ni QSqlDatabase issus de database() n'existe (par ex. à la fin de main()).
    void shutdown();

    // Poignée Qt sur la connexion ; invalide tant que open() n'a pas réussi.
    // Ne pas la conserver en membre de classe : rappeler database() au besoin.
    QSqlDatabase database() const;

    QSqlError lastError() const { return m_error; }
    // Message lisible (erreur Qt + piste de résolution) du dernier open() ayant échoué.
    QString errorMessage() const { return m_message; }
    // Rapport de configuration : version de Qt, architecture, pilotes, DSN, utilisateur, version Oracle.
    QString diagnostics() const;

private:
    OracleConnection() = default;
    // Volontairement trivial : pas de removeDatabase() pendant la destruction des statiques
    // (l'ordre de destruction par rapport aux globaux de Qt n'est pas garanti) → voir shutdown().
    ~OracleConnection() = default;

    static QString describe(const QSqlError &error);

    QSqlError m_error;
    QString m_message;
};
