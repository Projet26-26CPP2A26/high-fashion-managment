#include "employeemodule.h"
#include "ui_employees.h"
#include "icons.h"
#include "uihelpers.h"

using namespace UiHelpers;

void EmployeeModule::refreshCandidates()
{
    const int previous = selectedId(ui->candidatesTable);
    ui->candidatesTable->setRowCount(0);
    candidates = repo.rank(ui->taskSpecialty->currentText(), ui->taskHours->value());
    for (const auto &c : candidates) {
        addRow(ui->candidatesTable,
            {c.employee().name(), c.employee().specialty(), QString::number(c.employee().load()) + " %",
                QString::number(c.specialtyPoints()), QString::number(c.loadPoints()),
                QString::number(c.capacityPoints()), QString::number(c.score()),
                c.fits() ? QStringLiteral("Suffisante") : QStringLiteral("Insuffisante")},
            c.employee().id());
        if (!c.fits()) {
            ui->candidatesTable->item(ui->candidatesTable->rowCount() - 1, 7)->setForeground(QColor("#f0b9bb"));
        }
    }
    ui->assignButton->setEnabled(false);
    if (!candidates.isEmpty()) {
        int row = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            if (candidates[i].employee().id() == previous) {
                row = i;
            }
        }
        ui->candidatesTable->selectRow(row);
    }
}

void EmployeeModule::assignTask()
{
    const int id = selectedId(ui->candidatesTable);
    if (!id) {
        return;
    }
    if (confirm(this,
            QStringLiteral("Affecter « %1 » (%2 h) à %3 ?")
                .arg(ui->taskTitle->text())
                .arg(ui->taskHours->value())
                .arg(repo.findEmployee(id).name()))) {
        report(repo.assign(id, ui->taskTitle->text(), ui->taskSpecialty->currentText(), ui->taskHours->value()),
            "Tâche affectée ; charge actualisée.");
    }
}

void EmployeeModule::completeTask()
{
    const int id = selectedId(ui->assignmentsTable);
    if (id && confirm(this, "Marquer cette tâche comme terminée et libérer sa charge ?")) {
        report(repo.completeAssignment(id), "Tâche terminée ; capacité libérée.");
    }
}
