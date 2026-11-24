#include "employeemodule.h"
#include "ui_employees.h"
#include "icons.h"
#include "uihelpers.h"

using namespace UiHelpers;

void EmployeeModule::refreshAttendance() {
    ui->attendanceTable->setRowCount(0); const QString search=ui->attendanceSearch->text().trimmed();
    for(const auto &a:pointages) {
        if(!search.isEmpty()&&!a.name().contains(search,Qt::CaseInsensitive)) continue;
        if(ui->attendanceDayFilter->currentIndex()==1&&a.day()!=QDate::currentDate()) continue;
        const bool live=a.day()==QDate::currentDate()&&!a.departure().isValid();
        const QString state=a.departure().isValid()?QStringLiteral("Terminé"):live?QStringLiteral("En cours"):QStringLiteral("À corriger");
        addRow(ui->attendanceTable,{a.day().toString("dd/MM/yyyy"),archived(a.name(),a.employeeId()),hm(a.arrival()),hm(a.departure()),QString::number(a.pause())+" min",a.departure().isValid()||live?duration(a.minutes()):QStringLiteral("—"),a.departure().isValid()||live?duration(a.overtime()):QStringLiteral("—"),state},a.id());
    }
    ui->deleteAttendanceButton->setEnabled(false);
}

void EmployeeModule::calculation() {
    const int elapsed=ui->attendanceArrival->time().secsTo(ui->attendanceDeparture->time())/60, pause=ui->attendancePause->value();
    ui->attendanceCalculation->setText(elapsed<=0?QStringLiteral("Le départ doit suivre l'arrivée."):pause>=elapsed?QStringLiteral("La pause doit être inférieure à la présence."):
        QStringLiteral("Travail net : %1 · heures supplémentaires : %2").arg(duration(elapsed-pause),duration(std::max(0,elapsed-pause-480))));
}

void EmployeeModule::saveAttendance() {
    const int id=ui->attendanceEmployee->currentData().toInt(); const QDate day=ui->attendanceDate->date();
    for(const auto &a:pointages) if(a.employeeId()==id&&a.day()==day) {if(!confirm(this,"Un pointage existe pour cette date. Remplacer ses horaires et sa pause ?")) return; break;}
    report(repo.saveAttendance(id,day,ui->attendanceArrival->time(),ui->attendanceDeparture->time(),ui->attendancePause->value()),"Pointage enregistré.");
}

void EmployeeModule::loadAttendance(int row) {
    if(row<0||!ui->attendanceTable->item(row,0)) return;
    const int id=ui->attendanceTable->item(row,0)->data(Qt::UserRole).toInt();
    for(const auto &a:pointages) if(a.id()==id) {
        if(!a.employeeId()) {ui->employeeStatus->setText("Historique d'un employé supprimé : consultation uniquement."); return;}
        ui->attendanceEmployee->setCurrentIndex(ui->attendanceEmployee->findData(a.employeeId())); ui->attendanceDate->setDate(a.day()); ui->attendanceArrival->setTime(a.arrival());
        ui->attendanceDeparture->setTime(a.departure().isValid()?a.departure():QTime::currentTime()); ui->attendancePause->setValue(a.pause()); calculation(); return;
    }
}

void EmployeeModule::deleteAttendance() {const int id=selectedId(ui->attendanceTable); if(id&&confirm(this,"Supprimer définitivement ce pointage ?")) report(repo.removeAttendance(id),"Pointage supprimé.");}
