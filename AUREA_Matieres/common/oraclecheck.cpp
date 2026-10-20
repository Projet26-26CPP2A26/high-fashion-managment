#include "oraclecheck.h"
#include "oracleconnection.h"
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <cmath>

namespace {

struct Log {
    QStringList lines;
    int failed = 0;
    void pass(const QString &text) { lines << QStringLiteral("[ OK ]    ") + text; }
    void fail(const QString &text) { lines << QStringLiteral("[ ÉCHEC ] ") + text; ++failed; }
    void info(const QString &text) { lines << QStringLiteral("[ info ]  ") + text; }
    bool check(bool condition, const QString &text) { condition ? pass(text) : fail(text); return condition; }
};

// Exécute une instruction sans paramètre ; renvoie false et remplit « error » en cas d'échec.
bool exec(QSqlDatabase &db, const QString &sql, QString *error = nullptr) {
    QSqlQuery q(db);
    if (q.exec(sql)) return true;
    if (error) *error = q.lastError().text();
    return false;
}

// Exécute une instruction et consigne le résultat (l'erreur est lue APRÈS l'exécution : ordre d'évaluation garanti).
bool step(QSqlDatabase &db, Log &log, const QString &sql, const QString &title) {
    QString error;
    const bool ok = exec(db, sql, &error);
    if (ok) log.pass(title); else log.fail(title + QStringLiteral(" : ") + error);
    return ok;
}

// Valeur unique d'un SELECT (ou variant invalide en cas d'échec).
QVariant scalar(QSqlDatabase &db, const QString &sql, const QVariant &bind = QVariant()) {
    QSqlQuery q(db);
    q.prepare(sql);
    if (bind.isValid()) q.addBindValue(bind);
    if (!q.exec() || !q.next()) return QVariant();
    return q.value(0);
}

void dropScratch(QSqlDatabase &db) {
    // Les erreurs « objet inexistant » sont normales ici.
    exec(db, QStringLiteral("DROP TABLE AUREA_TST_CHILD PURGE"));
    exec(db, QStringLiteral("DROP TABLE AUREA_TST_PARENT PURGE"));
    exec(db, QStringLiteral("DROP SEQUENCE AUREA_TST_SEQ"));
}

void runSteps(QSqlDatabase &db, Log &log) {
    QString error;

    // 1. Requête minimale
    log.check(scalar(db, QStringLiteral("SELECT 1 FROM dual")).toInt() == 1, QStringLiteral("SELECT 1 FROM dual"));

    // 2. Objets temporaires : séquence + table parente + table fille avec clé étrangère
    dropScratch(db);
    if (!step(db, log, QStringLiteral("CREATE SEQUENCE AUREA_TST_SEQ START WITH 1 INCREMENT BY 1 NOCACHE"),
              QStringLiteral("CREATE SEQUENCE (droit CREATE SEQUENCE requis)"))) return;
    if (!step(db, log, QStringLiteral(
            "CREATE TABLE AUREA_TST_PARENT (id NUMBER(10) PRIMARY KEY, label VARCHAR2(100 CHAR) NOT NULL, "
            "amount NUMBER(12,2), born DATE, created TIMESTAMP, note VARCHAR2(100 CHAR))"),
              QStringLiteral("CREATE TABLE parent (droit CREATE TABLE requis)"))) return;
    if (!step(db, log, QStringLiteral(
            "CREATE TABLE AUREA_TST_CHILD (id NUMBER(10) PRIMARY KEY, "
            "parent_id NUMBER(10) NOT NULL REFERENCES AUREA_TST_PARENT(id))"),
              QStringLiteral("CREATE TABLE enfant avec clé étrangère"))) return;

    // 3. INSERT avec valeurs liées : texte accentué, nombre, DATE, TIMESTAMP, NULL ; identifiant par séquence
    const QString label = QStringLiteral("Dentelle de Calais — Élégance d'été");
    const QDate born(1992, 3, 14);
    const QDateTime created(QDate(2026, 10, 14), QTime(14, 36, 5));
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO AUREA_TST_PARENT(id,label,amount,born,created,note) VALUES(AUREA_TST_SEQ.NEXTVAL,?,?,?,?,?)"));
        q.addBindValue(label);
        q.addBindValue(1234.56);
        q.addBindValue(born);
        q.addBindValue(created);
        q.addBindValue(QVariant());   // NULL
        const bool inserted = q.exec();
        log.check(inserted, QStringLiteral("INSERT avec valeurs liées (texte, nombre, date, horodatage, NULL)")
                                + (inserted ? QString() : QStringLiteral(" : ") + q.lastError().text()));
        if (!inserted) return;
        log.info(QStringLiteral("numRowsAffected() = %1 ; lastInsertId() %2")
                     .arg(q.numRowsAffected())
                     .arg(q.lastInsertId().isValid() ? QStringLiteral("renvoie une valeur")
                                                     : QStringLiteral("est vide avec Oracle/ODBC : on lit SEQUENCE.CURRVAL à la place")));
    }
    const qlonglong id = scalar(db, QStringLiteral("SELECT AUREA_TST_SEQ.CURRVAL FROM dual")).toLongLong();
    if (!log.check(id > 0, QStringLiteral("Identifiant généré par la séquence (CURRVAL) = %1").arg(id))) return;

    // 4a. Liaison d'un entier 64 bits (qint64, utilisé pour les identifiants dans l'application)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM AUREA_TST_PARENT WHERE id=?"));
        q.addBindValue(id);
        const bool ok = q.exec() && q.next();
        const QString why = ok ? QString() : QStringLiteral(" : ") + q.lastError().text();
        const bool found = ok && q.value(0).toInt() == 1;
        // Limite connue du pilote « Oracle in XE » (11g) : information, pas un échec de l'installation.
        log.info(found ? QStringLiteral("Liaison d'un entier 64 bits (qlonglong) : acceptée par le pilote")
                       : QStringLiteral("Liaison d'un entier 64 bits (qlonglong) REFUSÉE par le pilote Oracle 11g%1 : "
                                        "les identifiants qint64 devront être liés en int (32 bits)").arg(why));
    }
    const int key = int(id);   // identifiants liés en int dans la suite

    // 4b. SELECT et relecture : aller-retour exact des types
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT label,amount,born,created,note FROM AUREA_TST_PARENT WHERE id=?"));
        q.addBindValue(key);
        const bool selected = q.exec() && q.next();
        const QString why = selected ? QString() : QStringLiteral(" : ") + q.lastError().text();
        if (!log.check(selected, QStringLiteral("SELECT … WHERE id = ? (entier 32 bits)") + why)) return;
        log.check(q.value(0).toString() == label, QStringLiteral("Texte accentué relu à l'identique"));
        log.check(std::abs(q.value(1).toDouble() - 1234.56) < 1e-9, QStringLiteral("Nombre décimal relu à l'identique"));
        log.check(q.value(2).toDate() == born, QStringLiteral("DATE relue à l'identique (%1)").arg(q.value(2).toDate().toString(Qt::ISODate)));
        log.check(q.value(3).toDateTime() == created, QStringLiteral("TIMESTAMP relu à l'identique (%1)").arg(q.value(3).toDateTime().toString(Qt::ISODate)));
        log.check(q.value(4).isNull(), QStringLiteral("NULL relu comme NULL"));
    }

    // 5. Chaîne vide : information, pas un échec (comportement documenté d'Oracle)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE AUREA_TST_PARENT SET note=? WHERE id=?"));
        q.addBindValue(QString(""));
        q.addBindValue(key);
        if (q.exec()) {
            const QVariant back = scalar(db, QStringLiteral("SELECT note FROM AUREA_TST_PARENT WHERE id=?"), key);
            log.info(QStringLiteral("Chaîne vide enregistrée puis relue : %1")
                         .arg(back.isNull() ? QStringLiteral("NULL (Oracle assimile '' à NULL : à prévoir pour les colonnes NOT NULL)")
                                            : QStringLiteral("chaîne vide")));
        }
    }

    // 6. UPDATE : nombre de lignes touchées
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE AUREA_TST_PARENT SET amount=? WHERE id=?"));
        q.addBindValue(99.5);
        q.addBindValue(key);
        const bool updated = q.exec();
        const int rows = q.numRowsAffected();
        log.check(updated && rows == 1, QStringLiteral("UPDATE : une ligne modifiée (numRowsAffected = %1)").arg(rows));
    }

    // 7. Clé étrangère
    {
        QSqlQuery ok(db);
        ok.prepare(QStringLiteral("INSERT INTO AUREA_TST_CHILD(id,parent_id) VALUES(AUREA_TST_SEQ.NEXTVAL,?)"));
        ok.addBindValue(key);
        log.check(ok.exec(), QStringLiteral("Clé étrangère : insertion d'un enfant valide"));
    }
    {
        QSqlQuery bad(db);
        bad.prepare(QStringLiteral("INSERT INTO AUREA_TST_CHILD(id,parent_id) VALUES(AUREA_TST_SEQ.NEXTVAL,?)"));
        bad.addBindValue(999999999);
        const bool refused = !bad.exec();
        log.check(refused && bad.lastError().text().contains(QLatin1String("ORA-02291")),
                  QStringLiteral("Clé étrangère : parent inexistant refusé (ORA-02291)")
                      + (refused ? QString() : QStringLiteral(" — l'insertion a été acceptée !")));
    }
    {
        QSqlQuery del(db);
        del.prepare(QStringLiteral("DELETE FROM AUREA_TST_PARENT WHERE id=?"));
        del.addBindValue(key);
        const bool refused = !del.exec();
        log.check(refused && del.lastError().text().contains(QLatin1String("ORA-02292")),
                  QStringLiteral("Clé étrangère : suppression du parent référencé refusée (ORA-02292)")
                      + (refused ? QString() : QStringLiteral(" — la suppression a été acceptée !")));
    }

    // 8. Transactions : ROLLBACK annule, COMMIT conserve
    const bool transactional = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    if (!log.check(transactional, QStringLiteral("Le pilote déclare le support des transactions"))) return;
    const auto insertTagged = [&](const QString &tag) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO AUREA_TST_PARENT(id,label) VALUES(AUREA_TST_SEQ.NEXTVAL,?)"));
        q.addBindValue(tag);
        return q.exec();
    };
    const auto countTagged = [&](const QString &tag) {
        return scalar(db, QStringLiteral("SELECT COUNT(*) FROM AUREA_TST_PARENT WHERE label=?"), tag).toInt();
    };
    log.check(db.transaction() && insertTagged(QStringLiteral("TX-ROLLBACK")) && db.rollback() && countTagged(QStringLiteral("TX-ROLLBACK")) == 0,
              QStringLiteral("Transaction : ROLLBACK annule l'insertion"));
    log.check(db.transaction() && insertTagged(QStringLiteral("TX-COMMIT")) && db.commit() && countTagged(QStringLiteral("TX-COMMIT")) == 1,
              QStringLiteral("Transaction : COMMIT conserve l'insertion"));

    // 9. DELETE : d'abord l'enfant, puis les parents
    {
        QSqlQuery q(db);
        log.check(q.exec(QStringLiteral("DELETE FROM AUREA_TST_CHILD")) && q.numRowsAffected() == 1, QStringLiteral("DELETE de l'enfant"));
        QSqlQuery p(db);
        log.check(p.exec(QStringLiteral("DELETE FROM AUREA_TST_PARENT")) && p.numRowsAffected() == 2, QStringLiteral("DELETE des parents (2 lignes attendues)"));
    }
}

}   // namespace

namespace OracleCheck {

Report run() {
    Log log;
    OracleConnection &oracle = OracleConnection::instance();
    const OracleConnection::Settings s = OracleConnection::settings();

    if (!oracle.open()) {
        log.fail(QStringLiteral("Connexion Oracle (DSN %1, utilisateur %2)").arg(s.dsn, s.user));
        log.lines << QString() << oracle.errorMessage() << QString() << oracle.diagnostics();
        Report report;
        report.ok = false;
        report.text = log.lines.join(QLatin1Char('\n'));
        oracle.shutdown();
        return report;
    }
    log.pass(QStringLiteral("Connexion Oracle ouverte (DSN %1, utilisateur %2)").arg(s.dsn, s.user));
    log.lines << oracle.diagnostics().split(QLatin1Char('\n'));

    {   // portée : la poignée et les requêtes sont détruites avant shutdown()
        QSqlDatabase db = oracle.database();
        runSteps(db, log);
        dropScratch(db);   // nettoyage, même après un échec
    }

    oracle.close();
    log.check(!oracle.isOpen(), QStringLiteral("Fermeture de la connexion"));
    log.check(oracle.open() && oracle.isOpen(), QStringLiteral("Réouverture de la connexion après fermeture"));
    oracle.shutdown();

    Report report;
    report.ok = log.failed == 0;
    log.lines << QString() << (report.ok ? QStringLiteral("RÉSULTAT : tous les contrôles sont passés.")
                                          : QStringLiteral("RÉSULTAT : %1 contrôle(s) en échec.").arg(log.failed));
    report.text = log.lines.join(QLatin1Char('\n'));
    return report;
}

}   // namespace OracleCheck
