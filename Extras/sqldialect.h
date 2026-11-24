#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

// Petites différences entre SQLite et Oracle (pilote « QODBC »).
namespace SqlDialect {

inline bool isOracle(const QSqlDatabase &db) { return db.driverName() == "QODBC"; }

// Le pilote Oracle refuse les entiers 64 bits (qint64) : les identifiants sont envoyés en int.
inline QVariant id(qint64 value) { return QVariant(int(value)); }

// Identifiant créé par le dernier INSERT.
// SQLite : lastInsertId(). Oracle : lastInsertId() est vide, on lit la séquence (SEQUENCE.CURRVAL).
inline qint64 lastId(const QSqlDatabase &db, const QSqlQuery &insert, const QString &sequence)
{
    if (!isOracle(db)) return insert.lastInsertId().toLongLong();
    QSqlQuery q(db);
    if (q.exec("SELECT " + sequence + ".CURRVAL FROM dual") && q.next())
        return q.value(0).toLongLong();
    return 0;
}

} // namespace SqlDialect
