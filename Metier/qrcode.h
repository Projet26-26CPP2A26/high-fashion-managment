#pragma once
#include <QDateTime>
#include <QImage>
class Material;
#include <QString>
#include <QStringList>

class QrService
{
public:
    static QString pageUrl(const Material &material, const QDateTime &recorded = QDateTime::currentDateTime());
    static QString payload(const QString &lot);
    static QString labelText(const Material &material, const QDateTime &recorded = QDateTime::currentDateTime());
    static QString lotFromCode(const QString &code);
    static QImage generate(const QString &text, int scale = 8);
    static QStringList decode(const QImage &image, QString *error = nullptr);
};
