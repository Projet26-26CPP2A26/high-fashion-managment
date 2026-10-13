#include "materialwindow.h"
#include "ui_materialwindow.h"
#include "icons.h"
#include "qrservice.h"
#include "reports.h"
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QInputDialog>
#include <QLocale>
#include <QMap>
#include <QPixmap>
#include <QMessageBox>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {
QTableWidgetItem *cell(const QString &text,qint64 id=0) {
    auto *item=new QTableWidgetItem(text); item->setData(Qt::UserRole,id); return item;
}
QTableWidgetItem *number(double value,qint64 id=0) {
    auto *item=new QTableWidgetItem; item->setData(Qt::DisplayRole,value); item->setData(Qt::UserRole,id);
    item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); return item;
}
void configure(QTableWidget *table,const QList<int> &widths) {
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    table->verticalHeader()->setDefaultSectionSize(52); table->verticalHeader()->hide();
    table->setWordWrap(false); table->setAlternatingRowColors(true); table->setSortingEnabled(false);
    for(int i=0;i<widths.size();++i) table->setColumnWidth(i,widths[i]);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
}
bool confirm(QWidget *parent,const QString &title,const QString &text) {
    QMessageBox box(QMessageBox::Question,title,text,QMessageBox::NoButton,parent);
    auto *yes=box.addButton(QStringLiteral("Confirmer"),QMessageBox::AcceptRole);
    auto *no=box.addButton(QStringLiteral("Annuler"),QMessageBox::RejectRole);
    box.setDefaultButton(no); box.exec(); return box.clickedButton()==yes;
}
QString state(const Material &m) {
    return m.quantity==0?QStringLiteral("Rupture"):m.low()?QStringLiteral("À commander"):QStringLiteral("Disponible");
}
}

MaterialWindow::MaterialWindow(InventoryRepository &repository,QWidget *parent)
    :QMainWindow(parent),ui(new Ui::MaterialWindow),repo(repository) {
    ui->setupUi(this);
    QFile theme(QStringLiteral(":/materials/theme.qss"));
    if(theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()));
    // Icônes dessinées en code, partagées avec les autres modules (clair au repos, foncé sur le bouton doré actif).
    const struct {QPushButton *button; Icons::Kind icon;} navIcons[]={
        {ui->navInventory,Icons::Grid},{ui->navEditor,Icons::Document},{ui->navAlerts,Icons::Alert},
        {ui->navQr,Icons::Search},{ui->navStats,Icons::Chart},{ui->navHistory,Icons::Clock},{ui->helpButton,Icons::Help}};
    for(const auto &n:navIcons) {n.button->setIcon(Icons::nav(n.icon)); n.button->setIconSize(QSize(20,20));}
    const struct {QLabel *label; Icons::Kind icon;} cardIcons[]={
        {ui->referencesIcon,Icons::Grid},{ui->alertsIcon,Icons::Alert},{ui->valueIcon,Icons::Chart},{ui->suppliersIcon,Icons::People}};
    for(const auto &i:cardIcons) i.label->setPixmap(Icons::pixmap(i.icon,QColor("#c9b27c"),18));
    ui->chartLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    ui->qrImage->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    ui->nameEdit->setMaxLength(120); ui->supplierEdit->setMaxLength(120); ui->colorEdit->setMaxLength(60); ui->lotEdit->setMaxLength(80);
    ui->todayLabel->setText(QLocale().toString(QDate::currentDate(),QStringLiteral("dddd d MMMM yyyy")));
    for(QLabel *label:{ui->referencesNote,ui->alertsNote,ui->valueNote,ui->suppliersNote}) label->setWordWrap(true);
    auto *nav=new QButtonGroup(this); nav->setExclusive(true);
    const QList<QPushButton*> buttons={ui->navInventory,ui->navEditor,ui->navAlerts,ui->navQr,ui->navStats,ui->navHistory};
    for(int i=0;i<buttons.size();++i) {
        nav->addButton(buttons[i],i);
        connect(buttons[i],&QPushButton::clicked,this,[this,i] {if(i==1) edit(); else setPage(i);});
    }
    configure(ui->materialsTable,{174,84,84,60,56,60,94,158,105,115});
    configure(ui->alertsTable,{210,210,85,85,80,85,130});
    configure(ui->statsTable,{250,180,180,200});
    configure(ui->historyTable,{155,210,105,125,85,70,260});
    connect(ui->newMaterialButton,&QPushButton::clicked,this,[this]{edit();});
    connect(ui->searchEdit,&QLineEdit::textChanged,this,[this]{updateInventory();});
    connect(ui->typeFilter,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->stockFilter,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->sortCombo,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->editButton,&QPushButton::clicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) edit(id);});
    connect(ui->materialsTable,&QTableWidget::cellDoubleClicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) edit(id);});
    connect(ui->deleteButton,&QPushButton::clicked,this,&MaterialWindow::removeSelected);
    connect(ui->movementButton,&QPushButton::clicked,this,[this]{moveStock(selectedId(ui->materialsTable));});
    connect(ui->openQrButton,&QPushButton::clicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) {selectQrLot(id); setPage(3);}});
    connect(ui->materialsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const qint64 id=selectedId(ui->materialsTable); const bool enabled=id!=0;
        for(auto *b:{ui->editButton,ui->deleteButton,ui->movementButton,ui->openQrButton}) b->setEnabled(enabled);
        const Material m=repo.find(id);
        ui->selectionLabel->setText(m.id?QStringLiteral("ID %1 · Lot %2").arg(m.id).arg(m.lot):QStringLiteral("Sélectionnez une matière pour agir."));
    });
    connect(ui->saveButton,&QPushButton::clicked,this,&MaterialWindow::save);
    connect(ui->cancelButton,&QPushButton::clicked,this,[this]{setPage(0);});
    connect(ui->exportButton,&QPushButton::clicked,this,&MaterialWindow::exportInventory);
    connect(ui->purchaseOrderButton,&QPushButton::clicked,this,&MaterialWindow::purchaseOrder);
    connect(ui->receiveButton,&QPushButton::clicked,this,[this]{moveStock(selectedId(ui->alertsTable),true);});
    connect(ui->alertsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const Material m=repo.find(selectedId(ui->alertsTable)); const bool enabled=m.id!=0;
        ui->purchaseOrderButton->setEnabled(enabled); ui->receiveButton->setEnabled(enabled); ui->reorderQuantity->setEnabled(enabled);
        ui->reorderQuantity->setValue(enabled?std::max(1.0,m.minimum-m.quantity):0);
        ui->reorderQuantity->setDecimals((m.unit==QStringLiteral("pièce")||m.unit==QStringLiteral("bobine"))?0:2);
        ui->reorderQuantity->setSuffix(enabled?" "+m.unit:QString());
    });
    connect(ui->lotCombo,&QComboBox::currentIndexChanged,this,[this]{updateLot();});
    connect(ui->refreshQrButton,&QPushButton::clicked,this,[this]{updateLot();});
    connect(ui->openPhonePageButton,&QPushButton::clicked,this,[this]{if(!currentPhoneUrl.isEmpty()) QDesktopServices::openUrl(QUrl(currentPhoneUrl));});
    connect(ui->findLotButton,&QPushButton::clicked,this,[this]{scanCode(ui->scanEdit->text());});
    connect(ui->scanEdit,&QLineEdit::returnPressed,this,[this]{scanCode(ui->scanEdit->text());});
    connect(ui->scanImageButton,&QPushButton::clicked,this,&MaterialWindow::scanImage);
    connect(ui->consumeButton,&QPushButton::clicked,this,&MaterialWindow::consume);
    connect(ui->saveQrButton,&QPushButton::clicked,this,&MaterialWindow::saveQr);
    connect(ui->printLabelButton,&QPushButton::clicked,this,&MaterialWindow::printLabel);
    connect(ui->statsPdfButton,&QPushButton::clicked,this,&MaterialWindow::exportStats);
    connect(ui->helpButton,&QPushButton::clicked,this,[this]{
        QMessageBox::information(this,QStringLiteral("Aide — Gestion des matières"),QStringLiteral(
            "• Matières premières : recherchez, filtrez et sélectionnez une ligne pour modifier, supprimer ou enregistrer une entrée / sortie.\n\n"
            "• Ajouter une matière : chaque référence appartient à un lot unique. L’unité reste fixe une fois le lot enregistré.\n\n"
            "• Réapprovisionnement : les matières sous le seuil sont repérées automatiquement. Le PDF prépare une commande ; il ne modifie pas le stock. Enregistrez la réception à l’arrivée des marchandises.\n\n"
            "• Lots & QR : le téléphone ouvre une fiche datée, sans PC ni Wi-Fi partagé. Scannez avec la caméra et ouvrez le lien, sans compte ni connexion utilisateur. Première visite avec internet ; ensuite hors connexion dans le même navigateur tant que son cache est conservé. Les anciens QR internes restent lisibles dans AUREA. La consommation est confirmée avant déduction.\n\n"
            "• Statistiques : comparaison de la valeur des stocks par type, sans additionner des unités différentes.\n\n"
            "• Mouvements : historique des réceptions, corrections et consommations.\n\n"
            "Ctrl+N : nouvelle matière. Ctrl+F : rechercher. Ctrl+S : enregistrer le formulaire.\n\n"
            "Base active : ")+repo.path());
    });
    connect(new QShortcut(QKeySequence::New,this),&QShortcut::activated,this,[this]{edit();});
    connect(new QShortcut(QKeySequence::Find,this),&QShortcut::activated,this,[this]{setPage(0); ui->searchEdit->setFocus();});
    connect(new QShortcut(QKeySequence::Save,this),&QShortcut::activated,this,[this]{if(ui->pages->currentIndex()==1) save();});
    refresh(); setPage(0);
}
MaterialWindow::~MaterialWindow() {delete ui;}
void MaterialWindow::setSidebarVisible(bool visible) {ui->sidebar->setVisible(visible);}
QList<QPushButton *> MaterialWindow::navButtons() const {return {ui->navInventory,ui->navEditor,ui->navAlerts,ui->navQr,ui->navStats,ui->navHistory};}
QPushButton *MaterialWindow::helpButton() const {return ui->helpButton;}
void MaterialWindow::notify(const QString &message) {ui->statusbar->showMessage(message,12000);}
void MaterialWindow::failure(const QString &message) {QMessageBox::warning(this,QStringLiteral("AUREA"),message);}
qint64 MaterialWindow::selectedId(QTableWidget *table) const {
    const int row=table->currentRow(); return row>=0&&table->item(row,0)?table->item(row,0)->data(Qt::UserRole).toLongLong():0;
}
void MaterialWindow::setPage(int index) {
    static const QStringList titles={QStringLiteral("Matières premières"),QStringLiteral("Fiche matière"),QStringLiteral("Réapprovisionnement"),QStringLiteral("Lots & QR codes"),QStringLiteral("Statistiques du stock"),QStringLiteral("Mouvements de stock")};
    static const QStringList subtitles={QStringLiteral("Tissus, fils et fournitures : une vue claire de votre atelier."),QStringLiteral("Chaque détail compte, du fournisseur au dernier mètre de tissu."),QStringLiteral("Anticipez les ruptures et préparez les prochains achats."),QStringLiteral("Identifiez vos lots et suivez leur utilisation en atelier."),QStringLiteral("Comprenez la valeur et les besoins de votre stock."),QStringLiteral("La mémoire des entrées, des corrections et des consommations.")};
    if(index<0||index>=6) return;
    ui->pages->setCurrentIndex(index); ui->pageTitle->setText(titles[index]); ui->pageSubtitle->setText(subtitles[index]);
    ui->breadcrumb->setText(QStringLiteral("ATELIER / MATIÈRES"));
    ui->metricsRow->setVisible(index==0||index==2);
    const QList<QPushButton*> buttons={ui->navInventory,ui->navEditor,ui->navAlerts,ui->navQr,ui->navStats,ui->navHistory}; buttons[index]->setChecked(true);
    ui->newMaterialButton->setVisible(index!=1);
    if(index==4) QTimer::singleShot(0,this,[this]{updateStats();});
}
void MaterialWindow::refresh() {
    materials=repo.all(); if(!repo.error().isEmpty()) {failure(repo.error()); return;}
    int low=0; double value=0; QSet<QString> suppliers;
    for(const auto &m:materials) {low+=m.low(); value+=m.quantity*m.price; suppliers.insert(m.supplier.toCaseFolded());}
    ui->referencesValue->setText(QString::number(materials.size())); ui->alertsValue->setText(QString::number(low));
    ui->valueValue->setText(StockReports::money(value)); ui->suppliersValue->setText(QString::number(suppliers.size()));
    updateInventory();
    ui->alertsTable->setRowCount(0);
    for(const auto &m:materials) if(m.low()) {
        const int row=ui->alertsTable->rowCount(); ui->alertsTable->insertRow(row);
        const QStringList vals={m.name,m.supplier,StockReports::quantity(m.quantity),StockReports::quantity(m.minimum),m.unit,StockReports::quantity(m.minimum-m.quantity),m.lot};
        for(int c=0;c<vals.size();++c) ui->alertsTable->setItem(row,c,cell(vals[c],m.id));
        ui->alertsTable->item(row,0)->setForeground(QColor("#e2cf9f"));
    }
    const bool haveAlerts=ui->alertsTable->rowCount()>0;
    ui->purchaseOrderButton->setEnabled(haveAlerts); ui->receiveButton->setEnabled(haveAlerts); ui->reorderQuantity->setEnabled(haveAlerts);
    if(haveAlerts) ui->alertsTable->selectRow(0);
    ui->alertHeadline->setText(haveAlerts?QStringLiteral("%1 matière(s) à réapprovisionner").arg(low):QStringLiteral("Votre stock est au-dessus des seuils"));
    const qint64 lotId=ui->lotCombo->currentData().toLongLong();
    ui->lotCombo->blockSignals(true); ui->lotCombo->clear();
    for(const auto &m:materials) ui->lotCombo->addItem(m.lot+QStringLiteral(" — ")+m.name,m.id);
    if(materials.isEmpty()) ui->lotCombo->addItem(QStringLiteral("Aucun lot enregistré"),0);
    int position=ui->lotCombo->findData(lotId); ui->lotCombo->setCurrentIndex(position>=0?position:0); ui->lotCombo->blockSignals(false); updateLot();
    const QVector<Movement> history=repo.history(); if(!repo.error().isEmpty()) {failure(repo.error()); return;}
    ui->historyTable->setRowCount(history.size());
    for(int r=0;r<history.size();++r) {
        const auto &m=history[r]; const auto date=QDateTime::fromString(m.date,Qt::ISODateWithMs).toLocalTime();
        const QStringList vals={date.toString(QStringLiteral("dd/MM/yyyy  HH:mm")),m.name,m.lot,m.kind,(m.delta>0?QStringLiteral("+"):QString())+StockReports::quantity(m.delta),m.unit,m.reason};
        for(int c=0;c<vals.size();++c) ui->historyTable->setItem(r,c,cell(vals[c]));
        ui->historyTable->item(r,4)->setForeground(QColor(m.delta>0?"#afd0b1":"#e8bf9b"));
    }
    updateStats();
    notify(QStringLiteral("%1 · %2 alerte(s) de stock").arg(repo.path().endsWith(QStringLiteral("demo.sqlite"))?QStringLiteral("Démonstration — données d’exemple"):QStringLiteral("Stock enregistré"),QString::number(low)));
}
QVector<Material> MaterialWindow::filteredMaterials() const {
    QVector<Material> result; const QString search=ui->searchEdit->text().trimmed(); const int type=ui->typeFilter->currentIndex(),stock=ui->stockFilter->currentIndex();
    for(const auto &m:materials) {
        if(type>0&&m.type!=ui->typeFilter->currentText()) continue;
        if((stock==1&&!m.low())||(stock==2&&(m.low()||m.quantity==0))||(stock==3&&m.quantity!=0)) continue;
        if(!search.isEmpty() && !(m.name+" "+m.type+" "+m.color+" "+m.supplier+" "+m.lot+" "+QString::number(m.id)).contains(search,Qt::CaseInsensitive)) continue;
        result.push_back(m);
    }
    const int sort=ui->sortCombo->currentIndex();
    std::stable_sort(result.begin(),result.end(),[sort](const Material &a,const Material &b){
        if(sort==1&&a.quantity!=b.quantity) return a.quantity<b.quantity;
        if(sort==2&&a.price!=b.price) return a.price<b.price;
        if(sort==3&&a.type!=b.type) return QString::localeAwareCompare(a.type,b.type)<0;
        return QString::localeAwareCompare(a.name,b.name)<0;
    }); return result;
}
void MaterialWindow::updateInventory() {
    const qint64 old=selectedId(ui->materialsTable); const auto rows=filteredMaterials();
    ui->materialsTable->setRowCount(0); ui->materialsTable->setRowCount(rows.size()); int selected=-1;
    for(int r=0;r<rows.size();++r) {
        const auto &m=rows[r]; if(m.id==old) selected=r;
        const QStringList vals={m.name,m.type,m.color,StockReports::quantity(m.quantity),StockReports::quantity(m.minimum),m.unit,StockReports::money(m.price),m.supplier,m.lot,state(m)};
        for(int c=0;c<vals.size();++c) ui->materialsTable->setItem(r,c,cell(vals[c],m.id));
        ui->materialsTable->item(r,0)->setToolTip(QStringLiteral("ID %1 · %2").arg(m.id).arg(m.name));
        QFont font=ui->materialsTable->item(r,0)->font(); font.setWeight(QFont::DemiBold); ui->materialsTable->item(r,0)->setFont(font);
        for(int c:{3,4,6}) ui->materialsTable->item(r,c)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        const QColor accent=m.quantity==0?QColor("#e8a9b0"):m.low()?QColor("#e6ce89"):QColor("#b7ccb3");
        ui->materialsTable->item(r,9)->setForeground(accent);
        ui->materialsTable->item(r,9)->setBackground(m.quantity==0?QColor("#3a2a2e"):m.low()?QColor("#3a3527"):QColor("#27322b"));
    }
    if(selected>=0) ui->materialsTable->selectRow(selected);
    for(auto *b:{ui->editButton,ui->deleteButton,ui->movementButton,ui->openQrButton}) b->setEnabled(selected>=0);
    ui->resultCountLabel->setText(QStringLiteral("%1 matière(s) affichée(s) sur %2").arg(rows.size()).arg(materials.size()));
    if(rows.isEmpty()) ui->selectionLabel->setText(materials.isEmpty()?QStringLiteral("Commencez avec « Nouvelle matière »."):QStringLiteral("Aucun résultat pour ces filtres."));
    else if(selected<0) ui->selectionLabel->setText(QStringLiteral("Sélectionnez une matière pour agir."));
}
void MaterialWindow::edit(qint64 id) {
    Material m; if(id) {m=repo.find(id); if(!m.id) {failure(QStringLiteral("Matière introuvable.")); return;}}
    editingId=id; ui->idEdit->setText(id?QString::number(id):QStringLiteral("Attribué à l’enregistrement"));
    ui->nameEdit->setText(m.name); ui->typeEdit->setCurrentText(id?m.type:QStringLiteral("Tissu")); ui->colorEdit->setText(m.color); ui->supplierEdit->setText(m.supplier); ui->lotEdit->setText(m.lot);
    ui->quantityEdit->setValue(m.quantity); ui->minimumEdit->setValue(m.minimum); ui->priceEdit->setValue(m.price); ui->unitEdit->setCurrentText(m.unit); ui->unitEdit->setEnabled(!id);
    ui->editReason->setText(QStringLiteral("Correction d’inventaire")); ui->validationLabel->clear();
    ui->formTitle->setText(id?QStringLiteral("Modifier — ")+m.name:QStringLiteral("Nouvelle matière"));
    ui->saveButton->setText(id?QStringLiteral("Enregistrer les modifications"):QStringLiteral("Enregistrer la matière")); setPage(1); ui->nameEdit->setFocus();
}
void MaterialWindow::save() {
    Material m; m.id=editingId; m.name=ui->nameEdit->text(); m.type=ui->typeEdit->currentText(); m.color=ui->colorEdit->text();
    m.supplier=ui->supplierEdit->text(); m.lot=ui->lotEdit->text(); m.quantity=ui->quantityEdit->value(); m.minimum=ui->minimumEdit->value(); m.price=ui->priceEdit->value(); m.unit=ui->unitEdit->currentText();
    if(!repo.save(m,ui->editReason->text())) {ui->validationLabel->setText(repo.error()); return;}
    editingId=0; refresh(); setPage(0);
    for(int r=0;r<ui->materialsTable->rowCount();++r) if(ui->materialsTable->item(r,0)->data(Qt::UserRole).toLongLong()==m.id) {ui->materialsTable->selectRow(r); break;}
    notify(QStringLiteral("%1 a été enregistrée.").arg(m.name));
}
void MaterialWindow::removeSelected() {
    const Material m=repo.find(selectedId(ui->materialsTable)); if(!m.id) return;
    if(!confirm(this,QStringLiteral("Supprimer une matière"),QStringLiteral("Supprimer « %1 » et son lot %2 ?\nLe QR code ne retrouvera plus ce lot. L’historique des mouvements sera conservé.").arg(m.name,m.lot))) return;
    if(!repo.remove(m.id)) {failure(repo.error()); return;} refresh(); notify(QStringLiteral("Matière supprimée. Historique conservé."));
}
void MaterialWindow::moveStock(qint64 id,bool receiptOnly) {
    const Material m=repo.find(id); if(!m.id) return;
    QDialog dialog(this); dialog.setWindowTitle(receiptOnly?QStringLiteral("Réception de matières"):QStringLiteral("Mouvement de stock")); dialog.setMinimumWidth(480);
    QFormLayout form(&dialog); QLabel info(m.name+QStringLiteral(" · lot ")+m.lot+QStringLiteral("\nStock : ")+StockReports::quantity(m.quantity)+" "+m.unit); info.setWordWrap(true); form.addRow(&info);
    QComboBox kind; kind.addItems({QStringLiteral("Entrée / réception"),QStringLiteral("Sortie / consommation")}); if(!receiptOnly) form.addRow(QStringLiteral("Mouvement"),&kind);
    QDoubleSpinBox amount; amount.setRange(0,9999999.99); amount.setDecimals((m.unit==QStringLiteral("pièce")||m.unit==QStringLiteral("bobine"))?0:2); amount.setValue(receiptOnly?std::max(1.0,m.minimum-m.quantity):1); amount.setSuffix(" "+m.unit); form.addRow(QStringLiteral("Quantité"),&amount);
    QLineEdit reason; reason.setText(receiptOnly?QStringLiteral("Réception fournisseur"):QStringLiteral("Mouvement d’atelier")); form.addRow(QStringLiteral("Motif *"),&reason);
    QDialogButtonBox actions(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); actions.button(QDialogButtonBox::Ok)->setText(QStringLiteral("Enregistrer")); actions.button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Annuler")); form.addRow(&actions);
    connect(&actions,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(&actions,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted) return;
    if(!repo.move(id,(kind.currentIndex()==1?-1:1)*amount.value(),reason.text())) {failure(repo.error()); return;}
    refresh(); notify(QStringLiteral("Mouvement enregistré pour le lot %1.").arg(m.lot));
}
QString MaterialWindow::pdfPath(const QString &suggestion) {
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Enregistrer le document PDF"),suggestion,QStringLiteral("Document PDF (*.pdf)"));
    if(!path.isEmpty()&&!path.endsWith(QStringLiteral(".pdf"),Qt::CaseInsensitive)) path+=QStringLiteral(".pdf");
    return path;
}
void MaterialWindow::exportInventory() {
    const QString path=pdfPath(QStringLiteral("AUREA-inventaire.pdf")); if(path.isEmpty()) return;
    QString scope=QStringLiteral("Filtres : %1 · %2").arg(ui->typeFilter->currentText(),ui->stockFilter->currentText());
    if(!ui->searchEdit->text().trimmed().isEmpty()) scope+=QStringLiteral(" · Recherche : ")+ui->searchEdit->text().trimmed();
    QString error; if(!StockReports::writePdf(path,StockReports::inventoryHtml(filteredMaterials(),scope),&error,true)) {failure(error); return;}
    notify(QStringLiteral("Inventaire exporté : ")+path);
}
void MaterialWindow::purchaseOrder() {
    const Material m=repo.find(selectedId(ui->alertsTable)); if(!m.id) return;
    const double amount=ui->reorderQuantity->value();
    if(amount<=0) {failure(QStringLiteral("La quantité à commander doit être positive.")); return;}
    const QString path=pdfPath(QStringLiteral("AUREA-commande-")+m.lot+QStringLiteral(".pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::writePdf(path,StockReports::purchaseHtml(m,amount),&error)) {failure(error); return;}
    notify(QStringLiteral("Bon de commande préparé. Enregistrez la réception à l’arrivée des matières."));
}
void MaterialWindow::selectQrLot(qint64 id) {const int index=ui->lotCombo->findData(id); if(index>=0) ui->lotCombo->setCurrentIndex(index); updateLot();}
void MaterialWindow::updateLot() {
    const Material m=repo.find(ui->lotCombo->currentData().toLongLong()); const bool enabled=m.id!=0;
    for(auto *b:{ui->saveQrButton,ui->printLabelButton,ui->consumeButton}) b->setEnabled(enabled);
    ui->consumeQuantity->setEnabled(enabled); currentQr=QImage(); currentPhoneUrl.clear(); ui->openPhonePageButton->setEnabled(false);
    if(!enabled) {ui->qrImage->setPixmap(QPixmap()); ui->qrImage->setText(QStringLiteral("Ajoutez une matière pour créer une étiquette QR.")); ui->lotDetailsLabel->setText(QStringLiteral("Aucun lot enregistré.")); ui->qrPayload->clear(); return;}
    currentPhoneUrl=QrService::phonePayload(m);
    const QString code=currentPhoneUrl;
    currentQr=QrService::generate(code);
    // Keep whole QR modules and the white quiet zone when fitting the display.
    const int modules=currentQr.width()/8; const QImage display=QrService::generate(code,std::max(1,270/std::max(1,modules)));
    ui->qrImage->setPixmap(QPixmap::fromImage(display));
    ui->qrPayload->setText(QStringLiteral("Fiche datée · Lot %1 · %2").arg(m.lot,QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")));
    ui->qrPayload->setToolTip(code); ui->qrTitle->setText(QStringLiteral("Lot ")+m.lot);
    ui->openPhonePageButton->setEnabled(!currentPhoneUrl.isEmpty());
    ui->phoneHelpLabel->setText(QStringLiteral("Scannez avec la caméra du téléphone et ouvrez le lien : la fiche apparaît directement, sans compte. Internet (4G/5G) suffit, sans Wi-Fi partagé ni PC allumé. Après « Lecture hors connexion prête », le même navigateur peut lire les QR sans internet. Stock daté : recréez le QR après modification."));
    ui->lotDetailsLabel->setText(QStringLiteral("%1\n%2 · %3\nFournisseur : %4\nStock : %5 %6 · Seuil : %7 %6\nPrix unitaire : %8")
        .arg(m.name,m.type,m.color,m.supplier,StockReports::quantity(m.quantity),m.unit,StockReports::quantity(m.minimum),StockReports::money(m.price)));
    ui->consumeQuantity->setDecimals((m.unit==QStringLiteral("pièce")||m.unit==QStringLiteral("bobine"))?0:2); ui->consumeQuantity->setSuffix(" "+m.unit);
    ui->consumeButton->setEnabled(m.quantity>0); ui->consumeQuantity->setMaximum(m.quantity); ui->consumeQuantity->setValue(std::min(1.0,m.quantity));
}
void MaterialWindow::scanCode(const QString &code) {
    const QString lot=QrService::lotFromCode(code);
    if(lot.isEmpty()) {failure(QStringLiteral("Code invalide. Utilisez un QR AUREA ou l’identifiant d’un lot.")); return;}
    const Material m=repo.findLot(lot);
    if(!m.id) {failure(QStringLiteral("Le lot %1 est introuvable dans cette base.").arg(lot)); return;}
    selectQrLot(m.id); setPage(3); notify(QStringLiteral("Lot identifié : %1 · %2").arg(m.lot,m.supplier));
}
void MaterialWindow::scanImage() {
    const QString path=QFileDialog::getOpenFileName(this,QStringLiteral("Lire un QR code depuis une image"),{},QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)")); if(path.isEmpty()) return;
    QImageReader reader(path); reader.setAutoTransform(true); const QSize size=reader.size();
    if(size.width()>6000||size.height()>6000) reader.setScaledSize(size.scaled(2400,2400,Qt::KeepAspectRatio));
    QString error; const auto codes=QrService::decode(reader.read(),&error); if(codes.isEmpty()) {failure(error); return;}
    QStringList matching; for(const auto &code:codes) if(!QrService::lotFromCode(code).isEmpty()) matching.append(code);
    if(matching.isEmpty()) {failure(QStringLiteral("Ce QR code ne contient pas un lot AUREA.")); return;}
    QString choice=matching.first(); if(matching.size()>1) {
        bool ok=false; choice=QInputDialog::getItem(this,QStringLiteral("Plusieurs lots détectés"),QStringLiteral("Choisissez le code à lire"),matching,0,false,&ok); if(!ok) return;
    }
    ui->scanEdit->setText(choice); scanCode(choice);
}
void MaterialWindow::consume() {
    const Material m=repo.find(ui->lotCombo->currentData().toLongLong()); if(!m.id) return;
    const double amount=ui->consumeQuantity->value(); const QString reason=ui->consumeReason->text().trimmed();
    if(amount<=0||reason.isEmpty()) {failure(QStringLiteral("Indiquez une quantité positive et un motif de consommation.")); return;}
    if(!confirm(this,QStringLiteral("Confirmer la consommation"),QStringLiteral("Déduire %1 %2 du lot %3 ?\nStock après consommation : %4 %2.").arg(StockReports::quantity(amount),m.unit,m.lot,StockReports::quantity(m.quantity-amount)))) return;
    if(!repo.move(m.id,-amount,reason)) {failure(repo.error()); return;}
    refresh(); selectQrLot(m.id); notify(QStringLiteral("Consommation enregistrée. Stock du lot mis à jour."));
}
void MaterialWindow::saveQr() {
    const Material m=repo.find(ui->lotCombo->currentData().toLongLong()); if(!m.id||currentQr.isNull()) return;
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Enregistrer le QR code"),QStringLiteral("AUREA-QR-")+m.lot+QStringLiteral(".png"),QStringLiteral("Image PNG (*.png)")); if(path.isEmpty()) return;
    if(!path.endsWith(QStringLiteral(".png"),Qt::CaseInsensitive)) path+=QStringLiteral(".png");
    if(!currentQr.save(path,"PNG")) {failure(QStringLiteral("Impossible d’enregistrer cette image.")); return;} notify(QStringLiteral("Étiquette QR enregistrée : ")+path);
}
void MaterialWindow::printLabel() {
    const Material m=repo.find(ui->lotCombo->currentData().toLongLong()); if(!m.id) return;
    const QString path=pdfPath(QStringLiteral("AUREA-etiquette-")+m.lot+QStringLiteral(".pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::labelPdf(path,m,currentQr,&error)) {failure(error); return;} notify(QStringLiteral("Étiquette du lot exportée : ")+path);
}
void MaterialWindow::updateStats() {
    struct Stat {int count=0,low=0; double value=0;}; QMap<QString,Stat> stats;
    for(const auto &m:materials) {auto &s=stats[m.type]; ++s.count; s.low+=m.low(); s.value+=m.quantity*m.price;}
    ui->statsTable->setRowCount(stats.size()); int r=0;
    for(auto it=stats.cbegin();it!=stats.cend();++it,++r) {
        ui->statsTable->setItem(r,0,cell(it.key())); ui->statsTable->setItem(r,1,number(it->count)); ui->statsTable->setItem(r,2,number(it->low)); ui->statsTable->setItem(r,3,cell(StockReports::money(it->value)));
    }
    QSize target=ui->chartLabel->size(); if(target.width()<400) target=QSize(950,270);
    ui->chartLabel->setPixmap(QPixmap::fromImage(StockReports::chart(materials,target)));
}
void MaterialWindow::exportStats() {
    const QString path=pdfPath(QStringLiteral("AUREA-statistiques.pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::writePdf(path,StockReports::statisticsHtml(materials),&error)) {failure(error); return;} notify(QStringLiteral("Statistiques exportées : ")+path);
}
void MaterialWindow::resizeEvent(QResizeEvent *event) {QMainWindow::resizeEvent(event); if(ui&&ui->pages->currentIndex()==4) QTimer::singleShot(0,this,[this]{updateStats();});}
void MaterialWindow::renderPreviews(const QString &directory) {
    QDir().mkpath(directory); const QStringList names={"01-stock","02-formulaire","03-reapprovisionnement","04-lots-qr","05-statistiques","06-mouvements"};
    resize(1380,900);
    for(int i=0;i<names.size();++i) {
        if(i==1) edit(materials.isEmpty()?0:materials.first().id); else setPage(i);
        if(i==0 && !materials.isEmpty()) ui->materialsTable->selectRow(0);
        QApplication::processEvents(); if(i==4) updateStats();
        grab().save(QDir(directory).filePath(names[i]+QStringLiteral(".png")));
    }
    setPage(0); resize(1120,760); QApplication::processEvents(); grab().save(QDir(directory).filePath(QStringLiteral("07-stock-compact.png")));
    StockReports::writePdf(QDir(directory).filePath(QStringLiteral("08-inventaire.pdf")),StockReports::inventoryHtml(materials,QStringLiteral("Données de démonstration")),nullptr,true);
    StockReports::writePdf(QDir(directory).filePath(QStringLiteral("09-statistiques.pdf")),StockReports::statisticsHtml(materials));
    if(!materials.isEmpty()) {
        const Material sample=materials.first();
        StockReports::writePdf(QDir(directory).filePath(QStringLiteral("10-bon-commande.pdf")),StockReports::purchaseHtml(sample,12));
        const QImage qr=QrService::generate(QrService::phonePayload(sample));
        qr.save(QDir(directory).filePath(QStringLiteral("lot-exemple.png")));
        StockReports::labelPdf(QDir(directory).filePath(QStringLiteral("11-etiquette.pdf")),sample,qr);
    }
}
