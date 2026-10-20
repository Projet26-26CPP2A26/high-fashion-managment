#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct Material {
    qint64 id = 0;
    QString name, type, color, unit = QStringLiteral("m"), supplier, lot;
    double quantity = 0, minimum = 0, price = 0;
    bool low() const { return quantity < minimum; }
};

struct Movement {
    QString date, name, lot, kind, unit, reason;
    double delta = 0;
};

class InventoryRepository {
public:
    InventoryRepository();
    ~InventoryRepository();
    bool open(const QString &path, bool seedDemo = false);
    bool openOracle(bool seedDemo = false);   // connexion Oracle du singleton OracleConnection ; schéma créé par oracle/01_schema.sql
    QVector<Material> all();
    QVector<Movement> history();
    Material find(qint64 id);
    Material findLot(const QString &lot);
    bool save(Material &m, const QString &reason);
    bool remove(qint64 id);
    bool move(qint64 id, double delta, const QString &reason);
    QString error() const { return m_error; }
    QString path() const { return m_db.databaseName(); }
    QSqlDatabase database() const { return m_db; }
    bool demonstration() const { return m_demo; }
    static QString validate(const Material &m);
private:
    QSqlDatabase m_db;
    QString m_connection, m_error;
    bool m_demo = false;
    bool m_oracle = false;
    bool prepare(bool seedDemo);   // schéma (SQLite) ou vérification (Oracle), puis exemples
    bool recordMovement(const Material &m, double delta, const QString &kind, const QString &reason);
    bool fail(const QString &message, bool rollback = false);
    bool seed();
};
