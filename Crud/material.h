#pragma once
#include <QString>

class Material
{
public:
    Material();

    int id() const;
    QString name() const;
    QString type() const;
    QString color() const;
    QString unit() const;
    QString supplier() const;
    QString lot() const;
    double quantity() const;
    double minimum() const;
    double price() const;
    bool low() const;
    double toOrder() const;

    void setId(int value);
    void setName(const QString &value);
    void setType(const QString &value);
    void setColor(const QString &value);
    void setUnit(const QString &value);
    void setSupplier(const QString &value);
    void setLot(const QString &value);
    void setQuantity(double value);
    void setMinimum(double value);
    void setPrice(double value);

private:
    int m_id;
    QString m_name;
    QString m_type;
    QString m_color;
    QString m_unit;
    QString m_supplier;
    QString m_lot;
    double m_quantity;
    double m_minimum;
    double m_price;
};
