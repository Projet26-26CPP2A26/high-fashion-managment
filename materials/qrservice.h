#pragma once
#include "inventoryrepository.h"
#include <QImage>
#include <QString>
#include <QStringList>

// QR codes des lots.
// - Le QR contient un lien vers itty.bitty.site (gratuit, sans compte) : le téléphone ouvre une page
//   web avec la fiche du lot. La page est écrite dans le lien lui-même : aucun site à héberger.
// - Les images QR sont créées par api.qrserver.com et lues par zxing.org (internet nécessaire sur le PC).
namespace QrService {
QString link(const Material &m);                                    // lien à mettre dans le QR
QString lotFromCode(const QString &code);                           // lien, « AUREA:LOT:x » ou lot -> lot
QImage generate(const QString &text);                               // texte -> image du QR (API)
QStringList decode(const QImage &image, QString *error = nullptr);  // image -> textes lus (API)
}
