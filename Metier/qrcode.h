#pragma once
#include <QDateTime>
#include <QImage>
class Material;
#include <QString>
#include <QStringList>

namespace QrService {
// Lien vers le site : la fiche de la matière est dans le lien.
QString pageUrl(const Material &material, const QDateTime &recorded = QDateTime::currentDateTime());
QString payload(const QString &lot);
QString labelText(const Material &material, const QDateTime &recorded = QDateTime::currentDateTime());
QString lotFromCode(const QString &code);
QImage generate(const QString &text, int scale = 8);
QStringList decode(const QImage &image, QString *error = nullptr);
}
