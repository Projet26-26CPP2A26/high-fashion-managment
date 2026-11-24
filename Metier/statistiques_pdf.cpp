#include "employeemodule.h"
#include "ui_employees.h"
#include "icons.h"
#include "pdfexport.h"
#include "uihelpers.h"

using namespace UiHelpers;

QString EmployeeModule::employeesHtml() const {
    QString body=QStringLiteral("<p>%1 employé(s) affiché(s) · recherche : %2 · %3 · %4 · %5</p><table width='100%' cellspacing='0' cellpadding='5' border='1'>")
        .arg(filtered.size()).arg(ui->employeeSearch->text().toHtmlEscaped(),ui->availabilityFilter->currentText().toHtmlEscaped(),ui->positionFilter->currentText().toHtmlEscaped(),ui->employeeSort->currentText().toHtmlEscaped());
    body+=headers({"ID","Nom","E-mail","Poste","Spécialité","Embauche","Disponibilité","Charge"});
    for(const auto &e:filtered) body+=htmlRow({e.reference(),e.name(),e.email(),e.position(),e.specialty(),e.hired().toString("dd/MM/yyyy"),availability(e),QString::number(e.load())+" %"});
    return document("Liste des employés",body+"</table>");
}

QString EmployeeModule::statisticsHtml() const {
    QString body="<p>Effectif total : "+QString::number(employees.size())+"</p><table width='100%' cellpadding='6' border='1'>"+headers({"Catégorie","Groupe","Effectif"});
    for(int r=0;r<ui->employeeStatsTable->rowCount();++r) {QStringList values; for(int c=0;c<3;++c) values.append(ui->employeeStatsTable->item(r,c)->text()); body+=htmlRow(values);} return document("Statistiques des employés",body+"</table>");
}

void EmployeeModule::exportPdf(bool statistics) {
    QString path=QFileDialog::getSaveFileName(this,"Enregistrer le PDF",statistics?"AUREA_statistiques_employes.pdf":"AUREA_employes.pdf","Documents PDF (*.pdf)"); if(path.isEmpty()) return;
    if(!path.endsWith(".pdf",Qt::CaseInsensitive)) path+=".pdf";
    QString error;
    if(!Reports::writePdf(path,statistics?statisticsHtml():employeesHtml(),&error,true)) DialogSkin::warn(this,"AUREA",error); else ui->employeeStatus->setText("PDF enregistré : "+path);
}

void EmployeeModule::updateStatistics() {
    QMap<QString,int> specialties,positions; int available=0,loadSum=0;
    for(const auto &e:employees) {++specialties[e.specialty()]; ++positions[e.position()]; available+=e.available(); if(e.available()) loadSum+=e.load();}
    ui->employeeStatsTable->setRowCount(0);
    for(auto i=specialties.cbegin();i!=specialties.cend();++i) addRow(ui->employeeStatsTable,{"Spécialité",i.key(),QString::number(i.value())});
    addRow(ui->employeeStatsTable,{"Disponibilité","Disponible",QString::number(available)}); addRow(ui->employeeStatsTable,{"Disponibilité","Indisponible",QString::number(employees.size()-available)});
    for(auto i=positions.cbegin();i!=positions.cend();++i) addRow(ui->employeeStatsTable,{"Poste",i.key(),QString::number(i.value())});
    ui->employeeStatsSummary->setText(QStringLiteral("%1 employé(s) · %2 disponible(s) · %3 indisponible(s)").arg(employees.size()).arg(available).arg(employees.size()-available));

    const QDate today=QDate::currentDate(); int monthMinutes=0,monthOvertime=0;
    for(const auto &a:pointages) if(a.arrival().isValid()&&a.day().year()==today.year()&&a.day().month()==today.month()) {monthMinutes+=a.minutes(); monthOvertime+=a.overtime();}
    ui->statStaffValue->setText(QString::number(employees.size())); ui->statStaffSub->setText(QStringLiteral("%1 disponible(s)").arg(available));
    ui->statLoadValue->setText(QStringLiteral("%1 %").arg(available?loadSum/available:0)); ui->statLoadSub->setText(QStringLiteral("équipe disponible"));
    ui->statHoursValue->setText(QStringLiteral("%1 h").arg(monthMinutes/60)); ui->statHoursSub->setText(QLocale().monthName(today.month()));
    ui->statOvertimeValue->setText(QStringLiteral("%1 h").arg(monthOvertime/60)); ui->statOvertimeSub->setText(QStringLiteral("au-delà de 8 h nettes"));

    QVector<QPair<QString,int>> bySpecialty; for(auto i=specialties.cbegin();i!=specialties.cend();++i) bySpecialty.append(qMakePair(i.key(),i.value()));
    std::stable_sort(bySpecialty.begin(),bySpecialty.end(),[](const auto &a,const auto &b){return a.second>b.second;});
    QVector<BarRow> specialtyRows; int most=1; for(const auto &s:bySpecialty) {most=std::max(most,s.second); specialtyRows.append(BarRow{s.first,double(s.second),QString::number(s.second),false});}
    QVector<Employee> byLoad=employees; std::stable_sort(byLoad.begin(),byLoad.end(),[](const Employee &a,const Employee &b){return a.load()>b.load();});
    QVector<BarRow> loadRows; for(const auto &e:byLoad) loadRows.append(BarRow{e.name(),double(e.load()),QStringLiteral("%1 %").arg(e.load()),e.load()>=90});
    const qreal ratio=devicePixelRatioF();
    ui->employeeChart->setPixmap(barsImage(specialtyRows,most,std::max(320,ui->employeeChart->width()),150,ratio));
    ui->loadChart->setPixmap(barsImage(loadRows,100,std::max(320,ui->loadChart->width()),120,ratio));
}

void EmployeeModule::updateHoursChart() {
    const QDate today=QDate::currentDate(); QVector<double> hours(7,0.0); QStringList labels;
    for(int i=0;i<7;++i) labels<<QLocale().dayName(today.addDays(i-6).dayOfWeek(),QLocale::ShortFormat);
    for(const auto &a:pointages) {const int offset=int(a.day().daysTo(today)); if(offset>=0&&offset<7&&a.arrival().isValid()) hours[6-offset]+=a.minutes()/60.0;}
    const int width=std::max(420,ui->hoursChart->width()),height=210;
    ui->hoursChart->setPixmap(hoursChartImage(hours,labels,width,height,devicePixelRatioF()));
}
