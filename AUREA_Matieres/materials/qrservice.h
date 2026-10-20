#pragma once
#include <QDateTime>
#include <QImage>
struct Material;
#include <QString>
#include <QStringList>

namespace QrService {
QString payload(const QString &lot);
QString readerUrl();
QString phonePayload(const Material &material, const QDateTime &recorded = QDateTime::currentDateTimeUtc());
QString lotFromCode(const QString &code);
QImage generate(const QString &text, int scale = 8);
QStringList decode(const QImage &image, QString *error = nullptr);
}
