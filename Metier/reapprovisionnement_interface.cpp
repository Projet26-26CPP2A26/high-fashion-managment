#include "materialwindow.h"
#include "OrderAnimation.h"
#include "dialogskin.h"
#include "pdfexport.h"
#include "sound.h"
#include "ui_materialwindow.h"
#include "uihelpers.h"
#include <QDesktopServices>
#include <QUrl>

void MaterialWindow::purchaseOrder()
{
    const Material m = repo.findMaterial(selectedId(ui->alertsTable));
    if (!m.id()) {
        failure(QStringLiteral("Choisissez d’abord une matière dans le tableau."));
        return;
    }
    const double amount = ui->reorderQuantity->value();
    if (amount <= 0) {
        failure(QStringLiteral("La quantité à commander doit être positive."));
        return;
    }
    const QString path = pdfPath(QStringLiteral("AUREA-commande-") + m.lot() + QStringLiteral(".pdf"));
    if (path.isEmpty())
        return;
    QString error;
    if (!StockReports::writePdf(path, StockReports::purchaseHtml(m, amount), &error)) {
        failure(error);
        return;
    }
    ordered.insert(m.id(), amount);
    refresh();
    orderReady(path, QStringLiteral("%1 %2 de « %3 » chez %4.")
                         .arg(StockReports::quantity(amount), m.unit(), m.name(), m.supplier()));
}
void MaterialWindow::orderAll()
{
    const QString path = pdfPath(QStringLiteral("AUREA-commandes.pdf"));
    if (path.isEmpty())
        return;
    QString error;
    if (!StockReports::writePdf(path, StockReports::allOrdersHtml(materials), &error, true)) {
        failure(error);
        return;
    }
    int count = 0;
    for (const auto &m : materials)
        if (m.low()) {
            ordered.insert(m.id(), m.toOrder());
            ++count;
        }
    refresh();
    orderReady(path, QStringLiteral("%1 matière(s), une page par fournisseur.").arg(count));
}
void MaterialWindow::orderReady(const QString &path, const QString &text)
{
    if (DialogSkin::ask(this, QStringLiteral("Bon de commande prêt"),
                        text + QStringLiteral("\n\n1. Envoyez ce PDF au fournisseur.\n2. À l’arrivée de la "
                                              "marchandise, cliquez sur « Marchandise reçue » : le stock remontera."),
                        QStringLiteral("Ouvrir le PDF")))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
void MaterialWindow::showOrderStep()
{
    const Material m = repo.findMaterial(selectedId(ui->alertsTable));
    if (!m.id()) {
        orderGuide->setStep(ui->alertsTable->rowCount() ? 1 : 0);
        ui->alertHeadline->setText(ui->alertsTable->rowCount()
                                       ? QStringLiteral("Choisissez une matière dans le tableau")
                                       : QStringLiteral("Rien à commander : votre stock est au-dessus des seuils"));
        return;
    }
    const double amount = ui->reorderQuantity->value();
    if (ordered.contains(m.id())) {
        orderGuide->setStep(3);
        ui->alertHeadline->setText(
            QStringLiteral("%1 : %2 %3 commandés chez %4. À l’arrivée, cliquez sur « Marchandise reçue ».")
                .arg(m.name(), StockReports::quantity(ordered.value(m.id())), m.unit(), m.supplier()));
    } else {
        orderGuide->setStep(2);
        ui->alertHeadline->setText(
            QStringLiteral("%1 : %2. Commandez %3 %4 chez %5 (environ %6).")
                .arg(m.name(),
                     m.quantity() == 0
                         ? QStringLiteral("rupture")
                         : QStringLiteral("reste %1 %2").arg(StockReports::quantity(m.quantity()), m.unit()),
                     StockReports::quantity(amount), m.unit(), m.supplier(), StockReports::money(amount * m.price())));
    }
}
void MaterialWindow::showAlert(const QVector<Material> &list)
{
    if (isVisible())
        Sound::play("alerte");
    QStringList lines;
    for (const auto &m : list.mid(0, 8))
        lines.append(QStringLiteral("• %1 : %2 / %3 %4 (%5)")
                         .arg(m.name(), StockReports::quantity(m.quantity()), StockReports::quantity(m.minimum()),
                              m.unit(), state(m)));
    if (list.size() > 8)
        lines.append(QStringLiteral("… et %1 autre(s)").arg(list.size() - 8));
    auto *box =
        new DialogSkin(this, LivingIcon::Alert, QStringLiteral("Alerte de stock"),
                       QStringLiteral("%1 matière(s) sous le seuil minimal :\n\n").arg(list.size()) + lines.join('\n'),
                       QStringLiteral("Commander"), QStringLiteral("Plus tard"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QDialog::accepted, this, [this] { setPage(2); });
    box->open();
}
