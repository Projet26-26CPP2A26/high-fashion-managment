#include "oraclecheck.h"
#include "oracleconnection.h"
#include <QSqlQuery>
#include <QStringList>

// Exécute un SELECT qui renvoie une seule valeur. ok = false si la requête échoue.
static QString selectValue(const QString &sql, bool &ok)
{
    QSqlQuery q(OracleConnection::instance().database());
    ok = q.exec(sql) && q.next();
    return ok ? q.value(0).toString() : QString();
}

OracleCheck::Report OracleCheck::run()
{
    OracleConnection &oracle = OracleConnection::instance();
    QStringList lines;
    int failed = 0;
    // Ajoute une ligne « [ OK ] » ou « [ ÉCHEC ] » au rapport.
    auto check = [&](bool ok, const QString &text) {
        lines << (ok ? "[ OK ]    " : "[ ÉCHEC ] ") + text;
        if (!ok) ++failed;
        return ok;
    };

    // 1. Connexion
    if (check(oracle.open(), QString("Connexion Oracle (DSN %1, utilisateur %2)").arg(oracle.dsn(), oracle.user()))) {
        bool ok = false;

        // 2. Version du serveur et requête minimale
        const QString version = selectValue("SELECT banner FROM v$version WHERE ROWNUM = 1", ok);
        if (ok) lines << "Serveur : " + version;
        selectValue("SELECT 1 FROM dual", ok);
        check(ok, "SELECT 1 FROM dual");

        // 3. Tables de l'application (créées par oracle/01_schema.sql)
        for (const QString &table : {"materials"}) {
            const QString count = selectValue("SELECT COUNT(*) FROM " + table, ok);
            check(ok, QString("Table %1 : %2 ligne(s)").arg(table, ok ? count : "introuvable, exécutez oracle/01_schema.sql"));
        }

        // 4. Fermeture puis réouverture
        oracle.close();
        check(!oracle.isOpen(), "Fermeture de la connexion");
        check(oracle.open(), "Réouverture de la connexion");
    } else {
        lines << "" << oracle.errorMessage();
    }
    oracle.shutdown();

    lines << "" << (failed == 0 ? QString("RÉSULTAT : tous les contrôles sont passés.")
                                : QString("RÉSULTAT : %1 contrôle(s) en échec.").arg(failed));
    return {failed == 0, lines.join('\n')};
}
