#include "inventoryrepository.h"
#include "oracleconnection.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <cmath>

static double rounded(double value)
{
    return std::round(value * 100.0) / 100.0;
}

static Material readMaterial(const QSqlQuery &query)
{
    Material m;
    m.setId(query.value(0).toInt());
    m.setName(query.value(1).toString());
    m.setType(query.value(2).toString());
    m.setColor(query.value(3).toString());
    m.setQuantity(query.value(4).toDouble());
    m.setMinimum(query.value(5).toDouble());
    m.setUnit(query.value(6).toString());
    m.setPrice(query.value(7).toDouble());
    m.setSupplier(query.value(8).toString());
    m.setLot(query.value(9).toString());
    return m;
}

Material::Material()
    : m_id(0), m_unit("m"), m_quantity(0), m_minimum(0), m_price(0)
{
}

int Material::id() const { return m_id; }
QString Material::name() const { return m_name; }
QString Material::type() const { return m_type; }
QString Material::color() const { return m_color; }
QString Material::unit() const { return m_unit; }
QString Material::supplier() const { return m_supplier; }
QString Material::lot() const { return m_lot; }
double Material::quantity() const { return m_quantity; }
double Material::minimum() const { return m_minimum; }
double Material::price() const { return m_price; }
bool Material::low() const { return m_quantity < m_minimum; }
// Remonter au double du seuil, pour ne pas repasser en alerte juste après la livraison.
double Material::toOrder() const { return low() ? rounded(2 * m_minimum - m_quantity) : 0; }

void Material::setId(int value) { m_id = value; }
void Material::setName(const QString &value) { m_name = value; }
void Material::setType(const QString &value) { m_type = value; }
void Material::setColor(const QString &value) { m_color = value; }
void Material::setUnit(const QString &value) { m_unit = value; }
void Material::setSupplier(const QString &value) { m_supplier = value; }
void Material::setLot(const QString &value) { m_lot = value; }
void Material::setQuantity(double value) { m_quantity = value; }
void Material::setMinimum(double value) { m_minimum = value; }
void Material::setPrice(double value) { m_price = value; }

InventoryRepository::InventoryRepository()
{
}

QString InventoryRepository::error() const
{
    return m_error;
}

bool InventoryRepository::fail(const QString &message)
{
    m_error = message;
    return false;
}

QString InventoryRepository::validate(const Material &m)
{
    if (m.name().trimmed().isEmpty() || m.type().trimmed().isEmpty() || m.color().trimmed().isEmpty()
        || m.supplier().trimmed().isEmpty() || m.lot().trimmed().isEmpty())
        return "Renseignez le nom, le type, la couleur, le fournisseur et le lot.";
    if (m.name().size() > 120 || m.supplier().size() > 120 || m.color().size() > 60 || m.lot().size() > 80)
        return "Nom et fournisseur : 120 caractères maximum ; couleur : 60 ; lot : 80.";
    if (!std::isfinite(m.quantity()) || !std::isfinite(m.minimum()) || !std::isfinite(m.price())
        || m.quantity() < 0 || m.minimum() < 0 || m.price() < 0
        || m.quantity() > 9999999.99 || m.minimum() > 9999999.99 || m.price() > 9999999.99)
        return "Quantité, seuil et prix doivent être compris entre 0 et 9 999 999,99.";
    const QStringList units = {"m", "pièce", "bobine", "kg"};
    if (!units.contains(m.unit()))
        return "Choisissez une unité valide.";
    if (m.unit() == "pièce" || m.unit() == "bobine") {
        if (std::abs(m.quantity() - std::round(m.quantity())) > 0.00001
            || std::abs(m.minimum() - std::round(m.minimum())) > 0.00001)
            return "Les pièces et les bobines doivent avoir des quantités et des seuils entiers.";
    }
    const QString lot = m.lot();
    for (const QChar c : lot) {
        if (c.isSpace() || c == ':' || !c.isPrint())
            return "Le lot ne doit contenir ni espace, ni deux-points, ni caractère de contrôle.";
    }
    return QString();
}

bool InventoryRepository::lotUsed(const QString &lot, int exceptId)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM materials WHERE UPPER(lot) = UPPER(?) AND id <> ?");
    query.addBindValue(lot);
    query.addBindValue(exceptId);
    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

QVector<Material> InventoryRepository::loadMaterials()
{
    m_error.clear();
    QVector<Material> list;
    QSqlQuery query;
    if (!query.exec("SELECT id, name, type, color, quantity, minimum, unit, price, supplier, lot "
                    "FROM materials ORDER BY UPPER(name)")) {
        fail(query.lastError().text());
        return list;
    }
    while (query.next())
        list.append(readMaterial(query));
    return list;
}

Material InventoryRepository::findMaterial(int id)
{
    m_error.clear();
    QSqlQuery query;
    query.prepare("SELECT id, name, type, color, quantity, minimum, unit, price, supplier, lot "
                  "FROM materials WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        fail(query.lastError().text());
        return Material();
    }
    if (query.next())
        return readMaterial(query);
    return Material();
}

Material InventoryRepository::findLot(const QString &lot)
{
    m_error.clear();
    QSqlQuery query;
    query.prepare("SELECT id, name, type, color, quantity, minimum, unit, price, supplier, lot "
                  "FROM materials WHERE UPPER(lot) = UPPER(?)");
    query.addBindValue(lot.trimmed());
    if (!query.exec()) {
        fail(query.lastError().text());
        return Material();
    }
    if (query.next())
        return readMaterial(query);
    return Material();
}

bool InventoryRepository::addMaterial(Material &material)
{
    m_error.clear();
    const QString problem = validate(material);
    if (!problem.isEmpty())
        return fail(problem);
    if (lotUsed(material.lot().trimmed(), 0))
        return fail("Cet identifiant de lot existe déjà. Choisissez un lot unique.");

    material.setId(OracleConnection::nextId("materials"));
    QSqlQuery query;
    query.prepare("INSERT INTO materials(id, name, type, color, quantity, minimum, unit, price, supplier, lot) "
                  "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(material.id());
    query.addBindValue(material.name().trimmed());
    query.addBindValue(material.type());
    query.addBindValue(material.color().trimmed());
    query.addBindValue(rounded(material.quantity()));
    query.addBindValue(rounded(material.minimum()));
    query.addBindValue(material.unit());
    query.addBindValue(rounded(material.price()));
    query.addBindValue(material.supplier().trimmed());
    query.addBindValue(material.lot().trimmed());
    if (!query.exec())
        return fail(query.lastError().text());
    return true;
}

bool InventoryRepository::updateMaterial(const Material &material)
{
    m_error.clear();
    const QString problem = validate(material);
    if (!problem.isEmpty())
        return fail(problem);
    const Material old = findMaterial(material.id());
    if (old.id() == 0)
        return fail("Cette matière n’existe plus.");
    if (old.unit() != material.unit())
        return fail("L’unité d’un lot enregistré ne peut pas être changée. Créez un nouveau lot.");
    if (lotUsed(material.lot().trimmed(), material.id()))
        return fail("Cet identifiant de lot existe déjà. Choisissez un lot unique.");

    QSqlQuery query;
    query.prepare("UPDATE materials SET name = ?, type = ?, color = ?, quantity = ?, minimum = ?, unit = ?, "
                  "price = ?, supplier = ?, lot = ? WHERE id = ?");
    query.addBindValue(material.name().trimmed());
    query.addBindValue(material.type());
    query.addBindValue(material.color().trimmed());
    query.addBindValue(rounded(material.quantity()));
    query.addBindValue(rounded(material.minimum()));
    query.addBindValue(material.unit());
    query.addBindValue(rounded(material.price()));
    query.addBindValue(material.supplier().trimmed());
    query.addBindValue(material.lot().trimmed());
    query.addBindValue(material.id());
    if (!query.exec())
        return fail(query.lastError().text());
    return true;
}

bool InventoryRepository::deleteMaterial(int id)
{
    m_error.clear();
    QSqlQuery query;
    query.prepare("DELETE FROM materials WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec())
        return fail(query.lastError().text());
    if (query.numRowsAffected() == 0)
        return fail("Cette matière n’existe plus.");
    return true;
}

bool InventoryRepository::changeQuantity(int id, double delta)
{
    m_error.clear();
    if (!std::isfinite(delta) || std::abs(delta) < 0.005 || std::abs(delta) > 9999999.99)
        return fail("Indiquez une quantité positive.");
    delta = rounded(delta);
    Material material = findMaterial(id);
    if (material.id() == 0)
        return fail("Ce lot n’existe plus.");
    material.setQuantity(rounded(material.quantity() + delta));
    if (material.quantity() < 0)
        return fail("Stock insuffisant pour cette sortie.");
    const QString problem = validate(material);
    if (!problem.isEmpty())
        return fail(problem);

    QSqlQuery query;
    query.prepare("UPDATE materials SET quantity = ? WHERE id = ?");
    query.addBindValue(material.quantity());
    query.addBindValue(id);
    if (!query.exec())
        return fail(query.lastError().text());
    return true;
}
