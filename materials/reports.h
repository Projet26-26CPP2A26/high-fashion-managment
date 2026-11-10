#pragma once
#include "inventoryrepository.h"
#include <QImage>

// Textes formatés, documents PDF et graphique des statistiques.
namespace StockReports {
QString money(double amount);      // 85 -> « 85,00 DT »
QString quantity(double amount);   // 42.5 -> « 42,5 »
QString inventoryHtml(const QVector<Material> &materials, const QString &scope);
QString purchaseHtml(const Material &m, double amount);
QString statisticsHtml(const QVector<Material> &materials);
QString labelHtml(const Material &m, const QImage &qr);
bool writePdf(const QString &path, const QString &html, QString *error = nullptr, bool landscape = false);
bool labelPdf(const QString &path, const Material &m, const QImage &qr, QString *error = nullptr);
QImage chart(const QVector<Material> &materials, const QSize &size);
}
