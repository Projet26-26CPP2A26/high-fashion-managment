#include "materialwindow.h"
#include "ChartAnimation.h"
#include "pdfexport.h"
#include "ui_materialwindow.h"
#include "uihelpers.h"
#include <QFileDialog>
#include <QMap>

QString MaterialWindow::pdfPath(const QString &suggestion)
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Enregistrer le document PDF"), suggestion,
                                                QStringLiteral("Document PDF (*.pdf)"));
    if (!path.isEmpty() && !path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        path += QStringLiteral(".pdf");
    return path;
}
void MaterialWindow::exportInventory()
{
    const QString path = pdfPath(QStringLiteral("AUREA-inventaire.pdf"));
    if (path.isEmpty())
        return;
    QString scope =
        QStringLiteral("Filtres : %1 · %2").arg(ui->typeFilter->currentText(), ui->stockFilter->currentText());
    if (!ui->searchEdit->text().trimmed().isEmpty())
        scope += QStringLiteral(" · Recherche : ") + ui->searchEdit->text().trimmed();
    QString error;
    if (!StockReports::writePdf(path, StockReports::inventoryHtml(filteredMaterials(), scope), &error, true)) {
        failure(error);
        return;
    }
    notify(QStringLiteral("Inventaire exporté : ") + path);
}
void MaterialWindow::updateStats()
{
    QMap<QString, TypeStat> stats;
    for (const auto &m : materials)
        stats[m.type()].add(m);
    ui->statsTable->setRowCount(stats.size());
    int r = 0;
    for (auto it = stats.cbegin(); it != stats.cend(); ++it, ++r) {
        ui->statsTable->setItem(r, 0, cell(it.key()));
        ui->statsTable->setItem(r, 1, number(it->count()));
        ui->statsTable->setItem(r, 2, number(it->low()));
        ui->statsTable->setItem(r, 3, cell(StockReports::money(it->value())));
    }
    QVector<QPair<QString, double>> values;
    for (auto it = stats.cbegin(); it != stats.cend(); ++it)
        values.append({it.key(), it->value()});
    chart->setData(values);
}
void MaterialWindow::exportStats()
{
    const QString path = pdfPath(QStringLiteral("AUREA-statistiques.pdf"));
    if (path.isEmpty())
        return;
    QString error;
    if (!StockReports::writePdf(path, StockReports::statisticsHtml(materials), &error)) {
        failure(error);
        return;
    }
    notify(QStringLiteral("Statistiques exportées : ") + path);
}
