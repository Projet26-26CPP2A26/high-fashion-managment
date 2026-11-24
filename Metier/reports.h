#pragma once
#include "inventoryrepository.h"
#include <QImage>

class TypeStat {
public:
    TypeStat();
    void add(const Material &material);
    int count() const;
    int low() const;
    double value() const;

private:
    int m_count;
    int m_low;
    double m_value;
};

namespace StockReports {
QString money(double amount);
QString quantity(double amount);
QString inventoryHtml(const QVector<Material> &materials, const QString &scope);
QString purchaseHtml(const Material &m, double amount);
QString allOrdersHtml(const QVector<Material> &materials);
QString statisticsHtml(const QVector<Material> &materials);
bool writePdf(const QString &path, const QString &html, QString *error = nullptr, bool landscape = false);
bool labelPdf(const QString &path, const Material &m, const QImage &qr, QString *error = nullptr);
QImage chart(const QVector<Material> &materials, const QSize &size);
}
