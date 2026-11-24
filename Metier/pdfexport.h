#pragma once
#include "materialrepository.h"
#include <QImage>

class TypeStat
{
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

class StockReports
{
public:
    static QString money(double amount);
    static QString quantity(double amount);
    static QString inventoryHtml(const QVector<Material> &materials, const QString &scope);
    static QString purchaseHtml(const Material &m, double amount);
    static QString allOrdersHtml(const QVector<Material> &materials);
    static QString statisticsHtml(const QVector<Material> &materials);
    static bool writePdf(const QString &path, const QString &html, QString *error = nullptr, bool landscape = false);
    static bool labelPdf(const QString &path, const Material &m, const QImage &qr, QString *error = nullptr);
    static QImage chart(const QVector<Material> &materials, const QSize &size);
};
