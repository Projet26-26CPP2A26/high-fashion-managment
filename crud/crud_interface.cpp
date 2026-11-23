#include "employeemodule.h"
#include "ui_employees.h"
#include "icons.h"
#include "uihelpers.h"

using namespace UiHelpers;

void EmployeeModule::refreshEmployees() {
    filtered.clear(); const QString search=ui->employeeSearch->text().trimmed();
    for(const auto &e:employees) {
        if(!search.isEmpty()&&!QStringList{e.name(),e.email(),e.reference()}.join(" ").contains(search,Qt::CaseInsensitive)) continue;
        if(ui->availabilityFilter->currentIndex()==1&&!e.available()) continue;
        if(ui->availabilityFilter->currentIndex()==2&&e.available()) continue;
        if(ui->positionFilter->currentIndex()>0&&ui->positionFilter->currentText()!=e.position()) continue;
        filtered.append(e);
    }
    const int sort=ui->employeeSort->currentIndex();
    std::sort(filtered.begin(),filtered.end(),[sort](const Employee &a,const Employee &b) {
        if(sort==2) return a.id()<b.id();
        if(sort==3&&a.hired()!=b.hired()) return a.hired()>b.hired();
        if(sort==4&&a.hired()!=b.hired()) return a.hired()<b.hired();
        if(sort==5&&a.specialty()!=b.specialty()) return QString::localeAwareCompare(a.specialty(),b.specialty())<0;
        if(sort==6&&a.load()!=b.load()) return a.load()<b.load();
        if(sort==7&&a.load()!=b.load()) return a.load()>b.load();
        const int compare=QString::localeAwareCompare(a.name(),b.name()); return sort==1?compare>0:compare<0;
    });
    ui->employeesTable->setRowCount(0);
    for(const auto &e:filtered) addRow(ui->employeesTable,{e.reference(),e.name(),e.email(),e.position(),e.specialty(),e.hired().toString("dd/MM/yyyy"),availability(e),QString::number(e.load())+" %"},e.id());
    ui->employeeCount->setText(QStringLiteral("%1 sur %2 employé(s)").arg(filtered.size()).arg(employees.size()));
    ui->editEmployeeButton->setEnabled(false); ui->deleteEmployeeButton->setEnabled(false); ui->employeesPdfButton->setEnabled(!filtered.isEmpty());
}

void EmployeeModule::editEmployee(int id) {
    Employee e; if(id) e=repo.findEmployee(id); if(id&&!e.id()) return;
    QDialog dialog(this); dialog.setWindowTitle(id?"Modifier un employé":"Ajouter un employé"); dialog.setMinimumWidth(530);
    QFormLayout form(&dialog); form.setSpacing(12);
    QLineEdit name(e.name()),email(e.email()); name.setMaxLength(120); email.setMaxLength(180);
    QComboBox position,specialty,available; position.setEditable(true); specialty.setEditable(true);
    position.addItems(EmployeeRepository::positions()); specialty.addItems(EmployeeRepository::specialties()); available.addItems({"Disponible","Indisponible"});
    if(id) {position.setCurrentText(e.position()); specialty.setCurrentText(e.specialty()); available.setCurrentIndex(e.available()?0:1);}
    QDateEdit hired(id?e.hired():QDate::currentDate()); hired.setCalendarPopup(true); hired.setDisplayFormat("dd/MM/yyyy"); hired.setMinimumDate(QDate(1970,1,1)); hired.setMaximumDate(QDate::currentDate());
    QSpinBox load; load.setRange(0,100); load.setSuffix(" %"); load.setValue(e.baseLoad());
    QLabel identifier(id?e.reference():"Automatique à l'enregistrement"); form.addRow("Identifiant",&identifier);
    form.addRow("Nom",&name); form.addRow("E-mail",&email); form.addRow("Poste",&position); form.addRow("Spécialité",&specialty);
    form.addRow("Date d'embauche",&hired); form.addRow("Disponibilité",&available); form.addRow("Charge initiale (hors tâches)",&load);
    QLabel error; error.setWordWrap(true); error.setStyleSheet("color:#f0b9bb"); form.addRow(&error);
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel); buttons.button(QDialogButtonBox::Save)->setText("Enregistrer"); buttons.button(QDialogButtonBox::Cancel)->setText("Annuler"); form.addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        e.setName(name.text()); e.setEmail(email.text()); e.setPosition(position.currentText()); e.setSpecialty(specialty.currentText());
        e.setHired(hired.date()); e.setAvailable(available.currentIndex()==0); e.setBaseLoad(load.value());
        const bool saved=id?repo.updateEmployee(e):repo.addEmployee(e);
        if(saved) dialog.accept(); else error.setText(repo.error());
    });
    if(DialogSkin::run(dialog)==QDialog::Accepted) {refresh(); ui->employeeStatus->setText("Fiche employé enregistrée.");}
}

void EmployeeModule::deleteEmployee() {
    const auto e=repo.findEmployee(selectedId(ui->employeesTable)); if(!e.id()) return;
    if(confirm(this,"Supprimer "+e.name()+" ? Ses tâches de production seront supprimées avec lui.")) report(repo.deleteEmployee(e.id()),"Employé supprimé.");
}
