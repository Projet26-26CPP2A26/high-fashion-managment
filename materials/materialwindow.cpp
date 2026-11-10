#include "materialwindow.h"
#include "ui_materialwindow.h"
#include "icons.h"
#include "qrservice.h"
#include "reports.h"
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QSet>
#include <QShortcut>
#include <QTimer>
#include <algorithm>

using StockReports::money;      // 85,00 DT
using StockReports::quantity;   // 42,5

// Pages de la fenêtre (même ordre que les boutons du menu et que ui->pages)
enum Page { Inventory, Editor, Alerts, Qr, Stats };

// ===================== Petites fonctions utilitaires =====================
namespace {

// Case de tableau. L'id de la matière est caché dedans pour retrouver la ligne sélectionnée.
QTableWidgetItem *cell(const QString &text, qint64 id = 0)
{
    auto *item = new QTableWidgetItem(text);
    item->setData(Qt::UserRole, id);
    return item;
}

// Remplit une ligne entière d'un tableau.
void setRow(QTableWidget *table, int row, const QStringList &values, qint64 id = 0)
{
    for (int column = 0; column < values.size(); ++column)
        table->setItem(row, column, cell(values[column], id));
}

// Style commun des tableaux + largeur de chaque colonne.
void configure(QTableWidget *table, const QList<int> &widths)
{
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setDefaultSectionSize(52);
    table->verticalHeader()->hide();
    table->setWordWrap(false);
    table->setAlternatingRowColors(true);
    for (int i = 0; i < widths.size(); ++i)
        table->setColumnWidth(i, widths[i]);
}

// Boîte « Confirmer / Annuler ». Renvoie true si l'utilisateur confirme.
bool confirm(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    QPushButton *yes = box.addButton("Confirmer", QMessageBox::AcceptRole);
    QPushButton *no = box.addButton("Annuler", QMessageBox::RejectRole);
    box.setDefaultButton(no);
    box.exec();
    return box.clickedButton() == yes;
}

QString state(const Material &m)
{
    if (m.quantity == 0) return "Rupture";
    if (m.low()) return "À commander";
    return "Disponible";
}

// Les pièces et les bobines se comptent sans décimales.
int decimalsFor(const QString &unit)
{
    return (unit == "pièce" || unit == "bobine") ? 0 : 2;
}

} // namespace

// ===================== Construction de la fenêtre =====================
MaterialWindow::MaterialWindow(InventoryRepository &repository, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MaterialWindow), repo(repository)
{
    ui->setupUi(this);

    QFile theme(":/materials/theme.qss");   // thème sombre / or
    if (theme.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(theme.readAll()));

    // --- Icônes ---
    const QList<QPushButton *> nav = navButtons();
    const Icons::Kind navIcons[] = {Icons::Grid, Icons::Document, Icons::Alert, Icons::Search, Icons::Chart};
    for (int i = 0; i < nav.size(); ++i) {
        nav[i]->setIcon(Icons::nav(navIcons[i]));
        nav[i]->setIconSize(QSize(20, 20));
    }
    ui->helpButton->setIcon(Icons::nav(Icons::Help));
    ui->helpButton->setIconSize(QSize(20, 20));
    const QColor gold("#c9b27c");
    ui->referencesIcon->setPixmap(Icons::pixmap(Icons::Grid, gold, 18));
    ui->alertsIcon->setPixmap(Icons::pixmap(Icons::Alert, gold, 18));
    ui->valueIcon->setPixmap(Icons::pixmap(Icons::Chart, gold, 18));
    ui->suppliersIcon->setPixmap(Icons::pixmap(Icons::People, gold, 18));

    // --- Réglages des champs et des tableaux ---
    ui->chartLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    ui->qrImage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->nameEdit->setMaxLength(120);
    ui->supplierEdit->setMaxLength(120);
    ui->colorEdit->setMaxLength(60);
    ui->lotEdit->setMaxLength(80);
    ui->editReasonField->setVisible(false);      // plus d'historique des mouvements : pas de motif
    ui->consumeReasonField->setVisible(false);
    ui->todayLabel->setText(QLocale().toString(QDate::currentDate(), "dddd d MMMM yyyy"));
    for (QLabel *label : {ui->referencesNote, ui->alertsNote, ui->valueNote, ui->suppliersNote})
        label->setWordWrap(true);
    configure(ui->materialsTable, {174, 84, 84, 60, 56, 60, 94, 158, 105, 115});
    configure(ui->alertsTable, {210, 210, 85, 85, 80, 85, 130});
    configure(ui->statsTable, {250, 180, 180, 200});

    // --- Menu latéral : un seul bouton coché à la fois ---
    auto *group = new QButtonGroup(this);
    for (int i = 0; i < nav.size(); ++i) {
        group->addButton(nav[i], i);
        connect(nav[i], &QPushButton::clicked, this, [this, i] {
            if (i == Editor) edit();   // « Fiche matière » ouvre un formulaire vide
            else setPage(i);
        });
    }
    connect(ui->helpButton, &QPushButton::clicked, this, &MaterialWindow::showHelp);

    // --- Page « Matières premières » ---
    connect(ui->newMaterialButton, &QPushButton::clicked, this, [this] { edit(); });
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &MaterialWindow::updateInventory);
    connect(ui->typeFilter, &QComboBox::currentIndexChanged, this, &MaterialWindow::updateInventory);
    connect(ui->stockFilter, &QComboBox::currentIndexChanged, this, &MaterialWindow::updateInventory);
    connect(ui->sortCombo, &QComboBox::currentIndexChanged, this, &MaterialWindow::updateInventory);
    connect(ui->materialsTable, &QTableWidget::itemSelectionChanged, this, &MaterialWindow::materialSelected);
    connect(ui->materialsTable, &QTableWidget::cellDoubleClicked, this, [this] { editSelected(); });
    connect(ui->editButton, &QPushButton::clicked, this, [this] { editSelected(); });
    connect(ui->deleteButton, &QPushButton::clicked, this, &MaterialWindow::removeSelected);
    connect(ui->movementButton, &QPushButton::clicked, this, [this] { moveStock(selectedId(ui->materialsTable)); });
    connect(ui->openQrButton, &QPushButton::clicked, this, [this] {
        selectQrLot(selectedId(ui->materialsTable));
        setPage(Qr);
    });
    connect(ui->exportButton, &QPushButton::clicked, this, &MaterialWindow::exportInventory);

    // --- Page « Fiche matière » ---
    connect(ui->saveButton, &QPushButton::clicked, this, &MaterialWindow::save);
    connect(ui->cancelButton, &QPushButton::clicked, this, [this] { setPage(Inventory); });

    // --- Page « Réapprovisionnement » ---
    connect(ui->alertsTable, &QTableWidget::itemSelectionChanged, this, &MaterialWindow::alertSelected);
    connect(ui->purchaseOrderButton, &QPushButton::clicked, this, &MaterialWindow::purchaseOrder);
    connect(ui->receiveButton, &QPushButton::clicked, this, [this] { moveStock(selectedId(ui->alertsTable), true); });

    // --- Page « Lots & QR codes » ---
    connect(ui->lotCombo, &QComboBox::currentIndexChanged, this, &MaterialWindow::updateLot);
    connect(ui->refreshQrButton, &QPushButton::clicked, this, &MaterialWindow::updateLot);
    connect(ui->findLotButton, &QPushButton::clicked, this, [this] { scanCode(ui->scanEdit->text()); });
    connect(ui->scanEdit, &QLineEdit::returnPressed, this, [this] { scanCode(ui->scanEdit->text()); });
    connect(ui->scanImageButton, &QPushButton::clicked, this, &MaterialWindow::scanImage);
    connect(ui->consumeButton, &QPushButton::clicked, this, &MaterialWindow::consume);
    connect(ui->saveQrButton, &QPushButton::clicked, this, &MaterialWindow::saveQr);
    connect(ui->printLabelButton, &QPushButton::clicked, this, &MaterialWindow::printLabel);

    // --- Page « Statistiques » ---
    connect(ui->statsPdfButton, &QPushButton::clicked, this, &MaterialWindow::exportStats);

    // --- Raccourcis clavier : Ctrl+N, Ctrl+F, Ctrl+S ---
    connect(new QShortcut(QKeySequence::New, this), &QShortcut::activated, this, [this] { edit(); });
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated, this, [this] {
        setPage(Inventory);
        ui->searchEdit->setFocus();
    });
    connect(new QShortcut(QKeySequence::Save, this), &QShortcut::activated, this, [this] {
        if (ui->pages->currentIndex() == Editor) save();
    });

    refresh();
    setPage(Inventory);
}

MaterialWindow::~MaterialWindow()
{
    delete ui;
}

// ===================== Navigation et messages =====================
void MaterialWindow::setSidebarVisible(bool visible)
{
    ui->sidebar->setVisible(visible);
}

QList<QPushButton *> MaterialWindow::navButtons() const
{
    return {ui->navInventory, ui->navEditor, ui->navAlerts, ui->navQr, ui->navStats};
}

QPushButton *MaterialWindow::helpButton() const
{
    return ui->helpButton;
}

void MaterialWindow::notify(const QString &message)
{
    ui->statusbar->showMessage(message, 12000);
}

void MaterialWindow::failure(const QString &message)
{
    QMessageBox::warning(this, "AUREA", message);
}

void MaterialWindow::showHelp()
{
    QMessageBox::information(this, "Aide — Gestion des matières",
        "• Matières premières : recherchez, filtrez et sélectionnez une ligne pour modifier, supprimer ou enregistrer une entrée / sortie.\n\n"
        "• Ajouter une matière : chaque matière a un lot unique. L’unité ne change plus une fois le lot enregistré.\n\n"
        "• Réapprovisionnement : les matières sous le seuil apparaissent ici. Le PDF prépare une commande sans modifier le stock.\n\n"
        "• Lots & QR : scannez le QR avec la caméra du téléphone : touchez le lien : la fiche du lot s’ouvre dans le navigateur, sans compte. "
        "Dans AUREA, tapez un lot ou ouvrez l’image d’un QR pour le retrouver. Les QR sont créés et lus par internet.\n\n"
        "• Statistiques : valeur du stock par type.\n\n"
        "Ctrl+N : nouvelle matière. Ctrl+F : rechercher. Ctrl+S : enregistrer le formulaire.\n\n"
        "Base active : " + repo.path());
}

void MaterialWindow::setPage(int index)
{
    struct PageText { const char *title; const char *subtitle; };
    static const PageText texts[] = {
        {"Matières premières", "Tissus, fils et fournitures : une vue claire de votre atelier."},
        {"Fiche matière", "Chaque détail compte, du fournisseur au dernier mètre de tissu."},
        {"Réapprovisionnement", "Anticipez les ruptures et préparez les prochains achats."},
        {"Lots & QR codes", "Identifiez vos lots et suivez leur utilisation en atelier."},
        {"Statistiques du stock", "Comprenez la valeur et les besoins de votre stock."},
    };
    if (index < Inventory || index > Stats) return;

    ui->pages->setCurrentIndex(index);
    ui->pageTitle->setText(texts[index].title);
    ui->pageSubtitle->setText(texts[index].subtitle);
    ui->breadcrumb->setText("ATELIER / MATIÈRES");
    ui->metricsRow->setVisible(index == Inventory || index == Alerts);
    ui->newMaterialButton->setVisible(index != Editor);
    navButtons()[index]->setChecked(true);

    // Le graphique est dessiné après l'affichage de la page, quand sa taille est connue.
    if (index == Stats)
        QTimer::singleShot(0, this, &MaterialWindow::updateStats);
}

void MaterialWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (ui->pages->currentIndex() == Stats)
        QTimer::singleShot(0, this, &MaterialWindow::updateStats);
}

// id de la matière sur la ligne sélectionnée (0 si aucune).
qint64 MaterialWindow::selectedId(QTableWidget *table) const
{
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0)) return 0;
    return table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

// Matière choisie dans la liste des lots (page QR).
Material MaterialWindow::currentLot()
{
    return repo.find(ui->lotCombo->currentData().toLongLong());
}

// ===================== Rafraîchir tout l'affichage =====================
void MaterialWindow::refresh()
{
    materials = repo.all();
    if (!repo.error().isEmpty()) {
        failure(repo.error());
        return;
    }
    updateMetrics();
    updateInventory();
    updateAlerts();
    updateLotList();
    updateStats();
    notify(QString("%1 alerte(s) de stock").arg(ui->alertsTable->rowCount()));
}

// Les 4 cartes de chiffres en haut de page.
void MaterialWindow::updateMetrics()
{
    int low = 0;
    double value = 0;
    QSet<QString> suppliers;
    for (const Material &m : materials) {
        if (m.low()) ++low;
        value += m.quantity * m.price;
        suppliers.insert(m.supplier.toLower());
    }
    ui->referencesValue->setText(QString::number(materials.size()));
    ui->alertsValue->setText(QString::number(low));
    ui->valueValue->setText(money(value));
    ui->suppliersValue->setText(QString::number(suppliers.size()));
}

// ===================== Page « Matières premières » =====================

// Matières qui correspondent à la recherche et aux filtres, triées.
QVector<Material> MaterialWindow::filteredMaterials() const
{
    const QString search = ui->searchEdit->text().trimmed();
    const int stock = ui->stockFilter->currentIndex();   // 0 tous, 1 sous le seuil, 2 disponible, 3 rupture
    QVector<Material> result;
    for (const Material &m : materials) {
        if (ui->typeFilter->currentIndex() > 0 && m.type != ui->typeFilter->currentText()) continue;
        if (stock == 1 && !m.low()) continue;
        if (stock == 2 && (m.low() || m.quantity == 0)) continue;
        if (stock == 3 && m.quantity != 0) continue;
        const QString allText = m.name + " " + m.type + " " + m.color + " " + m.supplier + " " + m.lot;
        if (!allText.contains(search, Qt::CaseInsensitive)) continue;
        result.push_back(m);
    }

    const int sort = ui->sortCombo->currentIndex();   // 0 nom, 1 stock, 2 prix, 3 type
    std::stable_sort(result.begin(), result.end(), [sort](const Material &a, const Material &b) {
        if (sort == 1 && a.quantity != b.quantity) return a.quantity < b.quantity;
        if (sort == 2 && a.price != b.price) return a.price < b.price;
        if (sort == 3 && a.type != b.type) return QString::localeAwareCompare(a.type, b.type) < 0;
        return QString::localeAwareCompare(a.name, b.name) < 0;   // sinon : par nom
    });
    return result;
}

void MaterialWindow::updateInventory()
{
    const qint64 previous = selectedId(ui->materialsTable);   // pour garder la même ligne sélectionnée
    const QVector<Material> rows = filteredMaterials();
    QTableWidget *table = ui->materialsTable;
    table->setRowCount(0);
    table->setRowCount(rows.size());

    int selectedRow = -1;
    for (int r = 0; r < rows.size(); ++r) {
        const Material &m = rows[r];
        if (m.id == previous) selectedRow = r;
        setRow(table, r, {m.name, m.type, m.color, quantity(m.quantity), quantity(m.minimum), m.unit,
                          money(m.price), m.supplier, m.lot, state(m)}, m.id);

        QFont bold = table->item(r, 0)->font();
        bold.setWeight(QFont::DemiBold);
        table->item(r, 0)->setFont(bold);
        for (int column : {3, 4, 6})   // nombres alignés à droite
            table->item(r, column)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Colonne « État » : rouge = rupture, jaune = à commander, vert = disponible
        QTableWidgetItem *status = table->item(r, 9);
        if (m.quantity == 0) {
            status->setForeground(QColor("#e8a9b0"));
            status->setBackground(QColor("#3a2a2e"));
        } else if (m.low()) {
            status->setForeground(QColor("#e6ce89"));
            status->setBackground(QColor("#3a3527"));
        } else {
            status->setForeground(QColor("#b7ccb3"));
            status->setBackground(QColor("#27322b"));
        }
    }

    if (selectedRow >= 0) table->selectRow(selectedRow);
    materialSelected();
    ui->resultCountLabel->setText(QString("%1 matière(s) affichée(s) sur %2").arg(rows.size()).arg(materials.size()));
    if (rows.isEmpty())
        ui->selectionLabel->setText(materials.isEmpty() ? "Commencez avec « Nouvelle matière »." : "Aucun résultat pour ces filtres.");
}

// Active les boutons d'action seulement si une ligne est sélectionnée.
void MaterialWindow::materialSelected()
{
    const Material m = repo.find(selectedId(ui->materialsTable));
    for (QPushButton *button : {ui->editButton, ui->deleteButton, ui->movementButton, ui->openQrButton})
        button->setEnabled(m.id != 0);
    if (m.id)
        ui->selectionLabel->setText(QString("ID %1 · Lot %2").arg(m.id).arg(m.lot));
    else
        ui->selectionLabel->setText("Sélectionnez une matière pour agir.");
}

void MaterialWindow::editSelected()
{
    const qint64 id = selectedId(ui->materialsTable);
    if (id) edit(id);
}

void MaterialWindow::removeSelected()
{
    const Material m = repo.find(selectedId(ui->materialsTable));
    if (!m.id) return;
    const QString question = QString("Supprimer « %1 » et son lot %2 ?").arg(m.name, m.lot);
    if (!confirm(this, "Supprimer une matière", question)) return;
    if (!repo.remove(m.id)) {
        failure(repo.error());
        return;
    }
    refresh();
    notify("Matière supprimée.");
}

// Entrée ou sortie de stock. receiptOnly = true : seulement une réception (page réappro).
void MaterialWindow::moveStock(qint64 id, bool receiptOnly)
{
    const Material m = repo.find(id);
    if (!m.id) return;

    QDialog dialog(this);
    dialog.setWindowTitle(receiptOnly ? "Réception de matières" : "Mouvement de stock");
    dialog.setMinimumWidth(480);
    QFormLayout form(&dialog);

    QLabel info(m.name + " · lot " + m.lot + "\nStock : " + quantity(m.quantity) + " " + m.unit);
    form.addRow(&info);

    QComboBox kind;
    kind.addItems({"Entrée / réception", "Sortie / consommation"});
    if (!receiptOnly) form.addRow("Mouvement", &kind);

    QDoubleSpinBox amount;
    amount.setRange(0, 9999999.99);
    amount.setDecimals(decimalsFor(m.unit));
    amount.setValue(receiptOnly ? std::max(1.0, m.minimum - m.quantity) : 1);
    amount.setSuffix(" " + m.unit);
    form.addRow("Quantité", &amount);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Enregistrer");
    buttons.button(QDialogButtonBox::Cancel)->setText("Annuler");
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;
    const double delta = (kind.currentIndex() == 1) ? -amount.value() : amount.value();   // sortie = négatif
    if (!repo.move(id, delta)) {
        failure(repo.error());
        return;
    }
    refresh();
    notify(QString("Mouvement enregistré pour le lot %1.").arg(m.lot));
}

void MaterialWindow::exportInventory()
{
    const QString path = pdfPath("AUREA-inventaire.pdf");
    if (path.isEmpty()) return;
    QString scope = QString("Filtres : %1 · %2").arg(ui->typeFilter->currentText(), ui->stockFilter->currentText());
    if (!ui->searchEdit->text().trimmed().isEmpty())
        scope += " · Recherche : " + ui->searchEdit->text().trimmed();
    QString error;
    if (!StockReports::writePdf(path, StockReports::inventoryHtml(filteredMaterials(), scope), &error, true)) {
        failure(error);
        return;
    }
    notify("Inventaire exporté : " + path);
}

// ===================== Page « Fiche matière » (ajout / modification) =====================

// id = 0 : nouvelle matière ; sinon : modification de la matière id.
void MaterialWindow::edit(qint64 id)
{
    Material m;
    if (id) {
        m = repo.find(id);
        if (!m.id) {
            failure("Matière introuvable.");
            return;
        }
    }
    editingId = id;
    ui->idEdit->setText(id ? QString::number(id) : "Attribué à l’enregistrement");
    ui->nameEdit->setText(m.name);
    ui->typeEdit->setCurrentText(id ? m.type : "Tissu");
    ui->colorEdit->setText(m.color);
    ui->supplierEdit->setText(m.supplier);
    ui->lotEdit->setText(m.lot);
    ui->quantityEdit->setValue(m.quantity);
    ui->minimumEdit->setValue(m.minimum);
    ui->priceEdit->setValue(m.price);
    ui->unitEdit->setCurrentText(m.unit);
    ui->unitEdit->setEnabled(!id);   // l'unité ne change plus après la création
    ui->validationLabel->clear();
    ui->formTitle->setText(id ? "Modifier — " + m.name : "Nouvelle matière");
    ui->saveButton->setText(id ? "Enregistrer les modifications" : "Enregistrer la matière");
    setPage(Editor);
    ui->nameEdit->setFocus();
}

void MaterialWindow::save()
{
    Material m;
    m.id = editingId;
    m.name = ui->nameEdit->text();
    m.type = ui->typeEdit->currentText();
    m.color = ui->colorEdit->text();
    m.supplier = ui->supplierEdit->text();
    m.lot = ui->lotEdit->text();
    m.quantity = ui->quantityEdit->value();
    m.minimum = ui->minimumEdit->value();
    m.price = ui->priceEdit->value();
    m.unit = ui->unitEdit->currentText();

    if (!repo.save(m)) {   // le dépôt vérifie les valeurs
        ui->validationLabel->setText(repo.error());
        return;
    }
    editingId = 0;
    refresh();
    setPage(Inventory);

    // Sélectionne la matière enregistrée dans le tableau.
    for (int r = 0; r < ui->materialsTable->rowCount(); ++r) {
        if (ui->materialsTable->item(r, 0)->data(Qt::UserRole).toLongLong() == m.id) {
            ui->materialsTable->selectRow(r);
            break;
        }
    }
    notify(QString("%1 a été enregistrée.").arg(m.name));
}

// ===================== Page « Réapprovisionnement » =====================
void MaterialWindow::updateAlerts()
{
    QTableWidget *table = ui->alertsTable;
    table->setRowCount(0);
    for (const Material &m : materials) {
        if (!m.low()) continue;
        const int row = table->rowCount();
        table->insertRow(row);
        setRow(table, row, {m.name, m.supplier, quantity(m.quantity), quantity(m.minimum), m.unit,
                            quantity(m.minimum - m.quantity), m.lot}, m.id);
        table->item(row, 0)->setForeground(QColor("#e2cf9f"));
    }

    const int count = table->rowCount();
    ui->alertHeadline->setText(count > 0 ? QString("%1 matière(s) à réapprovisionner").arg(count)
                                         : "Votre stock est au-dessus des seuils");
    if (count > 0) table->selectRow(0);
    alertSelected();
}

// Prépare la quantité à commander pour la matière sélectionnée.
void MaterialWindow::alertSelected()
{
    const Material m = repo.find(selectedId(ui->alertsTable));
    const bool selected = m.id != 0;
    ui->purchaseOrderButton->setEnabled(selected);
    ui->receiveButton->setEnabled(selected);
    ui->reorderQuantity->setEnabled(selected);
    ui->reorderQuantity->setDecimals(decimalsFor(m.unit));
    ui->reorderQuantity->setValue(selected ? std::max(1.0, m.minimum - m.quantity) : 0);
    ui->reorderQuantity->setSuffix(selected ? " " + m.unit : "");
}

void MaterialWindow::purchaseOrder()
{
    const Material m = repo.find(selectedId(ui->alertsTable));
    if (!m.id) return;
    const double amount = ui->reorderQuantity->value();
    if (amount <= 0) {
        failure("La quantité à commander doit être positive.");
        return;
    }
    const QString path = pdfPath("AUREA-commande-" + m.lot + ".pdf");
    if (path.isEmpty()) return;
    QString error;
    if (!StockReports::writePdf(path, StockReports::purchaseHtml(m, amount), &error)) {
        failure(error);
        return;
    }
    notify("Bon de commande préparé. Enregistrez la réception à l’arrivée des matières.");
}

// ===================== Page « Lots & QR codes » =====================

// Remplit la liste déroulante des lots en gardant le lot choisi avant.
void MaterialWindow::updateLotList()
{
    const qint64 previous = ui->lotCombo->currentData().toLongLong();
    ui->lotCombo->blockSignals(true);   // évite un updateLot() à chaque ajout
    ui->lotCombo->clear();
    for (const Material &m : materials)
        ui->lotCombo->addItem(m.lot + " — " + m.name, m.id);
    if (materials.isEmpty())
        ui->lotCombo->addItem("Aucun lot enregistré", 0);
    ui->lotCombo->setCurrentIndex(std::max(0, ui->lotCombo->findData(previous)));
    ui->lotCombo->blockSignals(false);
    updateLot();
}

void MaterialWindow::selectQrLot(qint64 id)
{
    const int index = ui->lotCombo->findData(id);
    if (index >= 0) ui->lotCombo->setCurrentIndex(index);
    updateLot();
}

// Affiche le QR et la fiche du lot choisi.
void MaterialWindow::updateLot()
{
    const Material m = currentLot();
    const bool found = m.id != 0;
    ui->saveQrButton->setEnabled(found);
    ui->printLabelButton->setEnabled(found);
    ui->consumeQuantity->setEnabled(found);
    ui->consumeButton->setEnabled(found && m.quantity > 0);

    if (!found) {
        currentQr = QImage();
        ui->qrImage->setPixmap(QPixmap());
        ui->qrImage->setText("Ajoutez une matière pour créer une étiquette QR.");
        ui->lotDetailsLabel->setText("Aucun lot enregistré.");
        ui->qrPayload->clear();
        return;
    }

    // Le QR en grand sert pour le PNG et le PDF ; à l'écran on l'affiche réduit.
    const QString link = QrService::link(m);
    currentQr = QrService::generate(link);
    if (currentQr.isNull()) {
        ui->qrImage->setPixmap(QPixmap());
        ui->qrImage->setText("QR indisponible : vérifiez la connexion internet puis « Recréer le QR ».");
    } else {
        ui->qrImage->setPixmap(QPixmap::fromImage(currentQr).scaled(270, 270, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    ui->qrImage->setToolTip(link);
    ui->qrTitle->setText("Lot " + m.lot);
    ui->qrPayload->setText("Fiche datée · Lot " + m.lot + " · " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm"));
    ui->lotDetailsLabel->setText(QString("%1\n%2 · %3\nFournisseur : %4\nStock : %5 %6 · Seuil : %7 %6\nPrix unitaire : %8")
        .arg(m.name, m.type, m.color, m.supplier, quantity(m.quantity), m.unit, quantity(m.minimum), money(m.price)));

    ui->consumeQuantity->setDecimals(decimalsFor(m.unit));
    ui->consumeQuantity->setSuffix(" " + m.unit);
    ui->consumeQuantity->setMaximum(m.quantity);
    ui->consumeQuantity->setValue(std::min(1.0, m.quantity));
}

// Retrouve un lot à partir du texte tapé ou lu dans un QR.
void MaterialWindow::scanCode(const QString &code)
{
    const QString lot = QrService::lotFromCode(code);
    if (lot.isEmpty()) {
        failure("Code invalide. Utilisez un QR AUREA ou le numéro d’un lot.");
        return;
    }
    const Material m = repo.findLot(lot);
    if (!m.id) {
        failure(QString("Le lot %1 est introuvable dans cette base.").arg(lot));
        return;
    }
    selectQrLot(m.id);
    setPage(Qr);
    notify(QString("Lot identifié : %1 · %2").arg(m.lot, m.supplier));
}

// Lit un QR dans une image choisie par l'utilisateur.
void MaterialWindow::scanImage()
{
    const QString path = QFileDialog::getOpenFileName(this, "Lire un QR code depuis une image", {},
                                                      "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;

    QString error;
    const QStringList codes = QrService::decode(QImage(path), &error);
    if (codes.isEmpty()) {
        failure(error);
        return;
    }
    for (const QString &code : codes) {   // le premier QR qui contient un lot AUREA
        const QString lot = QrService::lotFromCode(code);
        if (!lot.isEmpty()) {
            ui->scanEdit->setText(lot);
            scanCode(lot);
            return;
        }
    }
    failure("Ce QR code ne contient pas un lot AUREA.");
}

void MaterialWindow::consume()
{
    const Material m = currentLot();
    if (!m.id) return;
    const double amount = ui->consumeQuantity->value();
    if (amount <= 0) {
        failure("Indiquez une quantité positive.");
        return;
    }
    const QString question = QString("Déduire %1 %2 du lot %3 ?\nStock après consommation : %4 %2.")
                                 .arg(quantity(amount), m.unit, m.lot, quantity(m.quantity - amount));
    if (!confirm(this, "Confirmer la consommation", question)) return;
    if (!repo.move(m.id, -amount)) {
        failure(repo.error());
        return;
    }
    refresh();
    selectQrLot(m.id);
    notify("Consommation enregistrée. Stock du lot mis à jour.");
}

void MaterialWindow::saveQr()
{
    const Material m = currentLot();
    if (!m.id || currentQr.isNull()) return;
    QString path = QFileDialog::getSaveFileName(this, "Enregistrer le QR code", "AUREA-QR-" + m.lot + ".png", "Image PNG (*.png)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png", Qt::CaseInsensitive)) path += ".png";
    if (!currentQr.save(path, "PNG")) {
        failure("Impossible d’enregistrer cette image.");
        return;
    }
    notify("Étiquette QR enregistrée : " + path);
}

void MaterialWindow::printLabel()
{
    const Material m = currentLot();
    if (!m.id) return;
    const QString path = pdfPath("AUREA-etiquette-" + m.lot + ".pdf");
    if (path.isEmpty()) return;
    QString error;
    if (!StockReports::labelPdf(path, m, currentQr, &error)) {
        failure(error);
        return;
    }
    notify("Étiquette du lot exportée : " + path);
}

// ===================== Page « Statistiques » =====================
void MaterialWindow::updateStats()
{
    // Pour chaque type : nombre de matières, nombre sous le seuil, valeur du stock.
    struct Stat { int count = 0; int low = 0; double value = 0; };
    QMap<QString, Stat> stats;
    for (const Material &m : materials) {
        Stat &s = stats[m.type];
        s.count++;
        if (m.low()) s.low++;
        s.value += m.quantity * m.price;
    }

    ui->statsTable->setRowCount(stats.size());
    int r = 0;
    for (auto it = stats.cbegin(); it != stats.cend(); ++it, ++r)
        setRow(ui->statsTable, r, {it.key(), QString::number(it->count), QString::number(it->low), money(it->value)});

    QSize size = ui->chartLabel->size();
    if (size.width() < 400) size = QSize(950, 270);   // page pas encore affichée : taille par défaut
    ui->chartLabel->setPixmap(QPixmap::fromImage(StockReports::chart(materials, size)));
}

void MaterialWindow::exportStats()
{
    const QString path = pdfPath("AUREA-statistiques.pdf");
    if (path.isEmpty()) return;
    QString error;
    if (!StockReports::writePdf(path, StockReports::statisticsHtml(materials), &error)) {
        failure(error);
        return;
    }
    notify("Statistiques exportées : " + path);
}

// ===================== Commun =====================

// Demande où enregistrer un PDF (ajoute « .pdf » si besoin).
QString MaterialWindow::pdfPath(const QString &suggestion)
{
    QString path = QFileDialog::getSaveFileName(this, "Enregistrer le document PDF", suggestion, "Document PDF (*.pdf)");
    if (!path.isEmpty() && !path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
    return path;
}
