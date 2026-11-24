#include "material.h"
#include <cmath>

Material::Material() : m_id(0), m_unit("m"), m_quantity(0), m_minimum(0), m_price(0) {}

int Material::id() const
{
    return m_id;
}
QString Material::name() const
{
    return m_name;
}
QString Material::type() const
{
    return m_type;
}
QString Material::color() const
{
    return m_color;
}
QString Material::unit() const
{
    return m_unit;
}
QString Material::supplier() const
{
    return m_supplier;
}
QString Material::lot() const
{
    return m_lot;
}
double Material::quantity() const
{
    return m_quantity;
}
double Material::minimum() const
{
    return m_minimum;
}
double Material::price() const
{
    return m_price;
}
bool Material::low() const
{
    return m_quantity < m_minimum;
}
double Material::toOrder() const
{
    return low() ? std::round((2 * m_minimum - m_quantity) * 100.0) / 100.0 : 0;
}

void Material::setId(int value)
{
    m_id = value;
}
void Material::setName(const QString &value)
{
    m_name = value;
}
void Material::setType(const QString &value)
{
    m_type = value;
}
void Material::setColor(const QString &value)
{
    m_color = value;
}
void Material::setUnit(const QString &value)
{
    m_unit = value;
}
void Material::setSupplier(const QString &value)
{
    m_supplier = value;
}
void Material::setLot(const QString &value)
{
    m_lot = value;
}
void Material::setQuantity(double value)
{
    m_quantity = value;
}
void Material::setMinimum(double value)
{
    m_minimum = value;
}
void Material::setPrice(double value)
{
    m_price = value;
}
