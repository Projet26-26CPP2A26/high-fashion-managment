#pragma once
#include "inventoryrepository.h"
#include <QImage>

namespace StockReports {
QString money(double amount);
QString quantity(double amount);
QString inventoryHtml(const QVector<Material> &materials, const QString &scope);
QString purchaseHtml(const Material &m, double amount);
QString statisticsHtml(const QVector<Material> &materials);
bool writePdf(const QString &path, const QString &html, QString *error = nullptr, bool landscape = false);
bool labelPdf(const QString &path, const Material &m, const QImage &qr, QString *error = nullptr);
QImage chart(const QVector<Material> &materials, const QSize &size);
}
