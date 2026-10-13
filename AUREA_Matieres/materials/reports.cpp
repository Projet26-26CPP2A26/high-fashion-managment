#include "reports.h"
#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QMap>
#include <QPageLayout>
#include <QPainter>
#include <QPrinter>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextDocument>
#include <algorithm>
#include <cmath>

namespace {
const QLocale french(QLocale::French,QLocale::France);
QString escape(const QString &s) { return s.toHtmlEscaped(); }
QString shell(const QString &title,const QString &subtitle,const QString &body) {
    return QStringLiteral("<html><head><meta charset='utf-8'><style>body{font-family:'Inter','Segoe UI';font-size:10pt;color:#202728;}h1{font-family:Georgia;color:#8f753e;font-size:26pt;}h2{font-size:16pt;}p{line-height:140%;}th{background:#283033;color:#ffffff;padding:8px;text-align:left;}td{padding:7px;border-bottom:1px solid #dddddd;}table{border-collapse:collapse;width:100%;}.muted{color:#606b68;}.total{font-size:15pt;color:#8f753e;}</style></head><body><h1>AUREA</h1><p class='muted'>ATELIER HAUTE COUTURE · MATIÈRES PREMIÈRES</p><h2>%1</h2><p>%2</p>%3<p class='muted'>Édité le %4 · Prix unitaires en dinars tunisiens (DT)</p></body></html>")
        .arg(escape(title),escape(subtitle),body,escape(french.toString(QDateTime::currentDateTime(),QStringLiteral("dd MMMM yyyy à HH:mm"))));
}
QString cells(const QStringList &values) {
    QString html=QStringLiteral("<tr>"); for(const auto &v:values) html+=QStringLiteral("<td>")+escape(v)+QStringLiteral("</td>"); return html+QStringLiteral("</tr>");
}
}
QString StockReports::money(double amount) { return french.toString(amount,'f',2)+QStringLiteral(" DT"); }
QString StockReports::quantity(double amount) {
    QString s=french.toString(amount,'f',2);
    if(s.endsWith(QStringLiteral(",00"))) s.chop(3); else if(s.endsWith('0')) s.chop(1); return s;
}
QString StockReports::inventoryHtml(const QVector<Material> &materials,const QString &scope) {
    QString body=QStringLiteral("<table><thead><tr><th>Matière</th><th>Type / couleur</th><th>Stock</th><th>Seuil</th><th>Prix / unité</th><th>Fournisseur</th><th>Lot</th></tr></thead><tbody>"); double total=0;
    for(const auto &m:materials) {
        body+=cells({m.name,m.type+QStringLiteral(" / ")+m.color,quantity(m.quantity)+" "+m.unit,quantity(m.minimum)+" "+m.unit,money(m.price),m.supplier,m.lot}); total+=m.quantity*m.price;
    }
    body+=QStringLiteral("</tbody></table><p class='total'>%1 référence(s) · Valeur du stock : %2</p>").arg(materials.size()).arg(escape(money(total)));
    return shell(QStringLiteral("Inventaire des matières premières"),scope,body);
}
QString StockReports::purchaseHtml(const Material &m,double amount) {
    QString body=QStringLiteral("<p><b>Fournisseur :</b> %1<br><b>Référence du document :</b> BC-%2-%3</p><table><thead><tr><th>Matière</th><th>Couleur</th><th>Lot de référence</th><th>Quantité demandée</th><th>Prix estimé / unité</th><th>Montant estimé</th></tr></thead><tbody>")
        .arg(escape(m.supplier),QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),QString::number(m.id));
    body+=cells({m.name,m.color,m.lot,quantity(amount)+" "+m.unit,money(m.price),money(amount*m.price)});
    body+=QStringLiteral("</tbody></table><p class='total'>Total estimatif : %1</p><p>Prix à confirmer par le fournisseur. Taxes et frais de transport non inclus.</p><p>Adresse de livraison : ______________________________</p><p>Date souhaitée : ______________________________</p><p>Visa du responsable : ______________________________</p>").arg(escape(money(amount*m.price)));
    return shell(QStringLiteral("Bon de commande fournisseur"),QStringLiteral("Document préparé pour validation par le responsable de stock."),body);
}
QString StockReports::statisticsHtml(const QVector<Material> &materials) {
    struct Stat {int n=0,low=0; double value=0;}; QMap<QString,Stat> groups; double total=0; int low=0;
    for(const auto &m:materials) {auto &s=groups[m.type]; ++s.n; s.low+=m.low(); s.value+=m.quantity*m.price; total+=m.quantity*m.price; low+=m.low();}
    QString body=QStringLiteral("<p>%1 références · %2 sous le seuil minimal</p><table><thead><tr><th>Type</th><th>Références</th><th>Sous le seuil</th><th>Valeur du stock</th></tr></thead><tbody>").arg(materials.size()).arg(low);
    for(auto it=groups.cbegin();it!=groups.cend();++it) body+=cells({it.key(),QString::number(it->n),QString::number(it->low),money(it->value)});
    body+=QStringLiteral("</tbody></table><p class='total'>Valeur totale : %1</p>").arg(escape(money(total)));
    const QImage plot=chart(materials,QSize(1100,420)); QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); plot.save(&buffer,"PNG");
    body+=QStringLiteral("<p><img width='520' height='199' src='data:image/png;base64,%1'></p>").arg(QString::fromLatin1(png.toBase64()));
    return shell(QStringLiteral("Statistiques du stock"),QStringLiteral("La valeur permet de comparer des matières dont les unités diffèrent."),body);
}
bool StockReports::writePdf(const QString &path,const QString &html,QString *error,bool landscape) {
    if(error) error->clear();
    QTemporaryFile temporary(QFileInfo(path).absolutePath()+QStringLiteral("/.aurea-report-XXXXXX.pdf"));
    if(!temporary.open()) {if(error) *error=QStringLiteral("Impossible d’écrire le PDF dans ce dossier."); return false;}
    const QString temporaryPath=temporary.fileName(); temporary.close();
    QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(temporaryPath);
    printer.setPageSize(QPageSize(QPageSize::A4)); printer.setPageOrientation(landscape?QPageLayout::Landscape:QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(14,14,14,14),QPageLayout::Millimeter);
    QTextDocument doc; doc.setHtml(html); doc.print(&printer);
    QFile pdf(temporaryPath); QSaveFile output(path);
    if(!pdf.open(QIODevice::ReadOnly)||pdf.size()<500||!output.open(QIODevice::WriteOnly)) {
        if(error) *error=QStringLiteral("Impossible d’écrire le PDF dans ce dossier.");
        return false;
    }
    const QByteArray bytes=pdf.readAll();
    if(output.write(bytes)!=bytes.size()||!output.commit()) {
        if(error) *error=QStringLiteral("L’enregistrement du PDF a échoué.");
        return false;
    }
    return true;
}
bool StockReports::labelPdf(const QString &path,const Material &m,const QImage &qr,QString *error) {
    if(qr.isNull()) {if(error) *error=QStringLiteral("QR code indisponible."); return false;}
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); qr.save(&buffer,"PNG");
    QString body=QStringLiteral("<p><img width='260' height='260' src='data:image/png;base64,%1'></p><p><b>Lot :</b> %2<br><b>Matière :</b> %3<br><b>Type / couleur :</b> %4 / %5<br><b>Fournisseur :</b> %6</p><p class='muted'>Téléphone : ouvrez le lien avec la caméra. La fiche apparaît directement, sans compte. Première visite avec internet (4G/5G possible). Après « Lecture hors connexion prête », utilisez le même navigateur même sans internet. Aucun PC allumé ni Wi-Fi partagé requis. Stock daté : recréez l’étiquette après modification.</p>")
        .arg(QString::fromLatin1(png.toBase64()),escape(m.lot),escape(m.name),escape(m.type),escape(m.color),escape(m.supplier));
    return writePdf(path,shell(QStringLiteral("Étiquette de traçabilité"),QStringLiteral("Un code pour retrouver l’origine de chaque lot."),body),error);
}
QImage StockReports::chart(const QVector<Material> &materials,const QSize &requested) {
    const QSize size=requested.expandedTo(QSize(400,200)); QImage image(size,QImage::Format_ARGB32_Premultiplied); image.fill(QColor("#232326"));
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); QMap<QString,double> groups; double max=1;
    for(const auto &m:materials) groups[m.type]+=m.quantity*m.price;
    for(double v:groups) max=std::max(max,v);
    if(groups.isEmpty()) {p.setPen(QColor("#8f8c86")); p.drawText(image.rect(),Qt::AlignCenter,QStringLiteral("Ajoutez des matières pour afficher les statistiques.")); return image;}
    const double top=14,left=130,right=150,step=(size.height()-25.0)/groups.size(), width=size.width()-left-right;
    int i=0; QFont font(QStringLiteral("Segoe UI")); font.setPixelSize(13); p.setFont(font);
    for(auto it=groups.cbegin();it!=groups.cend();++it,++i) {
        const double y=top+i*step, height=std::min(27.0,step-8);
        p.setPen(QColor("#b9b4a8")); p.drawText(QRectF(0,y,115,height),Qt::AlignVCenter|Qt::AlignRight,it.key());
        p.setPen(Qt::NoPen); p.setBrush(QColor("#2e2e32")); p.drawRoundedRect(QRectF(left,y,width,height),5,5);
        QLinearGradient gradient(left,y,left+width,y); gradient.setColorAt(0,QColor("#bfa370")); gradient.setColorAt(1,QColor("#e9d8a8"));
        p.setBrush(gradient); p.drawRoundedRect(QRectF(left,y,width*it.value()/max,height),5,5);
        p.setPen(QColor("#f3f0ea")); p.drawText(QRectF(left+width+14,y,right-14,height),Qt::AlignVCenter|Qt::AlignLeft,money(it.value()));
    } return image;
}
