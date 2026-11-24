#pragma once
#include "material.h"
#include <QString>
#include <QVector>

class MaterialRepository {
public:
    MaterialRepository();

    QVector<Material> loadMaterials();
    Material findMaterial(int id);
    Material findLot(const QString &lot);
    bool addMaterial(Material &material);
    bool updateMaterial(const Material &material);
    bool deleteMaterial(int id);
    bool changeQuantity(int id, double delta);

    QString error() const;
    static QString validate(const Material &material);

private:
    bool fail(const QString &message);
    bool lotUsed(const QString &lot, int exceptId);

    QString m_error;
};
