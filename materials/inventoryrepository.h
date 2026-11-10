#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct Material {
    qint64 id = 0;
    QString name, type, color, unit = "m", supplier, lot;
    double quantity = 0, minimum = 0, price = 0;
    bool low() const { return quantity < minimum; }   // sous le seuil : à commander
};

// Accès à la base (SQLite ou Oracle) : table « materials ».
// Chaque fonction qui échoue renvoie false (ou un objet vide) et le message est dans error().
class InventoryRepository {
public:
    InventoryRepository();
    ~InventoryRepository();
    bool open(const QString &path, bool seedDemo = false);   // SQLite (fichier)
    bool openOracle(bool seedDemo = false);                  // Oracle (connexion OracleConnection)
    QVector<Material> all();
    Material find(qint64 id);
    Material findLot(const QString &lot);
    bool save(Material &m);   // ajoute (id = 0) ou modifie
    bool remove(qint64 id);
    bool move(qint64 id, double delta);   // entrée (+) ou sortie (-) de stock
    QString error() const { return m_error; }
    QString path() const { return m_db.databaseName(); }
    static QString validate(const Material &m);   // "" si la matière est valide, sinon le message d'erreur

private:
    QSqlDatabase m_db;
    QString m_connection, m_error;
    bool m_oracle = false;
    bool prepare(bool seedDemo);
    bool fail(const QString &message, bool rollback = false);
    bool seed();
};
