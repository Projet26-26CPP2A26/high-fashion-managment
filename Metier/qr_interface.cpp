// Métier 2 : QR code par lot (génération, lecture, déduction du stock).
#include "materialwindow.h"
#include <QSqlDatabase>
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
#include "QrAnimation.h"
#include "sound.h"

void MaterialWindow::selectQrLot(int id) {const int index=ui->lotCombo->findData(id); if(index>=0) ui->lotCombo->setCurrentIndex(index); updateLot();}
void MaterialWindow::updateLot() {
    const Material m=repo.findMaterial(ui->lotCombo->currentData().toInt()); const bool enabled=m.id()!=0;
    for(auto *b:{ui->saveQrButton,ui->printLabelButton,ui->consumeButton}) b->setEnabled(enabled);
    ui->consumeQuantity->setEnabled(enabled); currentQr=QImage(); currentQrText.clear(); ui->openPhonePageButton->setEnabled(false);
    if(!enabled) {qrAnimation->clear(); ui->qrImage->setPixmap(QPixmap()); ui->qrImage->setText(QStringLiteral("Ajoutez une matière pour créer une étiquette QR.")); ui->lotDetailsLabel->setText(QStringLiteral("Aucun lot enregistré.")); ui->qrPayload->clear(); return;}
    currentQrText=QrService::pageUrl(m);
    const QString code=currentQrText;
    currentQr=QrService::generate(code);
    ui->qrImage->setPixmap(QPixmap()); ui->qrImage->clear();
    qrAnimation->setCode(QrService::generate(code,1));
    ui->qrPayload->setText(QStringLiteral("Fiche datée · Lot %1 · %2").arg(m.lot(),QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")));
    ui->qrPayload->setToolTip(code); ui->qrTitle->setText(QStringLiteral("Lot ")+m.lot());
    ui->openPhonePageButton->setEnabled(!currentQrText.isEmpty());
    ui->phoneHelpLabel->setText(QStringLiteral("Scannez avec l’appareil photo du téléphone : la fiche du lot s’affiche. Stock daté : recréez le QR après modification."));
    ui->lotDetailsLabel->setText(QStringLiteral("%1\n%2 · %3\nFournisseur : %4\nStock : %5 %6 · Seuil : %7 %6\nPrix unitaire : %8")
        .arg(m.name(),m.type(),m.color(),m.supplier(),StockReports::quantity(m.quantity()),m.unit(),StockReports::quantity(m.minimum()),StockReports::money(m.price())));
    ui->consumeQuantity->setDecimals((m.unit()==QStringLiteral("pièce")||m.unit()==QStringLiteral("bobine"))?0:2); ui->consumeQuantity->setSuffix(" "+m.unit());
    ui->consumeButton->setEnabled(m.quantity()>0); ui->consumeQuantity->setMaximum(m.quantity()); ui->consumeQuantity->setValue(std::min(1.0,m.quantity()));
}
void MaterialWindow::scanCode(const QString &code) {
    const QString lot=QrService::lotFromCode(code);
    if(lot.isEmpty()) {failure(QStringLiteral("Code invalide. Utilisez un QR AUREA ou l’identifiant d’un lot.")); return;}
    const Material m=repo.findLot(lot);
    if(!m.id()) {qrAnimation->refuse(); Sound::play("refus"); failure(QStringLiteral("Le lot %1 est introuvable dans cette base.").arg(lot)); return;}
    selectQrLot(m.id()); setPage(3); qrAnimation->succeed(m.lot()); Sound::play("valide"); notify(QStringLiteral("Lot identifié : %1 · %2").arg(m.lot(),m.supplier()));
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
    const Material m=repo.findMaterial(ui->lotCombo->currentData().toInt()); if(!m.id()) return;
    const double amount=ui->consumeQuantity->value();
    if(amount<=0) {failure(QStringLiteral("Indiquez une quantité positive.")); return;}
    if(!confirm(this,QStringLiteral("Confirmer la consommation"),QStringLiteral("Déduire %1 %2 du lot %3 ?\nStock après consommation : %4 %2.").arg(StockReports::quantity(amount),m.unit(),m.lot(),StockReports::quantity(m.quantity()-amount)))) return;
    if(!repo.changeQuantity(m.id(),-amount)) {failure(repo.error()); return;}
    refresh(); selectQrLot(m.id()); qrAnimation->succeed("-"+StockReports::quantity(amount)+" "+m.unit()); Sound::play("valide"); notify(QStringLiteral("Consommation enregistrée. Stock du lot mis à jour."));
}
void MaterialWindow::saveQr() {
    const Material m=repo.findMaterial(ui->lotCombo->currentData().toInt()); if(!m.id()||currentQr.isNull()) return;
    QString path=QFileDialog::getSaveFileName(this,QStringLiteral("Enregistrer le QR code"),QStringLiteral("AUREA-QR-")+m.lot()+QStringLiteral(".png"),QStringLiteral("Image PNG (*.png)")); if(path.isEmpty()) return;
    if(!path.endsWith(QStringLiteral(".png"),Qt::CaseInsensitive)) path+=QStringLiteral(".png");
    if(!currentQr.save(path,"PNG")) {failure(QStringLiteral("Impossible d’enregistrer cette image.")); return;} notify(QStringLiteral("Étiquette QR enregistrée : ")+path);
}
void MaterialWindow::printLabel() {
    const Material m=repo.findMaterial(ui->lotCombo->currentData().toInt()); if(!m.id()) return;
    const QString path=pdfPath(QStringLiteral("AUREA-etiquette-")+m.lot()+QStringLiteral(".pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::labelPdf(path,m,currentQr,&error)) {failure(error); return;} notify(QStringLiteral("Étiquette du lot exportée : ")+path);
}
