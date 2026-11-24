// Métier 1 : réapprovisionnement (alertes et bons de commande).
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
#include "sound.h"

void MaterialWindow::purchaseOrder() {
    const Material m=repo.findMaterial(selectedId(ui->alertsTable)); if(!m.id()) return;
    const double amount=ui->reorderQuantity->value();
    if(amount<=0) {failure(QStringLiteral("La quantité à commander doit être positive.")); return;}
    const QString path=pdfPath(QStringLiteral("AUREA-commande-")+m.lot()+QStringLiteral(".pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::writePdf(path,StockReports::purchaseHtml(m,amount),&error)) {failure(error); return;}
    notify(QStringLiteral("Bon de commande préparé. Enregistrez la réception à l’arrivée des matières."));
}
void MaterialWindow::orderAll() {
    const QString path=pdfPath(QStringLiteral("AUREA-commandes.pdf")); if(path.isEmpty()) return;
    QString error; if(!StockReports::writePdf(path,StockReports::allOrdersHtml(materials),&error,true)) {failure(error); return;}
    notify(QStringLiteral("Bons de commande préparés : ")+path);
}
void MaterialWindow::showAlert(const QVector<Material> &list) {
    if(isVisible()) Sound::play("alerte");
    QStringList lines;
    for(const auto &m:list.mid(0,8)) lines.append(QStringLiteral("• %1 : %2 / %3 %4 (%5)").arg(m.name(),StockReports::quantity(m.quantity()),StockReports::quantity(m.minimum()),m.unit(),state(m)));
    if(list.size()>8) lines.append(QStringLiteral("… et %1 autre(s)").arg(list.size()-8));
    auto *box=new QMessageBox(QMessageBox::Warning,QStringLiteral("Alerte de stock"),QStringLiteral("%1 matière(s) sous le seuil minimal :\n\n").arg(list.size())+lines.join('\n'),QMessageBox::NoButton,this);
    box->setAttribute(Qt::WA_DeleteOnClose); box->setTextFormat(Qt::PlainText);
    auto *order=box->addButton(QStringLiteral("Commander"),QMessageBox::AcceptRole); box->addButton(QStringLiteral("Plus tard"),QMessageBox::RejectRole);
    connect(order,&QPushButton::clicked,this,[this]{setPage(2);});
    box->open();
}
