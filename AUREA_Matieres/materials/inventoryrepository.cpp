#include "inventoryrepository.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <cmath>

namespace {
Material readMaterial(const QSqlQuery &q) {
    Material m;
    m.id=q.value(0).toLongLong(); m.name=q.value(1).toString(); m.type=q.value(2).toString();
    m.color=q.value(3).toString(); m.quantity=q.value(4).toDouble(); m.minimum=q.value(5).toDouble();
    m.unit=q.value(6).toString(); m.price=q.value(7).toDouble(); m.supplier=q.value(8).toString(); m.lot=q.value(9).toString();
    return m;
}
const QString columns=QStringLiteral("id,name,type,color,quantity,minimum,unit,price,supplier,lot");
double rounded(double v) { return std::round(v * 100.0) / 100.0; }
}

InventoryRepository::InventoryRepository() : m_connection(QUuid::createUuid().toString()) {}
InventoryRepository::~InventoryRepository() {
    m_db.close(); m_db=QSqlDatabase(); QSqlDatabase::removeDatabase(m_connection);
}
bool InventoryRepository::fail(const QString &message, bool rollback) {
    m_error=message; if(rollback) m_db.rollback(); return false;
}
bool InventoryRepository::open(const QString &path, bool seedDemo) {
    m_demo=seedDemo;
    m_error.clear();
    m_db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),m_connection); m_db.setDatabaseName(path);
    if(!m_db.open()) return fail(m_db.lastError().text());
    QSqlQuery q(m_db);
    for(const QString &sql : {
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA busy_timeout=5000"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS materials (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,type TEXT NOT NULL,color TEXT NOT NULL,quantity REAL NOT NULL CHECK(quantity>=0),minimum REAL NOT NULL CHECK(minimum>=0),unit TEXT NOT NULL,price REAL NOT NULL CHECK(price>=0),supplier TEXT NOT NULL,lot TEXT NOT NULL UNIQUE COLLATE NOCASE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS movements (id INTEGER PRIMARY KEY AUTOINCREMENT,material_id INTEGER REFERENCES materials(id) ON DELETE SET NULL,date TEXT NOT NULL,name TEXT NOT NULL,lot TEXT NOT NULL,kind TEXT NOT NULL,delta REAL NOT NULL,unit TEXT NOT NULL,reason TEXT NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS movements_material_idx ON movements(material_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS materials_type_idx ON materials(type)")
    }) if(!q.exec(sql)) return fail(q.lastError().text());
    if(seedDemo) {
        if(!q.exec(QStringLiteral("SELECT count(*) FROM materials")) || !q.next()) return fail(q.lastError().text());
        if(q.value(0).toInt()==0 && !seed()) return false;
    }
    return true;
}
QString InventoryRepository::validate(const Material &m) {
    if(m.name.trimmed().isEmpty()||m.type.trimmed().isEmpty()||m.color.trimmed().isEmpty()||m.supplier.trimmed().isEmpty()||m.lot.trimmed().isEmpty())
        return QStringLiteral("Renseignez le nom, le type, la couleur, le fournisseur et le lot.");
    if(m.name.size()>120||m.supplier.size()>120||m.color.size()>60||m.lot.size()>80)
        return QStringLiteral("Nom et fournisseur : 120 caractères maximum ; couleur : 60 ; lot : 80.");
    if(!std::isfinite(m.quantity)||!std::isfinite(m.minimum)||!std::isfinite(m.price)||m.quantity<0||m.minimum<0||m.price<0||m.quantity>9999999.99||m.minimum>9999999.99||m.price>9999999.99)
        return QStringLiteral("Quantité, seuil et prix doivent être compris entre 0 et 9 999 999,99.");
    if(!QStringList{QStringLiteral("m"),QStringLiteral("pièce"),QStringLiteral("bobine"),QStringLiteral("kg")}.contains(m.unit))
        return QStringLiteral("Choisissez une unité valide.");
    if((m.unit==QStringLiteral("pièce")||m.unit==QStringLiteral("bobine")) &&
        (std::abs(m.quantity-std::round(m.quantity))>0.00001||std::abs(m.minimum-std::round(m.minimum))>0.00001))
        return QStringLiteral("Les pièces et les bobines doivent avoir des quantités et des seuils entiers.");
    for(const QChar c : m.lot) if(c.isSpace()||c==':'||!c.isPrint())
        return QStringLiteral("Le lot ne doit contenir ni espace, ni deux-points, ni caractère de contrôle.");
    return {};
}
QVector<Material> InventoryRepository::all() {
    m_error.clear(); QVector<Material> out; QSqlQuery q(m_db);
    if(!q.exec(QStringLiteral("SELECT ")+columns+QStringLiteral(" FROM materials ORDER BY name COLLATE NOCASE"))) { fail(q.lastError().text()); return out; }
    while(q.next()) out.push_back(readMaterial(q));
    return out;
}
Material InventoryRepository::find(qint64 id) {
    m_error.clear(); QSqlQuery q(m_db); q.prepare(QStringLiteral("SELECT ")+columns+QStringLiteral(" FROM materials WHERE id=?")); q.addBindValue(id);
    if(!q.exec()) { fail(q.lastError().text()); return {}; }
    return q.next()?readMaterial(q):Material{};
}
Material InventoryRepository::findLot(const QString &lot) {
    m_error.clear(); QSqlQuery q(m_db); q.prepare(QStringLiteral("SELECT ")+columns+QStringLiteral(" FROM materials WHERE lot=? COLLATE NOCASE")); q.addBindValue(lot.trimmed());
    if(!q.exec()) { fail(q.lastError().text()); return {}; }
    return q.next()?readMaterial(q):Material{};
}
bool InventoryRepository::recordMovement(const Material &m,double delta,const QString &kind,const QString &reason) {
    QSqlQuery q(m_db); q.prepare(QStringLiteral("INSERT INTO movements(material_id,date,name,lot,kind,delta,unit,reason) VALUES(?,?,?,?,?,?,?,?)"));
    q.addBindValue(m.id); q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)); q.addBindValue(m.name); q.addBindValue(m.lot);
    q.addBindValue(kind); q.addBindValue(rounded(delta)); q.addBindValue(m.unit); q.addBindValue(reason.trimmed());
    if(!q.exec()) return fail(q.lastError().text());
    return true;
}
bool InventoryRepository::save(Material &m,const QString &reason) {
    m_error.clear(); m.name=m.name.trimmed(); m.color=m.color.trimmed(); m.supplier=m.supplier.trimmed(); m.lot=m.lot.trimmed();
    const QString validation=validate(m); if(!validation.isEmpty()) return fail(validation);
    m.quantity=rounded(m.quantity); m.minimum=rounded(m.minimum); m.price=rounded(m.price);
    if(!m_db.transaction()) return fail(m_db.lastError().text());
    const bool updating=m.id!=0; const Material old=updating?find(m.id):Material{};
    if(updating && !old.id) return fail(QStringLiteral("Cette matière n’existe plus."),true);
    if(updating && old.unit!=m.unit) return fail(QStringLiteral("L’unité d’un lot enregistré ne peut pas être changée. Créez un nouveau lot."),true);
    const double delta=rounded(m.quantity-old.quantity);
    if(updating && delta!=0 && reason.trimmed().isEmpty()) return fail(QStringLiteral("Indiquez le motif du changement de stock."),true);
    QSqlQuery q(m_db);
    q.prepare(updating?QStringLiteral("UPDATE materials SET name=?,type=?,color=?,quantity=?,minimum=?,unit=?,price=?,supplier=?,lot=? WHERE id=?"):
        QStringLiteral("INSERT INTO materials(name,type,color,quantity,minimum,unit,price,supplier,lot) VALUES(?,?,?,?,?,?,?,?,?)"));
    for(const QVariant &v:QVariantList{m.name,m.type,m.color,m.quantity,m.minimum,m.unit,m.price,m.supplier,m.lot}) q.addBindValue(v);
    if(updating) q.addBindValue(m.id);
    if(!q.exec()) {
        const QString err=q.lastError().text();
        return fail(err.contains(QStringLiteral("UNIQUE"))?QStringLiteral("Cet identifiant de lot existe déjà. Choisissez un lot unique."):err,true);
    }
    const qint64 savedId=updating?m.id:q.lastInsertId().toLongLong();
    Material snapshot=m; snapshot.id=savedId;
    if(delta!=0 && !recordMovement(snapshot,delta,updating?QStringLiteral("Correction"):QStringLiteral("Stock initial"),updating?reason:QStringLiteral("Création de la matière"))) return fail(m_error,true);
    if(!m_db.commit()) return fail(m_db.lastError().text(),true);
    m.id=savedId; return true;
}
bool InventoryRepository::remove(qint64 id) {
    m_error.clear(); QSqlQuery q(m_db); q.prepare(QStringLiteral("DELETE FROM materials WHERE id=?")); q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    if(q.numRowsAffected()!=1) return fail(QStringLiteral("Cette matière n’existe plus."));
    return true;
}
bool InventoryRepository::move(qint64 id,double delta,const QString &reason) {
    m_error.clear();
    if(!std::isfinite(delta)||std::abs(delta)<0.005||std::abs(delta)>9999999.99||reason.trimmed().isEmpty()) return fail(QStringLiteral("Indiquez une quantité positive et un motif."));
    delta=rounded(delta);
    if(!m_db.transaction()) return fail(m_db.lastError().text());
    Material m=find(id); if(!m.id) return fail(QStringLiteral("Ce lot n’existe plus."),true);
    m.quantity=rounded(m.quantity+delta);
    const QString validation=validate(m); if(!validation.isEmpty()) return fail(m.quantity<0?QStringLiteral("Stock insuffisant pour cette sortie."):validation,true);
    QSqlQuery q(m_db); q.prepare(QStringLiteral("UPDATE materials SET quantity=? WHERE id=?")); q.addBindValue(m.quantity); q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text(),true);
    if(!recordMovement(m,delta,delta>0?QStringLiteral("Réception"):QStringLiteral("Consommation"),reason)) return fail(m_error,true);
    if(!m_db.commit()) return fail(m_db.lastError().text(),true);
    return true;
}
QVector<Movement> InventoryRepository::history() {
    m_error.clear(); QVector<Movement> out; QSqlQuery q(m_db);
    if(!q.exec(QStringLiteral("SELECT date,name,lot,kind,delta,unit,reason FROM movements ORDER BY id DESC"))) { fail(q.lastError().text()); return out; }
    while(q.next()) out.push_back({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(5).toString(),q.value(6).toString(),q.value(4).toDouble()});
    return out;
}
bool InventoryRepository::seed() {
    struct Sample {const char *name,*type,*color,*unit,*supplier,*lot; double quantity,minimum,price;};
    const Sample samples[]={
        {"Soie duchesse","Tissu","Ivoire","m","Maison des tissus","SOI-2601",42.5,15,85},
        {"Dentelle de Calais","Dentelle","Champagne","m","Dentelles Prestige","DEN-2602",6,10,64},
        {"Fil de soie","Fil","Or","bobine","Mercerie Élégance","FIL-2603",18,8,12.5},
        {"Bouton nacré","Bouton","Perle","pièce","Mercerie Élégance","BTN-2604",80,30,2.4},
        {"Satin de soie","Tissu","Bordeaux","m","Maison des tissus","SAT-2605",8,12,48},
        {"Fermeture invisible","Fermeture","Noir","pièce","Zip Couture","ZIP-2606",0,15,3.8},
        {"Doublure viscose","Doublure","Ivoire","m","Textile Méditerranée","DBL-2607",55,20,9.5},
        {"Ruban de velours","Accessoire","Bordeaux","m","Mercerie Élégance","RUB-2608",24,10,6.2}
    };
    for(const auto &s:samples) {
        Material m; m.name=QString::fromUtf8(s.name); m.type=QString::fromUtf8(s.type); m.color=QString::fromUtf8(s.color); m.unit=QString::fromUtf8(s.unit);
        m.supplier=QString::fromUtf8(s.supplier); m.lot=QString::fromUtf8(s.lot); m.quantity=s.quantity; m.minimum=s.minimum; m.price=s.price;
        if(!save(m,{})) return false;
    } return true;
}
