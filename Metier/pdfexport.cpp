#include "pdfexport.h"
#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPrinter>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextDocument>

bool Reports::writePdf(const QString &path,const QString &html,QString *error,bool landscape) {
    if(error) error->clear();
    QTemporaryFile temporary(QFileInfo(path).absolutePath()+QStringLiteral("/.aurea-report-XXXXXX.pdf"));
    if(!temporary.open()) {if(error) *error=QStringLiteral("Impossible d’écrire le PDF dans ce dossier."); return false;}
    const QString temporaryPath=temporary.fileName(); temporary.close();
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(temporaryPath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(landscape?QPageLayout::Landscape:QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(14,14,14,14),QPageLayout::Millimeter);
    QTextDocument document; document.setHtml(html); document.print(&printer);
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
