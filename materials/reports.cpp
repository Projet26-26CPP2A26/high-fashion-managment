#include "reports.h"
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPrinter>
#include <QTextDocument>
#include <algorithm>

// Les documents PDF sont écrits en HTML puis imprimés dans un fichier PDF par Qt.
namespace {
const QLocale french(QLocale::French, QLocale::France);

QString escape(const QString &s)
{
    return s.toHtmlEscaped();   // « < » devient « &lt; » : un nom ne peut pas casser le HTML
}

// Cadre commun à tous les documents : style, en-tête AUREA, titre, contenu, date.
QString page(const QString &title, const QString &subtitle, const QString &body)
{
    const QString style =
        "body{font-family:'Inter','Segoe UI';font-size:10pt;color:#202728;}"
        "h1{font-family:Georgia;color:#8f753e;font-size:26pt;} h2{font-size:16pt;} p{line-height:140%;}"
        "table{border-collapse:collapse;width:100%;}"
        "th{background:#283033;color:#ffffff;padding:8px;text-align:left;}"
        "td{padding:7px;border-bottom:1px solid #dddddd;}"
        ".muted{color:#606b68;} .total{font-size:15pt;color:#8f753e;}";
    const QString date = french.toString(QDateTime::currentDateTime(), "dd MMMM yyyy à HH:mm");
    return "<html><head><meta charset='utf-8'><style>" + style + "</style></head><body>"
           "<h1>AUREA</h1><p class='muted'>ATELIER HAUTE COUTURE · MATIÈRES PREMIÈRES</p>"
           "<h2>" + escape(title) + "</h2><p>" + escape(subtitle) + "</p>" + body +
           "<p class='muted'>Édité le " + date + " · Prix unitaires en dinars tunisiens (DT)</p></body></html>";
}

// En-tête de tableau : <table><tr><th>...</th></tr>
QString tableHead(const QStringList &titles)
{
    QString html = "<table><tr>";
    for (const QString &t : titles) html += "<th>" + t + "</th>";
    return html + "</tr>";
}

// Une ligne de tableau : <tr><td>...</td></tr>
QString tableRow(const QStringList &values)
{
    QString html = "<tr>";
    for (const QString &v : values) html += "<td>" + escape(v) + "</td>";
    return html + "</tr>";
}

// Image -> balise <img> intégrée au HTML (PNG en base64).
QString imageTag(const QImage &image, int width, int height)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString("<p><img width='%1' height='%2' src='data:image/png;base64,%3'></p>")
        .arg(width).arg(height).arg(QString::fromLatin1(png.toBase64()));
}
}

// 85 -> « 85,00 DT »
QString StockReports::money(double amount)
{
    return french.toString(amount, 'f', 2) + " DT";
}

// 42.50 -> « 42,5 » ; 20.00 -> « 20 »
QString StockReports::quantity(double amount)
{
    QString s = french.toString(amount, 'f', 2);
    if (s.endsWith(",00")) s.chop(3);
    else if (s.endsWith('0')) s.chop(1);
    return s;
}

QString StockReports::inventoryHtml(const QVector<Material> &materials, const QString &scope)
{
    QString body = tableHead({"Matière", "Type / couleur", "Stock", "Seuil", "Prix / unité", "Fournisseur", "Lot"});
    double total = 0;
    for (const Material &m : materials) {
        body += tableRow({m.name, m.type + " / " + m.color, quantity(m.quantity) + " " + m.unit,
                          quantity(m.minimum) + " " + m.unit, money(m.price), m.supplier, m.lot});
        total += m.quantity * m.price;
    }
    body += "</table>";
    body += QString("<p class='total'>%1 référence(s) · Valeur du stock : %2</p>").arg(materials.size()).arg(money(total));
    return page("Inventaire des matières premières", scope, body);
}

QString StockReports::purchaseHtml(const Material &m, double amount)
{
    const QString reference = "BC-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "-" + QString::number(m.id);
    QString body = "<p><b>Fournisseur :</b> " + escape(m.supplier) + "<br><b>Référence du document :</b> " + reference + "</p>";
    body += tableHead({"Matière", "Couleur", "Lot de référence", "Quantité demandée", "Prix estimé / unité", "Montant estimé"});
    body += tableRow({m.name, m.color, m.lot, quantity(amount) + " " + m.unit, money(m.price), money(amount * m.price)});
    body += "</table>";
    body += "<p class='total'>Total estimatif : " + money(amount * m.price) + "</p>";
    body += "<p>Prix à confirmer par le fournisseur. Taxes et frais de transport non inclus.</p>"
            "<p>Adresse de livraison : ______________________________</p>"
            "<p>Date souhaitée : ______________________________</p>"
            "<p>Visa du responsable : ______________________________</p>";
    return page("Bon de commande fournisseur", "Document préparé pour validation par le responsable de stock.", body);
}

QString StockReports::statisticsHtml(const QVector<Material> &materials)
{
    // Pour chaque type : nombre de matières, nombre sous le seuil, valeur du stock.
    struct Stat { int count = 0; int low = 0; double value = 0; };
    QMap<QString, Stat> groups;
    double total = 0;
    int low = 0;
    for (const Material &m : materials) {
        Stat &s = groups[m.type];
        s.count++;
        s.value += m.quantity * m.price;
        total += m.quantity * m.price;
        if (m.low()) {
            s.low++;
            low++;
        }
    }

    QString body = QString("<p>%1 références · %2 sous le seuil minimal</p>").arg(materials.size()).arg(low);
    body += tableHead({"Type", "Références", "Sous le seuil", "Valeur du stock"});
    for (auto it = groups.cbegin(); it != groups.cend(); ++it)
        body += tableRow({it.key(), QString::number(it->count), QString::number(it->low), money(it->value)});
    body += "</table>";
    body += "<p class='total'>Valeur totale : " + money(total) + "</p>";
    body += imageTag(chart(materials, QSize(1100, 420)), 520, 199);
    return page("Statistiques du stock", "La valeur permet de comparer des matières dont les unités diffèrent.", body);
}

QString StockReports::labelHtml(const Material &m, const QImage &qr)
{
    QString body = imageTag(qr, 260, 260);
    body += "<p><b>Lot :</b> " + escape(m.lot) + "<br><b>Matière :</b> " + escape(m.name)
          + "<br><b>Type / couleur :</b> " + escape(m.type) + " / " + escape(m.color)
          + "<br><b>Fournisseur :</b> " + escape(m.supplier) + "</p>";
    body += "<p class='muted'>Téléphone : scannez le QR avec la caméra, touchez le lien, la fiche du lot s’ouvre dans le navigateur."
            "Stock daté : recréez l’étiquette après modification.</p>";
    return page("Étiquette de traçabilité", "Un code pour retrouver l’origine de chaque lot.", body);
}

// Imprime le HTML dans un fichier PDF A4.
bool StockReports::writePdf(const QString &path, const QString &html, QString *error, bool landscape)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(14, 14, 14, 14), QPageLayout::Millimeter);

    QTextDocument document;
    document.setHtml(html);
    document.print(&printer);
    if (printer.printerState() == QPrinter::Error || !QFile::exists(path)) {
        if (error) *error = "Impossible d’écrire le PDF dans ce dossier.";
        return false;
    }
    return true;
}

bool StockReports::labelPdf(const QString &path, const Material &m, const QImage &qr, QString *error)
{
    if (qr.isNull()) {
        if (error) *error = "QR code indisponible : vérifiez la connexion internet.";
        return false;
    }
    return writePdf(path, labelHtml(m, qr), error);
}

// Graphique en barres horizontales : valeur du stock par type.
QImage StockReports::chart(const QVector<Material> &materials, const QSize &requested)
{
    const QSize size = requested.expandedTo(QSize(400, 200));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#232326"));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    QMap<QString, double> values;   // type -> valeur du stock
    for (const Material &m : materials)
        values[m.type] += m.quantity * m.price;
    if (values.isEmpty()) {
        p.setPen(QColor("#8f8c86"));
        p.drawText(image.rect(), Qt::AlignCenter, "Ajoutez des matières pour afficher les statistiques.");
        return image;
    }
    double max = 1;
    for (double v : values) max = std::max(max, v);

    // Zone des barres : 130 px à gauche pour le nom du type, 150 px à droite pour le montant.
    const double left = 130, right = 150, top = 14;
    const double width = size.width() - left - right;
    const double step = (size.height() - 25.0) / values.size();
    const double height = std::min(27.0, step - 8);
    QFont font("Segoe UI");
    font.setPixelSize(13);
    p.setFont(font);

    int i = 0;
    for (auto it = values.cbegin(); it != values.cend(); ++it, ++i) {
        const double y = top + i * step;
        p.setPen(QColor("#b9b4a8"));
        p.drawText(QRectF(0, y, 115, height), Qt::AlignVCenter | Qt::AlignRight, it.key());

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2e2e32"));   // fond de la barre
        p.drawRoundedRect(QRectF(left, y, width, height), 5, 5);
        QLinearGradient gold(left, y, left + width, y);
        gold.setColorAt(0, QColor("#bfa370"));
        gold.setColorAt(1, QColor("#e9d8a8"));
        p.setBrush(gold);                // barre dorée, proportionnelle à la valeur
        p.drawRoundedRect(QRectF(left, y, width * it.value() / max, height), 5, 5);

        p.setPen(QColor("#f3f0ea"));
        p.drawText(QRectF(left + width + 14, y, right - 14, height), Qt::AlignVCenter | Qt::AlignLeft, money(it.value()));
    }
    return image;
}
