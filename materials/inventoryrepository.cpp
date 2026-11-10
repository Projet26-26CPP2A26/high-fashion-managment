#include "inventoryrepository.h"
#include "oracleconnection.h"
#include "sqldialect.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>
#include <QVariant>
#include <cmath>

namespace {
const QString columns = "id,name,type,color,quantity,minimum,unit,price,supplier,lot";
const double maxValue = 9999999.99;

// Lit une ligne « SELECT columns ... » dans un objet Material (même ordre que « columns »).
Material readMaterial(const QSqlQuery &q)
{
    Material m;
    m.id = q.value(0).toLongLong();
    m.name = q.value(1).toString();
    m.type = q.value(2).toString();
    m.color = q.value(3).toString();
    m.quantity = q.value(4).toDouble();
    m.minimum = q.value(5).toDouble();
    m.unit = q.value(6).toString();
    m.price = q.value(7).toDouble();
    m.supplier = q.value(8).toString();
    m.lot = q.value(9).toString();
    return m;
}

// Arrondi à 2 décimales (centimes, centimètres).
double rounded(double v)
{
    return std::round(v * 100.0) / 100.0;
}

bool isWhole(double v)
{
    return std::abs(v - std::round(v)) < 0.00001;
}
}

// Chaque dépôt a son propre nom de connexion SQLite (plusieurs dépôts peuvent coexister).
InventoryRepository::InventoryRepository() : m_connection(QUuid::createUuid().toString()) {}

InventoryRepository::~InventoryRepository()
{
    m_db = QSqlDatabase();
    if (!m_oracle)   // la connexion Oracle appartient à OracleConnection, on ne la ferme pas ici
        QSqlDatabase::removeDatabase(m_connection);
}

// Enregistre le message d'erreur, annule la transaction si demandé, et renvoie false.
bool InventoryRepository::fail(const QString &message, bool rollback)
{
    m_error = message;
    if (rollback) m_db.rollback();
    return false;
}

// ===================== Ouverture =====================
bool InventoryRepository::open(const QString &path, bool seedDemo)
{
    m_error.clear();
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(path);
    if (!m_db.open()) return fail(m_db.lastError().text());
    return prepare(seedDemo);
}

bool InventoryRepository::openOracle(bool seedDemo)
{
    m_error.clear();
    OracleConnection &oracle = OracleConnection::instance();
    if (!oracle.open()) return fail(oracle.errorMessage());
    m_db = oracle.database();
    m_oracle = true;
    return prepare(seedDemo);
}

// Vérifie (Oracle) ou crée (SQLite) les tables, puis ajoute les exemples si la base est vide.
bool InventoryRepository::prepare(bool seedDemo)
{
    QSqlQuery q(m_db);
    if (m_oracle) {
        // Avec Oracle, les tables sont créées par le script oracle/01_schema.sql.
        if (!q.exec("SELECT COUNT(*) FROM materials"))
            return fail("Table Oracle introuvable (materials) : exécutez oracle/01_schema.sql. " + q.lastError().text());
    } else {
        const QStringList schema = {
            "PRAGMA foreign_keys=ON",
            "CREATE TABLE IF NOT EXISTS materials (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, type TEXT NOT NULL,"
            " color TEXT NOT NULL, quantity REAL NOT NULL CHECK(quantity>=0), minimum REAL NOT NULL CHECK(minimum>=0),"
            " unit TEXT NOT NULL, price REAL NOT NULL CHECK(price>=0), supplier TEXT NOT NULL, lot TEXT NOT NULL UNIQUE COLLATE NOCASE)",
        };
        for (const QString &sql : schema)
            if (!q.exec(sql)) return fail(q.lastError().text());
    }

    if (seedDemo) {
        if (!q.exec("SELECT COUNT(*) FROM materials") || !q.next()) return fail(q.lastError().text());
        if (q.value(0).toInt() == 0) return seed();
    }
    return true;
}

// ===================== Vérification d'une matière =====================
QString InventoryRepository::validate(const Material &m)
{
    if (m.name.trimmed().isEmpty() || m.type.trimmed().isEmpty() || m.color.trimmed().isEmpty()
        || m.supplier.trimmed().isEmpty() || m.lot.trimmed().isEmpty())
        return "Renseignez le nom, le type, la couleur, le fournisseur et le lot.";
    if (m.name.size() > 120 || m.supplier.size() > 120 || m.color.size() > 60 || m.lot.size() > 80)
        return "Nom et fournisseur : 120 caractères maximum ; couleur : 60 ; lot : 80.";
    for (double v : {m.quantity, m.minimum, m.price})
        if (!std::isfinite(v) || v < 0 || v > maxValue)   // isfinite : refuse « NaN » et l'infini
            return "Quantité, seuil et prix doivent être compris entre 0 et 9 999 999,99.";
    if (!QStringList{"m", "pièce", "bobine", "kg"}.contains(m.unit))
        return "Choisissez une unité valide.";
    if ((m.unit == "pièce" || m.unit == "bobine") && (!isWhole(m.quantity) || !isWhole(m.minimum)))
        return "Les pièces et les bobines doivent avoir des quantités et des seuils entiers.";
    for (const QChar c : m.lot)   // le lot est lu dans les QR : pas d'espace ni de « : »
        if (c.isSpace() || c == ':' || !c.isPrint())
            return "Le lot ne doit contenir ni espace, ni deux-points, ni caractère de contrôle.";
    return "";
}

// ===================== Lecture =====================
QVector<Material> InventoryRepository::all()
{
    m_error.clear();
    QVector<Material> result;
    QSqlQuery q(m_db);
    const QString order = m_oracle ? " ORDER BY UPPER(name)" : " ORDER BY name COLLATE NOCASE";
    if (!q.exec("SELECT " + columns + " FROM materials" + order)) {
        fail(q.lastError().text());
        return result;
    }
    while (q.next())
        result.push_back(readMaterial(q));
    return result;
}

Material InventoryRepository::find(qint64 id)
{
    m_error.clear();
    QSqlQuery q(m_db);
    q.prepare("SELECT " + columns + " FROM materials WHERE id=?");
    q.addBindValue(SqlDialect::id(id));
    if (!q.exec()) {
        fail(q.lastError().text());
        return {};
    }
    return q.next() ? readMaterial(q) : Material{};
}

// Recherche par lot sans tenir compte des majuscules.
Material InventoryRepository::findLot(const QString &lot)
{
    m_error.clear();
    QSqlQuery q(m_db);
    const QString where = m_oracle ? " WHERE UPPER(lot)=UPPER(?)" : " WHERE lot=? COLLATE NOCASE";
    q.prepare("SELECT " + columns + " FROM materials" + where);
    q.addBindValue(lot.trimmed());
    if (!q.exec()) {
        fail(q.lastError().text());
        return {};
    }
    return q.next() ? readMaterial(q) : Material{};
}

// ===================== Écriture =====================

// Ajoute (m.id == 0) ou modifie une matière.
// Tout se fait dans une transaction : en cas d'erreur, rien n'est enregistré.
bool InventoryRepository::save(Material &m)
{
    m_error.clear();
    m.name = m.name.trimmed();
    m.color = m.color.trimmed();
    m.supplier = m.supplier.trimmed();
    m.lot = m.lot.trimmed();
    const QString problem = validate(m);
    if (!problem.isEmpty()) return fail(problem);
    m.quantity = rounded(m.quantity);
    m.minimum = rounded(m.minimum);
    m.price = rounded(m.price);

    if (!m_db.transaction()) return fail(m_db.lastError().text());
    const bool updating = m.id != 0;
    const Material old = updating ? find(m.id) : Material{};
    if (updating && !old.id)
        return fail("Cette matière n’existe plus.", true);
    if (updating && old.unit != m.unit)
        return fail("L’unité d’un lot enregistré ne peut pas être changée. Créez un nouveau lot.", true);

    QSqlQuery q(m_db);
    if (updating)
        q.prepare("UPDATE materials SET name=?,type=?,color=?,quantity=?,minimum=?,unit=?,price=?,supplier=?,lot=? WHERE id=?");
    else
        q.prepare("INSERT INTO materials(name,type,color,quantity,minimum,unit,price,supplier,lot) VALUES(?,?,?,?,?,?,?,?,?)");
    for (const QVariant &v : QVariantList{m.name, m.type, m.color, m.quantity, m.minimum, m.unit, m.price, m.supplier, m.lot})
        q.addBindValue(v);
    if (updating) q.addBindValue(SqlDialect::id(m.id));
    if (!q.exec()) {
        const QString error = q.lastError().text();
        const bool duplicate = error.contains("UNIQUE") || error.contains("ORA-00001");   // lot déjà utilisé
        return fail(duplicate ? "Cet identifiant de lot existe déjà. Choisissez un lot unique." : error, true);
    }

    if (!updating) m.id = SqlDialect::lastId(m_db, q, "seq_materials");
    if (!m_db.commit()) {
        if (!updating) m.id = 0;
        return fail(m_db.lastError().text(), true);
    }
    return true;
}

bool InventoryRepository::remove(qint64 id)
{
    m_error.clear();
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM materials WHERE id=?");
    q.addBindValue(SqlDialect::id(id));
    if (!q.exec()) return fail(q.lastError().text());
    if (q.numRowsAffected() != 1) return fail("Cette matière n’existe plus.");
    return true;
}

// Entrée (delta > 0) ou sortie (delta < 0) de stock.
bool InventoryRepository::move(qint64 id, double delta)
{
    m_error.clear();
    if (!std::isfinite(delta) || std::abs(delta) < 0.005 || std::abs(delta) > maxValue)
        return fail("Indiquez une quantité positive.");
    delta = rounded(delta);

    if (!m_db.transaction()) return fail(m_db.lastError().text());
    Material m = find(id);
    if (!m.id) return fail("Ce lot n’existe plus.", true);
    m.quantity = rounded(m.quantity + delta);
    const QString problem = validate(m);
    if (!problem.isEmpty())
        return fail(m.quantity < 0 ? "Stock insuffisant pour cette sortie." : problem, true);

    QSqlQuery q(m_db);
    q.prepare("UPDATE materials SET quantity=? WHERE id=?");
    q.addBindValue(m.quantity);
    q.addBindValue(SqlDialect::id(id));
    if (!q.exec()) return fail(q.lastError().text(), true);
    if (!m_db.commit()) return fail(m_db.lastError().text(), true);
    return true;
}

// Matières d'exemple, ajoutées seulement si la base est vide.
bool InventoryRepository::seed()
{
    const QVector<Material> samples = {
        // id, nom, type, couleur, unité, fournisseur, lot, quantité, seuil, prix
        {0, "Soie duchesse", "Tissu", "Ivoire", "m", "Maison des tissus", "SOI-2601", 42.5, 15, 85},
        {0, "Dentelle de Calais", "Dentelle", "Champagne", "m", "Dentelles Prestige", "DEN-2602", 6, 10, 64},
        {0, "Fil de soie", "Fil", "Or", "bobine", "Mercerie Élégance", "FIL-2603", 18, 8, 12.5},
        {0, "Bouton nacré", "Bouton", "Perle", "pièce", "Mercerie Élégance", "BTN-2604", 80, 30, 2.4},
        {0, "Satin de soie", "Tissu", "Bordeaux", "m", "Maison des tissus", "SAT-2605", 8, 12, 48},
        {0, "Fermeture invisible", "Fermeture", "Noir", "pièce", "Zip Couture", "ZIP-2606", 0, 15, 3.8},
        {0, "Doublure viscose", "Doublure", "Ivoire", "m", "Textile Méditerranée", "DBL-2607", 55, 20, 9.5},
        {0, "Ruban de velours", "Accessoire", "Bordeaux", "m", "Mercerie Élégance", "RUB-2608", 24, 10, 6.2},
    };
    for (Material m : samples)
        if (!save(m)) return false;
    return true;
}
