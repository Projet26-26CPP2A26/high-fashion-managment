#include "qrcode.h"
#include "materialrepository.h"
#include "pdfexport.h"
#include "qrcodegen.hpp"
extern "C" {
#include "quirc.h"
}
#include <cstring>
#include <memory>
#include <QUrl>

static const QString siteUrl = "https://itty.bitty.site/#";

QString QrService::pageUrl(const Material &m, const QDateTime &recorded)
{
    const QString etat = m.quantity() == 0 ? "Rupture" : m.low() ? "À commander" : "Disponible";
    const QString couleur = m.quantity() == 0 ? "#e8a9b0" : m.low() ? "#e6ce89" : "#b7ccb3";
    const QList<QStringList> lignes = {{"Lot", m.lot()},
                                       {"Type", m.type()},
                                       {"Couleur", m.color()},
                                       {"Fournisseur", m.supplier()},
                                       {"Stock", StockReports::quantity(m.quantity()) + " " + m.unit()},
                                       {"Seuil", StockReports::quantity(m.minimum()) + " " + m.unit()},
                                       {"Prix", StockReports::money(m.price()) + " / " + m.unit()},
                                       {"Date", recorded.toString("dd/MM/yyyy")}};
    QString html = "<style>p,h2{animation:a 1s}@keyframes a{0%{translate:0 "
                   "40px}}body{background:#1c1c1e;color:#f3f0ea}h1,h2{font:32px "
                   "Georgia;color:#e2cf9f}h1{font-size:16px;letter-spacing:8px}"
                   "p{display:flex;justify-content:space-between;gap:20px;border-bottom:1px solid "
                   "#c9b27c55;color:#b9b4a8}b{color:#f3f0ea;text-align:right}</style>"
                   "<h1>AUREA</h1><h2>" +
                   m.name().toHtmlEscaped() + "</h2><i style=color:" + couleur + ">● " + etat + "</i>";
    for (const QStringList &ligne : lignes)
        html += "<p>" + ligne[0] + "<b>" + ligne[1].toHtmlEscaped() + "</b>";
    const QByteArray page = qCompress(html.toUtf8(), 9).mid(4).toBase64(QByteArray::OmitTrailingEquals);
    return siteUrl + QUrl::toPercentEncoding(m.lot()) + "/" + page;
}
QString QrService::payload(const QString &lot)
{
    return QStringLiteral("AUREA:LOT:") + lot;
}
QString QrService::labelText(const Material &m, const QDateTime &recorded)
{
    const auto plain = [](QString s) {
        s.replace(QChar(0x202F), ' ').replace(QChar(0x00A0), ' ');
        return s;
    };
    const QString unit = m.unit().isEmpty() ? QString() : QStringLiteral(" ") + m.unit();
    QStringList lines{QStringLiteral("AUREA - Fiche matière"),
                      QStringLiteral("Lot : ") + m.lot(),
                      QStringLiteral("Matière : ") + m.name(),
                      QStringLiteral("Type : ") + m.type() + QStringLiteral(" - ") + m.color(),
                      QStringLiteral("Fournisseur : ") + m.supplier(),
                      QStringLiteral("Stock : ") + plain(StockReports::quantity(m.quantity())) + unit +
                          QStringLiteral(" (seuil : ") + plain(StockReports::quantity(m.minimum())) + unit +
                          QStringLiteral(")"),
                      QStringLiteral("Prix : ") + plain(StockReports::money(m.price())) +
                          (m.unit().isEmpty() ? QString() : QStringLiteral(" / ") + m.unit()),
                      QStringLiteral("État : ") + (m.low() ? QStringLiteral("stock bas, à réapprovisionner")
                                                           : QStringLiteral("stock suffisant")),
                      QStringLiteral("Date : ") + recorded.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))};
    return lines.join('\n');
}
QString QrService::lotFromCode(const QString &code)
{
    QString text = code.trimmed();
    if (text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        if (!text.startsWith(siteUrl, Qt::CaseInsensitive))
            return {};
        text = QUrl::fromPercentEncoding(text.mid(siteUrl.size()).section('/', 0, 0).toUtf8());
    } else if (text.startsWith(QStringLiteral("AUREA:LOT:")))
        text = text.mid(10);
    else if (text.startsWith(QStringLiteral("AUREA")) && text.contains('\n')) {
        const QStringList lines = text.split('\n');
        text.clear();
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            const int colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).trimmed() == QStringLiteral("Lot")) {
                text = line.mid(colon + 1).trimmed();
                break;
            }
        }
    } else if (text.contains(':'))
        return {};
    if (text.isEmpty() || text.size() > 80)
        return {};
    for (const QChar c : text)
        if (c.isSpace() || c == ':' || !c.isPrint())
            return {};
    return text;
}
QImage QrService::generate(const QString &text, int scale)
{
    if (text.isEmpty() || scale < 1 || scale > 32)
        return {};
    try {
        const QByteArray bytes = text.toUtf8();
        const auto qr = qrcodegen::QrCode::encodeText(bytes.constData(), qrcodegen::QrCode::Ecc::LOW);
        const int border = 4, extent = (qr.getSize() + border * 2) * scale;
        QImage image(extent, extent, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < qr.getSize(); ++y)
            for (int x = 0; x < qr.getSize(); ++x)
                if (qr.getModule(x, y))
                    for (int dy = 0; dy < scale; ++dy) {
                        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine((y + border) * scale + dy));
                        for (int dx = 0; dx < scale; ++dx)
                            line[(x + border) * scale + dx] = qRgb(0, 0, 0);
                    }
        return image;
    } catch (const std::exception &) {
        return {};
    }
}
QStringList QrService::decode(const QImage &source, QString *error)
{
    if (error)
        error->clear();
    if (source.isNull()) {
        if (error)
            *error = QStringLiteral("Cette image ne peut pas être ouverte.");
        return {};
    }
    QImage image = source;
    if (image.width() > 2400 || image.height() > 2400)
        image = image.scaled(2400, 2400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image = image.convertToFormat(QImage::Format_Grayscale8);
    std::unique_ptr<quirc, decltype(&quirc_destroy)> scanner(quirc_new(), quirc_destroy);
    if (!scanner || quirc_resize(scanner.get(), image.width(), image.height()) < 0) {
        if (error)
            *error = QStringLiteral("Mémoire insuffisante pour lire ce QR code.");
        return {};
    }
    int w = 0, h = 0;
    uint8_t *buffer = quirc_begin(scanner.get(), &w, &h);
    for (int y = 0; y < h; ++y)
        std::memcpy(buffer + y * w, image.constScanLine(y), static_cast<size_t>(w));
    quirc_end(scanner.get());
    QStringList results;
    for (int i = 0; i < quirc_count(scanner.get()); ++i) {
        quirc_code code{};
        quirc_data data{};
        quirc_extract(scanner.get(), i, &code);
        auto status = quirc_decode(&code, &data);
        if (status == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(&code);
            status = quirc_decode(&code, &data);
        }
        if (status == QUIRC_SUCCESS)
            results.append(QString::fromUtf8(reinterpret_cast<const char *>(data.payload), data.payload_len));
    }
    results.removeDuplicates();
    if (results.isEmpty() && error)
        *error = QStringLiteral("Aucun QR code lisible. Choisissez une image nette contenant le code complet.");
    return results;
}
