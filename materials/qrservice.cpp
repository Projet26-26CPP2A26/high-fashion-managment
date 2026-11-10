#include "qrservice.h"
#include "reports.h"
#include <QBuffer>
#include <QDateTime>
#include <QEventLoop>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextDocumentFragment>
#include <QUrl>

namespace {
const QString site = "https://itty.bitty.site/#";

QString html(const QString &text)
{
    return text.toHtmlEscaped();   // « < » devient « &lt; » : un nom ne peut pas casser la page
}

// Envoie la requête et attend la réponse (15 s au plus).
// Renvoie le contenu de la réponse, ou vide si le serveur n'a pas répondu (pas d'internet).
QByteArray waitForReply(QNetworkReply *reply)
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    reply->deleteLater();
    return reply->readAll();
}
}

// Lien du type  https://itty.bitty.site/#Lot-SOI-2601/<page>
//   « Lot-SOI-2601 » = titre de la page (l'application y relit le lot)
//   <page>           = petite page HTML, compressée (qCompress) puis écrite en base64
QString QrService::link(const Material &m)
{
    // 1. La page web de la fiche
    QString page = "<meta name='viewport' content='width=device-width'>"
                   "<style>body{font-family:sans-serif;background:#1b2022;color:#eee;padding:24px}"
                   "h1{color:#c9b27c;margin:0 0 4px}small{color:#999}p{font-size:18px;line-height:1.6}</style>";
    page += "<small>AUREA · LOT " + html(m.lot) + "</small>";
    page += "<h1>" + html(m.name) + "</h1>";
    page += "<p>Type : " + html(m.type) + " · " + html(m.color);
    page += "<br>Stock : " + StockReports::quantity(m.quantity) + " " + m.unit + " (seuil " + StockReports::quantity(m.minimum) + ")";
    page += "<br>Prix : " + StockReports::money(m.price);
    page += "<br>Fournisseur : " + html(m.supplier) + "</p>";
    page += "<small>Fiche du " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "</small>";

    // 2. Compression : qCompress ajoute 4 octets (la taille) avant les données zlib, le site n'en veut pas.
    const QByteArray zlib = qCompress(page.toUtf8(), 9).mid(4);

    // 3. Le lien
    const QString title = "Lot-" + QUrl::toPercentEncoding(m.lot);   // « / » ou « # » dans un lot sont encodés
    return site + title + "/" + zlib.toBase64();
}

// Accepte un lien AUREA, « AUREA:LOT:SOI-2601 » ou simplement « SOI-2601 ».
// Renvoie "" si ce n'est pas un lot AUREA.
QString QrService::lotFromCode(const QString &code)
{
    QString lot = code.trimmed();
    if (lot.startsWith(site + "Lot-")) {
        // Lien : le lot est entre « #Lot- » et le « / » suivant.
        lot = lot.mid(site.size() + 4).section('/', 0, 0);
        lot = QUrl::fromPercentEncoding(lot.toUtf8());
    } else if (lot.startsWith("AUREA:LOT:")) {
        lot = lot.mid(10);   // on enlève « AUREA:LOT: » (10 caractères)
    }
    if (lot.contains(':') || lot.contains(' '))
        return "";           // par exemple un autre lien « https://... »
    return lot;
}

// GET https://api.qrserver.com/v1/create-qr-code/?size=400x400&data=<texte>  ->  image PNG
QImage QrService::generate(const QString &text)
{
    // margin = marge blanche nécessaire aux lecteurs ; le texte est encodé pour tenir dans l'adresse.
    const QUrl url("https://api.qrserver.com/v1/create-qr-code/?size=400x400&margin=16&data=" + QUrl::toPercentEncoding(text));

    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    return QImage::fromData(waitForReply(network.get(request)));
}

// Envoie l'image au lecteur en ligne ZXing (https://zxing.org/w/decode).
// Il répond par une page web où le texte du QR est écrit entre <pre> et </pre>.
QStringList QrService::decode(const QImage &image, QString *error)
{
    if (image.isNull()) {
        if (error) *error = "Cette image ne peut pas être ouverte.";
        return {};
    }

    // On réduit les grandes photos et on les envoie en PNG.
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (image.width() > 1200 || image.height() > 1200)
        image.scaled(1200, 1200, Qt::KeepAspectRatio).save(&buffer, "PNG");
    else
        image.save(&buffer, "PNG");

    // Formulaire « f = image », comme l'envoi d'un fichier sur un site web.
    auto *form = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart file;
    file.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"f\"; filename=\"qr.png\"");
    file.setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
    file.setBody(png);
    form->append(file);

    QNetworkAccessManager network;
    QNetworkRequest request(QUrl("https://zxing.org/w/decode"));
    request.setTransferTimeout(15000);
    QNetworkReply *reply = network.post(request, form);
    form->setParent(reply);   // le formulaire est supprimé avec la réponse
    const QString page = QString::fromUtf8(waitForReply(reply));
    if (page.isEmpty()) {
        if (error) *error = "Lecture impossible : vérifiez la connexion internet.";
        return {};
    }
    if (!page.contains("Decode Succeeded")) {
        if (error) *error = "Aucun QR code lisible. Choisissez une image nette contenant le code complet.";
        return {};
    }
    const QString text = page.section("<pre>", 1, 1).section("</pre>", 0, 0);
    // « &amp; » -> « & », etc. La balise <pre> garde les retours à la ligne.
    return {QTextDocumentFragment::fromHtml("<pre>" + text + "</pre>").toPlainText()};
}
