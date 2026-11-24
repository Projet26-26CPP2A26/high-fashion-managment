#include "materialwindow.h"
#include "pdfexport.h"
#include "ui_materialwindow.h"
#include "UiAnimations.h"
#include "uihelpers.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QTableWidget>
#include <algorithm>

int MaterialWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    return row >= 0 && table->item(row, 0) ? table->item(row, 0)->data(Qt::UserRole).toInt() : 0;
}
QVector<Material> MaterialWindow::filteredMaterials() const
{
    QVector<Material> result;
    const QString search = ui->searchEdit->text().trimmed();
    const int type = ui->typeFilter->currentIndex(), stock = ui->stockFilter->currentIndex();
    for (const auto &m : materials) {
        if (type > 0 && m.type() != ui->typeFilter->currentText())
            continue;
        if ((stock == 1 && !m.low()) || (stock == 2 && (m.low() || m.quantity() == 0)) ||
            (stock == 3 && m.quantity() != 0))
            continue;
        if (!search.isEmpty() && !(m.name() + " " + m.type() + " " + m.color() + " " + m.supplier() + " " + m.lot() +
                                   " " + QString::number(m.id()))
                                      .contains(search, Qt::CaseInsensitive))
            continue;
        result.push_back(m);
    }
    const int sort = ui->sortCombo->currentIndex();
    std::stable_sort(result.begin(), result.end(), [sort](const Material &a, const Material &b) {
        if (sort == 1 && a.quantity() != b.quantity())
            return a.quantity() < b.quantity();
        if (sort == 2 && a.price() != b.price())
            return a.price() < b.price();
        if (sort == 3 && a.type() != b.type())
            return QString::localeAwareCompare(a.type(), b.type()) < 0;
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    return result;
}
void MaterialWindow::updateInventory()
{
    const int old = selectedId(ui->materialsTable);
    const auto rows = filteredMaterials();
    ui->materialsTable->setRowCount(0);
    ui->materialsTable->setRowCount(rows.size());
    int selected = -1;
    for (int r = 0; r < rows.size(); ++r) {
        const auto &m = rows[r];
        if (m.id() == old)
            selected = r;
        const QStringList vals = {m.name(),
                                  m.type(),
                                  m.color(),
                                  StockReports::quantity(m.quantity()),
                                  StockReports::quantity(m.minimum()),
                                  m.unit(),
                                  StockReports::money(m.price()),
                                  m.supplier(),
                                  m.lot(),
                                  state(m)};
        for (int c = 0; c < vals.size(); ++c)
            ui->materialsTable->setItem(r, c, cell(vals[c], m.id()));
        ui->materialsTable->item(r, 0)->setToolTip(QStringLiteral("ID %1 · %2").arg(m.id()).arg(m.name()));
        QFont font = ui->materialsTable->item(r, 0)->font();
        font.setWeight(QFont::DemiBold);
        ui->materialsTable->item(r, 0)->setFont(font);
        for (int c : {3, 4, 6})
            ui->materialsTable->item(r, c)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        const QColor accent = m.quantity() == 0 ? QColor("#e8a9b0") : m.low() ? QColor("#e6ce89") : QColor("#b7ccb3");
        ui->materialsTable->item(r, 9)->setForeground(accent);
        ui->materialsTable->item(r, 9)->setBackground(m.quantity() == 0 ? QColor("#3a2a2e")
                                                      : m.low()         ? QColor("#3a3527")
                                                                        : QColor("#27322b"));
    }
    if (selected >= 0)
        ui->materialsTable->selectRow(selected);
    for (auto *b : {ui->editButton, ui->deleteButton, ui->stockButton, ui->openQrButton})
        b->setEnabled(selected >= 0);
    ui->resultCountLabel->setText(
        QStringLiteral("%1 matière(s) affichée(s) sur %2").arg(rows.size()).arg(materials.size()));
    if (rows.isEmpty())
        ui->selectionLabel->setText(materials.isEmpty() ? QStringLiteral("Commencez avec « Nouvelle matière ».")
                                                        : QStringLiteral("Aucun résultat pour ces filtres."));
    else if (selected < 0)
        ui->selectionLabel->setText(QStringLiteral("Sélectionnez une matière pour agir."));
}
void MaterialWindow::edit(int id)
{
    Material m;
    if (id) {
        m = repo.findMaterial(id);
        if (!m.id()) {
            failure(QStringLiteral("Matière introuvable."));
            return;
        }
    }
    editingId = id;
    ui->idEdit->setText(id ? QString::number(id) : QStringLiteral("Attribué à l’enregistrement"));
    ui->nameEdit->setText(m.name());
    ui->typeEdit->setCurrentText(id ? m.type() : QStringLiteral("Tissu"));
    ui->colorEdit->setText(m.color());
    ui->supplierEdit->setText(m.supplier());
    ui->lotEdit->setText(m.lot());
    ui->quantityEdit->setValue(m.quantity());
    ui->minimumEdit->setValue(m.minimum());
    ui->priceEdit->setValue(m.price());
    ui->unitEdit->setCurrentText(m.unit());
    ui->unitEdit->setEnabled(!id);
    ui->validationLabel->clear();
    ui->formTitle->setText(id ? QStringLiteral("Modifier — ") + m.name() : QStringLiteral("Nouvelle matière"));
    ui->saveButton->setText(id ? QStringLiteral("Enregistrer les modifications")
                               : QStringLiteral("Enregistrer la matière"));
    setPage(1);
    ui->nameEdit->setFocus();
}
void MaterialWindow::save()
{
    Material m;
    m.setId(editingId);
    m.setName(ui->nameEdit->text());
    m.setType(ui->typeEdit->currentText());
    m.setColor(ui->colorEdit->text());
    m.setSupplier(ui->supplierEdit->text());
    m.setLot(ui->lotEdit->text());
    m.setQuantity(ui->quantityEdit->value());
    m.setMinimum(ui->minimumEdit->value());
    m.setPrice(ui->priceEdit->value());
    m.setUnit(ui->unitEdit->currentText());
    const bool saved = editingId ? repo.updateMaterial(m) : repo.addMaterial(m);
    if (!saved) {
        ui->validationLabel->setText(repo.error());
        UiAnimations::shake(ui->saveButton);
        return;
    }
    editingId = 0;
    refresh();
    setPage(0);
    for (int r = 0; r < ui->materialsTable->rowCount(); ++r)
        if (ui->materialsTable->item(r, 0)->data(Qt::UserRole).toInt() == m.id()) {
            ui->materialsTable->selectRow(r);
            break;
        }
    notify(QStringLiteral("%1 a été enregistrée.").arg(m.name()));
}
void MaterialWindow::removeSelected()
{
    const Material m = repo.findMaterial(selectedId(ui->materialsTable));
    if (!m.id())
        return;
    if (!confirm(this, QStringLiteral("Supprimer une matière"),
                 QStringLiteral("Supprimer « %1 » et son lot %2 ?\nLe QR code ne retrouvera plus ce lot.")
                     .arg(m.name(), m.lot())))
        return;
    if (!repo.deleteMaterial(m.id())) {
        failure(repo.error());
        return;
    }
    refresh();
    notify(QStringLiteral("Matière supprimée."));
}
void MaterialWindow::changeStock(int id, bool receiptOnly)
{
    const Material m = repo.findMaterial(id);
    if (!m.id())
        return;
    QDialog dialog(this);
    dialog.setWindowTitle(receiptOnly ? QStringLiteral("Réception de matières")
                                      : QStringLiteral("Entrée / sortie de stock"));
    dialog.setMinimumWidth(480);
    QFormLayout form(&dialog);
    QLabel info(m.name() + QStringLiteral(" · lot ") + m.lot() + QStringLiteral("\nStock : ") +
                StockReports::quantity(m.quantity()) + " " + m.unit());
    info.setWordWrap(true);
    form.addRow(&info);
    QComboBox kind;
    kind.addItems({QStringLiteral("Entrée / réception"), QStringLiteral("Sortie / consommation")});
    if (!receiptOnly)
        form.addRow(QStringLiteral("Opération"), &kind);
    QDoubleSpinBox amount;
    amount.setRange(0, 9999999.99);
    amount.setDecimals((m.unit() == QStringLiteral("pièce") || m.unit() == QStringLiteral("bobine")) ? 0 : 2);
    amount.setValue(receiptOnly ? ordered.value(m.id(), m.low() ? m.toOrder() : 1) : 1);
    amount.setSuffix(" " + m.unit());
    form.addRow(QStringLiteral("Quantité"), &amount);
    QDialogButtonBox actions(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    actions.button(QDialogButtonBox::Ok)->setText(QStringLiteral("Enregistrer"));
    actions.button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Annuler"));
    form.addRow(&actions);
    connect(&actions, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!repo.changeQuantity(id, (kind.currentIndex() == 1 ? -1 : 1) * amount.value())) {
        failure(repo.error());
        return;
    }
    if (receiptOnly)
        ordered.remove(id);
    refresh();
    const Material now = repo.findMaterial(id);
    notify(
        QStringLiteral("%1 : nouveau stock %2 %3").arg(now.name(), StockReports::quantity(now.quantity()), now.unit()));
}
