#pragma once
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

// Différences SQLite / Oracle (via QODBC) partagées par les modules migrés (Employés, Production).
// Le dialecte se déduit du pilote de la connexion : « QODBC » = Oracle, sinon SQLite.
// Comportement SQLite inchangé : dates en texte ISO, identifiants lus par lastInsertId().
namespace SqlDialect {

inline bool isOracle(const QSqlDatabase &db) { return db.driverName() == QLatin1String("QODBC"); }

// Le pilote Oracle 11g refuse les entiers 64 bits liés (qint64) : les identifiants sont liés en int.
inline QVariant id(qint64 value) { return QVariant(int(value)); }

// NULL typé : un QVariant() sans type est lié en BINARY par QODBC et Oracle le refuse (ORA-00932).
inline QVariant nullId() { return QVariant(QMetaType::fromType<int>()); }
inline QVariant nullText() { return QVariant(QMetaType::fromType<QString>()); }

// SQLite : texte yyyy-MM-dd ; Oracle : QDate lié à une colonne DATE.
inline QVariant date(const QSqlDatabase &db, const QDate &value) {
    return isOracle(db) ? QVariant(value) : QVariant(value.toString(Qt::ISODate));
}

// SQLite : texte ISO ; Oracle : QDateTime lié à une colonne TIMESTAMP.
inline QVariant dateTime(const QSqlDatabase &db, const QDateTime &value) {
    return isOracle(db) ? QVariant(value) : QVariant(value.toString(Qt::ISODate));
}

// Date lue en base, au format ISO yyyy-MM-dd : SQLite renvoie du texte ISO, Oracle un QDateTime (colonne DATE).
inline QString isoDate(const QVariant &value) {
    const QDate d = value.toDate();
    return d.isValid() ? d.toString(Qt::ISODate) : value.toString();
}

// Identifiant généré par le dernier INSERT : lastInsertId() (SQLite) ou SEQUENCE.CURRVAL (Oracle,
// lastInsertId() est vide avec QODBC). À appeler sur la même connexion, juste après l'INSERT.
inline qint64 lastId(const QSqlDatabase &db, const QSqlQuery &insert, const char *sequence) {
    if (!isOracle(db)) return insert.lastInsertId().toLongLong();
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT ") + QLatin1String(sequence) + QStringLiteral(".CURRVAL FROM dual")) && q.next())
        return q.value(0).toLongLong();
    return 0;
}

}   // namespace SqlDialect
