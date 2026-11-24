#include "materialwindow.h"
#include <QSqlDatabase>
#include "ui_materialwindow.h"
#include "icons.h"
#include "qrcode.h"
#include "pdfexport.h"
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
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QShortcut>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include "uihelpers.h"
#include "ChartAnimation.h"
#include "ClosingAnimation.h"
#include "ConnectionAnimation.h"
#include "dialogskin.h"
#include "LogoAnimation.h"
#include "TableAnimation.h"
#include "QrAnimation.h"
#include "TransitionAnimation.h"
#include "UiAnimations.h"
#include "bornepage.h"
#include "sound.h"
#include <QCloseEvent>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>


MaterialWindow::MaterialWindow(QWidget *parent)
    :QMainWindow(parent),ui(new Ui::MaterialWindow) {
    ui->setupUi(this);
    QFile theme(QStringLiteral(":/assets/theme.qss"));
    if(theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()));
    livingLogo=new LivingLogo(ui->logoMark);
    spool=new SpoolAnimation; ui->sidebarLayout->insertWidget(ui->sidebarLayout->indexOf(ui->sidebarNote),spool);
    chart=new ChartAnimation(ui->chartLabel);
    navBorne=new QPushButton(QStringLiteral("Borne Arduino"));
    navBorne->setProperty("nav",true); navBorne->setCheckable(true);
    ui->sidebarLayout->insertWidget(ui->sidebarLayout->indexOf(ui->navStats)+1,navBorne);
    borne=new BornePage; borne->ask=[this](const QString &message){return arduinoAnswer(message);}; ui->pages->addWidget(borne);
    qrAnimation=new QrAnimation(ui->qrImage);
    const QList<QPushButton*> iconButtons={ui->navInventory,ui->navEditor,ui->navAlerts,ui->navQr,ui->navStats,navBorne,ui->helpButton};
    const QList<Icons::Kind> buttonIcons={Icons::Grid,Icons::Document,Icons::Alert,Icons::Search,Icons::Chart,Icons::Assign,Icons::Help};
    for(int i=0;i<iconButtons.size();++i) {iconButtons[i]->setIcon(Icons::nav(buttonIcons[i])); iconButtons[i]->setIconSize(QSize(20,20));}
    const QList<QLabel*> cardLabels={ui->referencesIcon,ui->alertsIcon,ui->valueIcon,ui->suppliersIcon};
    const QList<Icons::Kind> cardIcons={Icons::Grid,Icons::Alert,Icons::Chart,Icons::People};
    for(int i=0;i<cardLabels.size();++i) cardLabels[i]->setPixmap(Icons::pixmap(cardIcons[i],QColor("#c9b27c"),18));
    ui->chartLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    ui->qrImage->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    ui->nameEdit->setMaxLength(120); ui->supplierEdit->setMaxLength(120); ui->colorEdit->setMaxLength(60); ui->lotEdit->setMaxLength(80);
    ui->todayLabel->setText(QLocale().toString(QDate::currentDate(),QStringLiteral("dddd d MMMM yyyy")));
    for(QLabel *label:{ui->referencesNote,ui->alertsNote,ui->valueNote,ui->suppliersNote}) label->setWordWrap(true);
    auto *nav=new QButtonGroup(this); nav->setExclusive(true);
    const QList<QPushButton*> buttons=navButtons();
    new UiAnimations::NavPill(ui->sidebar,buttons);
    for(int i=0;i<buttons.size();++i) {
        nav->addButton(buttons[i],i);
        connect(buttons[i],&QPushButton::clicked,this,[this,i] {if(i==1) edit(); else setPage(i);});
    }
    configure(ui->materialsTable,{174,84,84,60,56,60,94,158,105,115});
    configure(ui->alertsTable,{200,190,75,75,75,80,120,120});
    configure(ui->statsTable,{250,180,180,200});
    connect(ui->newMaterialButton,&QPushButton::clicked,this,[this]{edit();});
    connect(ui->searchEdit,&QLineEdit::textChanged,this,[this]{updateInventory();});
    connect(ui->typeFilter,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->stockFilter,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->sortCombo,&QComboBox::currentIndexChanged,this,[this]{updateInventory();});
    connect(ui->editButton,&QPushButton::clicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) edit(id);});
    connect(ui->materialsTable,&QTableWidget::cellDoubleClicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) edit(id);});
    connect(ui->deleteButton,&QPushButton::clicked,this,&MaterialWindow::removeSelected);
    connect(ui->stockButton,&QPushButton::clicked,this,[this]{changeStock(selectedId(ui->materialsTable));});
    connect(ui->openQrButton,&QPushButton::clicked,this,[this]{const auto id=selectedId(ui->materialsTable); if(id) {selectQrLot(id); setPage(3);}});
    connect(ui->materialsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const int id=selectedId(ui->materialsTable); const bool enabled=id!=0;
        for(auto *b:{ui->editButton,ui->deleteButton,ui->stockButton,ui->openQrButton}) b->setEnabled(enabled);
        const Material m=repo.findMaterial(id);
        ui->selectionLabel->setText(m.id()?QStringLiteral("ID %1 · Lot %2").arg(m.id()).arg(m.lot()):QStringLiteral("Sélectionnez une matière pour agir."));
    });
    connect(ui->saveButton,&QPushButton::clicked,this,&MaterialWindow::save);
    connect(ui->cancelButton,&QPushButton::clicked,this,[this]{setPage(0);});
    connect(ui->exportButton,&QPushButton::clicked,this,&MaterialWindow::exportInventory);
    connect(ui->purchaseOrderButton,&QPushButton::clicked,this,&MaterialWindow::purchaseOrder);
    connect(ui->orderAllButton,&QPushButton::clicked,this,&MaterialWindow::orderAll);
    connect(ui->receiveButton,&QPushButton::clicked,this,[this]{changeStock(selectedId(ui->alertsTable),true);});
    connect(ui->alertsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const Material m=repo.findMaterial(selectedId(ui->alertsTable)); const bool enabled=m.id()!=0;
        ui->purchaseOrderButton->setEnabled(enabled); ui->receiveButton->setEnabled(enabled); ui->reorderQuantity->setEnabled(enabled);
        ui->reorderQuantity->setValue(m.toOrder());
        ui->reorderQuantity->setDecimals((m.unit()==QStringLiteral("pièce")||m.unit()==QStringLiteral("bobine"))?0:2);
        ui->reorderQuantity->setSuffix(enabled?" "+m.unit():QString());
    });
    connect(ui->lotCombo,&QComboBox::currentIndexChanged,this,[this]{updateLot();});
    connect(ui->refreshQrButton,&QPushButton::clicked,this,[this]{updateLot();});
    connect(ui->openPhonePageButton,&QPushButton::clicked,this,[this]{if(!currentQrText.isEmpty()) QDesktopServices::openUrl(QUrl(currentQrText));});
    connect(ui->findLotButton,&QPushButton::clicked,this,[this]{scanCode(ui->scanEdit->text());});
    connect(ui->scanEdit,&QLineEdit::returnPressed,this,[this]{scanCode(ui->scanEdit->text());});
    connect(ui->scanImageButton,&QPushButton::clicked,this,&MaterialWindow::scanImage);
    connect(ui->consumeButton,&QPushButton::clicked,this,&MaterialWindow::consume);
    connect(ui->saveQrButton,&QPushButton::clicked,this,&MaterialWindow::saveQr);
    connect(ui->printLabelButton,&QPushButton::clicked,this,&MaterialWindow::printLabel);
    connect(ui->statsPdfButton,&QPushButton::clicked,this,&MaterialWindow::exportStats);
    connect(ui->helpButton,&QPushButton::clicked,this,[this]{
        DialogSkin::info(this,QStringLiteral("Aide — Gestion des matières"),QStringLiteral(
            "• Matières premières : recherchez, filtrez et sélectionnez une ligne pour modifier, supprimer ou enregistrer une entrée / sortie.\n\n"
            "• Ajouter une matière : chaque référence appartient à un lot unique. L’unité reste fixe une fois le lot enregistré.\n\n"
            "• Réapprovisionnement : les matières sous le seuil sont repérées automatiquement. Une alerte s’affiche dès qu’une matière passe sous son seuil. « Tout commander » crée un bon par fournisseur. Le PDF prépare une commande ; il ne modifie pas le stock. Enregistrez la réception à l’arrivée des marchandises.\n\n"
            "• Lots & QR : scannez le QR avec l’appareil photo du téléphone pour afficher la fiche du lot. Recréez le QR après une modification du stock. La consommation est confirmée avant déduction.\n\n"
            "• Statistiques : comparaison de la valeur des stocks par type, sans additionner des unités différentes.\n\n"
            "Ctrl+N : nouvelle matière. Ctrl+F : rechercher. Ctrl+S : enregistrer le formulaire.\n\n"
            "Base active : ")+QSqlDatabase::database().databaseName());
    });
    connect(new QShortcut(QKeySequence::New,this),&QShortcut::activated,this,[this]{edit();});
    connect(new QShortcut(QKeySequence::Find,this),&QShortcut::activated,this,[this]{setPage(0); ui->searchEdit->setFocus();});
    connect(new QShortcut(QKeySequence::Save,this),&QShortcut::activated,this,[this]{if(ui->pages->currentIndex()==1) save();});
    arduinoLight=new LinkLight(this); ui->statusbar->addPermanentWidget(arduinoLight);
    arduinoLabel=new QLabel(this); arduinoLabel->setContentsMargins(0,0,12,0); ui->statusbar->addPermanentWidget(arduinoLabel);
    connect(A.getserial(),&QSerialPort::readyRead,this,&MaterialWindow::readArduino);
    connect(A.getserial(),&QSerialPort::errorOccurred,this,[this](QSerialPort::SerialPortError e){if(e==QSerialPort::ResourceError) QTimer::singleShot(0,this,[this]{A.close_arduino(); connectArduino();});});
    connect(A.getsimulator(),&QTcpSocket::readyRead,this,&MaterialWindow::readArduino);
    connect(A.getsimulator(),&QTcpSocket::connected,this,[this]{arduinoData.clear(); setArduinoState(true,QStringLiteral("Arduino simulé connecté (Wokwi)"));});
    connect(A.getsimulator(),&QTcpSocket::disconnected,this,[this]{arduinoData.clear(); setArduinoState(false,QStringLiteral("Arduino non connecté"));});
    auto *arduinoTimer=new QTimer(this); connect(arduinoTimer,&QTimer::timeout,this,&MaterialWindow::connectArduino); arduinoTimer->start(3000);
    connectArduino();
    UiAnimations::HoverGlow::install(this);
    for(QTableWidget *table:findChildren<QTableWidget*>()) new TableAnimation(table);
    refresh(); setPage(0);
}
void MaterialWindow::setArduinoState(bool on,const QString &text) {
    arduinoLabel->setText(text); arduinoLight->setOn(on); borne->setLink(on,text);
}
void MaterialWindow::playOpening() {
    opening=true;
    livingLogo->hide(); Sound::play("ouverture");
    const int low=int(lowIds.size());
    auto *intro=new TransitionAnimation(this,ui->logoMark,QStringLiteral("AUREA"),low?QStringLiteral("Bonjour · %1 matière(s) à commander").arg(low):QStringLiteral("Bonjour · votre stock est complet"));
    connect(intro,&TransitionAnimation::finished,this,[this]{
        livingLogo->show();
        opening=false;
        for(QLabel *label:{ui->referencesValue,ui->alertsValue,ui->suppliersValue,ui->valueValue}) label->setProperty("shown",0.0);
        refresh();
        if(!waitingAlerts.isEmpty()) {const auto list=waitingAlerts; waitingAlerts.clear(); showAlert(list);}
    });
}
void MaterialWindow::closeEvent(QCloseEvent *event) {
    if(leaving||!isVisible()||qApp->isSavingSession()) {event->accept(); return;}
    event->ignore();
    if(findChild<ClosingAnimation*>()) return;
    Sound::play("fermeture");
    connect(new ClosingAnimation(this),&ClosingAnimation::finished,this,[this]{leaving=true; close();});
}
MaterialWindow::~MaterialWindow() {A.getserial()->disconnect(this); A.getsimulator()->disconnect(this); A.close_arduino(); delete ui;}
void MaterialWindow::setSidebarVisible(bool visible) {ui->sidebar->setVisible(visible);}
QList<QPushButton *> MaterialWindow::navButtons() const {return {ui->navInventory,ui->navEditor,ui->navAlerts,ui->navQr,ui->navStats,navBorne};}
QPushButton *MaterialWindow::helpButton() const {return ui->helpButton;}
void MaterialWindow::notify(const QString &message) {ui->statusbar->showMessage(message,12000); if(isVisible()) UiAnimations::toast(this,message);}
void MaterialWindow::failure(const QString &message) {DialogSkin::warn(this,QStringLiteral("AUREA"),message);}
void MaterialWindow::setPage(int index) {
    static const QStringList titles={QStringLiteral("Matières premières"),QStringLiteral("Fiche matière"),QStringLiteral("Réapprovisionnement"),QStringLiteral("Lots & QR codes"),QStringLiteral("Statistiques du stock"),QStringLiteral("Borne Arduino")};
    static const QStringList subtitles={QStringLiteral("Tissus, fils et fournitures : une vue claire de votre atelier."),QStringLiteral("Chaque détail compte, du fournisseur au dernier mètre de tissu."),QStringLiteral("Anticipez les ruptures et préparez les prochains achats."),QStringLiteral("Identifiez vos lots et suivez leur utilisation en atelier."),QStringLiteral("Comprenez la valeur et les besoins de votre stock."),QStringLiteral("Consultez et mettez à jour le stock depuis la borne de l’atelier.")};
    if(index<0||index>=6) return;
    if(index!=ui->pages->currentIndex()) spool->hurry();
    UiAnimations::changePage(ui->pages,index); ui->pageTitle->setText(titles[index]); ui->pageSubtitle->setText(subtitles[index]);
    ui->breadcrumb->setText(QStringLiteral("ATELIER / MATIÈRES"));
    ui->metricsRow->setVisible(index==0||index==2);
    const QList<QPushButton*> buttons=navButtons(); buttons[index]->setChecked(true);
    ui->newMaterialButton->setVisible(index!=1);
    if(index==4) QTimer::singleShot(0,this,[this]{updateStats();});
}
void MaterialWindow::refresh() {
    materials=repo.loadMaterials(); if(!repo.error().isEmpty()) {failure(repo.error()); return;}
    int low=0; double value=0; QSet<QString> suppliers;
    for(const auto &m:materials) {low+=m.low(); value+=m.quantity()*m.price(); suppliers.insert(m.supplier().toCaseFolded());}
    const auto whole=[](double v){return QString::number(std::lround(v));};
    UiAnimations::count(ui->referencesValue,materials.size(),whole); UiAnimations::count(ui->alertsValue,low,whole);
    UiAnimations::count(ui->valueValue,value,[](double v){return StockReports::money(v);}); UiAnimations::count(ui->suppliersValue,suppliers.size(),whole);
    updateInventory();
    QVector<Material> lowList,newLow; QSet<int> ids;
    for(const auto &m:materials) if(m.low()) {lowList.append(m); ids.insert(m.id()); if(!lowIds.contains(m.id())) newLow.append(m);}
    lowIds=ids;
    ui->navAlerts->setText(low?QStringLiteral("À commander (%1)").arg(low):QStringLiteral("À commander"));
    // Les plus urgentes d'abord : stock restant le plus faible par rapport au seuil.
    std::stable_sort(lowList.begin(),lowList.end(),[](const Material &a,const Material &b){return a.quantity()/a.minimum()<b.quantity()/b.minimum();});
    ui->alertsTable->setRowCount(0);
    for(const auto &m:lowList) {
        const int row=ui->alertsTable->rowCount(); ui->alertsTable->insertRow(row);
        const QStringList vals={m.name(),m.supplier(),StockReports::quantity(m.quantity()),StockReports::quantity(m.minimum()),m.unit(),StockReports::quantity(m.minimum()-m.quantity()),m.lot(),state(m)};
        for(int c=0;c<vals.size();++c) ui->alertsTable->setItem(row,c,cell(vals[c],m.id()));
        ui->alertsTable->item(row,0)->setForeground(QColor(m.quantity()==0?"#e8a9b0":"#e2cf9f"));
    }
    const bool haveAlerts=ui->alertsTable->rowCount()>0;
    ui->purchaseOrderButton->setEnabled(haveAlerts); ui->orderAllButton->setEnabled(haveAlerts); ui->receiveButton->setEnabled(haveAlerts); ui->reorderQuantity->setEnabled(haveAlerts);
    if(haveAlerts) ui->alertsTable->selectRow(0);
    ui->alertHeadline->setText(haveAlerts?QStringLiteral("%1 matière(s) à réapprovisionner").arg(low):QStringLiteral("Votre stock est au-dessus des seuils"));
    const int lotId=ui->lotCombo->currentData().toInt();
    ui->lotCombo->blockSignals(true); ui->lotCombo->clear();
    for(const auto &m:materials) ui->lotCombo->addItem(m.lot()+QStringLiteral(" — ")+m.name(),m.id());
    if(materials.isEmpty()) ui->lotCombo->addItem(QStringLiteral("Aucun lot enregistré"),0);
    int position=ui->lotCombo->findData(lotId); ui->lotCombo->setCurrentIndex(position>=0?position:0); ui->lotCombo->blockSignals(false); updateLot();
    updateStats();
    if(!newLow.isEmpty()) {if(opening) waitingAlerts+=newLow; else QTimer::singleShot(0,this,[this,newLow]{if(opening) waitingAlerts+=newLow; else showAlert(newLow);});}
    ui->statusbar->showMessage(QStringLiteral("%1 · %2 alerte(s) de stock").arg(QStringLiteral("Stock enregistré"),QString::number(low)),12000);
}
void MaterialWindow::resizeEvent(QResizeEvent *event) {QMainWindow::resizeEvent(event); if(ui&&ui->pages->currentIndex()==4) QTimer::singleShot(0,this,[this]{updateStats();});}
