#include "employeemodule.h"
#include "fx.h"
#include "ui_employees.h"
#include "icons.h"
#include "uihelpers.h"

using namespace UiHelpers;

EmployeeModule::EmployeeModule(QWidget *parent):QWidget(parent),ui(new Ui::EmployeeModule) {
    ui->setupUi(this); repo.loadEmployees(); initialized=repo.error().isEmpty();

    for(QLayout *layout:QList<QLayout*>{ui->employeeRoot,ui->employeeListPageLayout,ui->assignmentPageLayout,ui->attendancePageLayout,ui->employeeStatsPageLayout,
        ui->employeeFilters,ui->employeeActions,ui->assignmentForm,ui->assignmentActions,ui->instantAttendance,ui->attendanceForm,ui->attendanceFilters}) layout->setContentsMargins(0,0,0,0);
    ui->employeeRoot->setSpacing(10);
    ui->kpiEmployeesIcon->setPixmap(Icons::pixmap(Icons::People,QColor("#e2cf9f"),18)); ui->kpiAvailableIcon->setPixmap(Icons::pixmap(Icons::Check,QColor("#e2cf9f"),18));
    ui->kpiPresentIcon->setPixmap(Icons::pixmap(Icons::Assign,QColor("#e2cf9f"),18)); ui->kpiHoursIcon->setPixmap(Icons::pixmap(Icons::Clock,QColor("#e2cf9f"),18));
    ui->alertButton->setIcon(Icons::pixmap(Icons::Alert,QColor("#2a2417"),20)); ui->alertButton->setIconSize(QSize(20,20));
    ui->hoursChart->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    ui->statStaffIcon->setPixmap(Icons::pixmap(Icons::People,QColor("#e2cf9f"),18)); ui->statLoadIcon->setPixmap(Icons::pixmap(Icons::Chart,QColor("#e2cf9f"),18));
    ui->statHoursIcon->setPixmap(Icons::pixmap(Icons::Clock,QColor("#e2cf9f"),18)); ui->statOvertimeIcon->setPixmap(Icons::pixmap(Icons::Alert,QColor("#e2cf9f"),18));
    ui->openStatsButton->setIcon(Icons::pixmap(Icons::Chart,QColor("#eeeae1"),18)); ui->openStatsButton->setIconSize(QSize(18,18)); ui->openStatsButton->setToolTip("Ouvrir l'onglet Statistiques");
    ui->employeeChart->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); ui->loadChart->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    if(!initialized) {ui->employeeStatus->setText("Erreur de base : "+repo.error()); setEnabled(false); return;}
    setupTable(ui->employeesTable,{80,145,180,130,160,95,115,80});
    setupTable(ui->attendanceTable,{95,180,75,75,75,115,115,100});
    setupTable(ui->todayTable,{190,90,90,85,120,120}); setupTable(ui->recentTable,{145,180,280,110});
    setupTable(ui->candidatesTable,{155,160,65,95,95,95,95,130}); setupTable(ui->assignmentsTable,{135,150,240,155,70,90});
    setupTable(ui->employeeStatsTable,{180,330,110});
    ui->attendanceDate->setDate(QDate::currentDate()); ui->attendanceDate->setMaximumDate(QDate::currentDate());
    ui->attendanceArrival->setTime(QTime(8,0)); ui->attendanceDeparture->setTime(QTime(17,0));
    connect(ui->alertButton,&QPushButton::clicked,this,[this]{ui->availabilityFilter->setCurrentIndex(0); ui->positionFilter->setCurrentIndex(0); ui->employeeSearch->clear(); ui->employeeSort->setCurrentIndex(7); setSection(1);});
    connect(ui->employeeSearch,&QLineEdit::textChanged,this,[this]{refreshEmployees();});
    for(auto *c:{ui->availabilityFilter,ui->positionFilter,ui->employeeSort}) connect(c,&QComboBox::currentIndexChanged,this,[this]{refreshEmployees();});
    connect(ui->addEmployeeButton,&QPushButton::clicked,this,[this]{editEmployee();});
    connect(ui->editEmployeeButton,&QPushButton::clicked,this,[this]{editEmployee(selectedId(ui->employeesTable));});
    connect(ui->employeesTable,&QTableWidget::cellDoubleClicked,this,[this]{editEmployee(selectedId(ui->employeesTable));});
    connect(ui->deleteEmployeeButton,&QPushButton::clicked,this,&EmployeeModule::deleteEmployee);
    connect(ui->employeesPdfButton,&QPushButton::clicked,this,[this]{exportPdf(false);});
    connect(ui->employeeStatsPdfButton,&QPushButton::clicked,this,[this]{exportPdf(true);});
    connect(ui->openStatsButton,&QPushButton::clicked,this,[this]{setSection(4);});
    connect(ui->statsBackButton,&QPushButton::clicked,this,[this]{setSection(1);});
    connect(ui->employeesTable,&QTableWidget::itemSelectionChanged,this,[this]{const bool selected=selectedId(ui->employeesTable)>0; ui->editEmployeeButton->setEnabled(selected); ui->deleteEmployeeButton->setEnabled(selected);});
    connect(ui->recommendButton,&QPushButton::clicked,this,[this]{if(ui->taskTitle->text().trimmed().isEmpty()) {DialogSkin::warn(this,"AUREA","Saisissez l'intitulé de la tâche."); return;} refreshCandidates();});
    connect(ui->taskTemplate,&QComboBox::currentIndexChanged,this,[this](int index){
        if(index<1) return;
        const QStringList specialties=EmployeeRepository::specialties(); const QList<int> hours={16,12,10,8,6,4};
        ui->taskTitle->setText(ui->taskTemplate->currentText()); ui->taskSpecialty->setCurrentText(specialties[index-1]); ui->taskHours->setValue(hours[index-1]);
    });
    connect(ui->taskTitle,&QLineEdit::textChanged,this,[this](const QString &title){
        const QStringList specialties=EmployeeRepository::specialties();
        const QList<QStringList> keywords={{"couture","coudre","assemblage","robe","veste","manteau"},{"broderie","broder","perle","paillette"},{"patron","gradation","coupe"},{"drapé","moulage","mannequin"},{"finition","ourlet","repassage","doublure"},{"qualité","contrôle","inspection"}};

        for(int i:QList<int>{1,2,3,4,5,0}) {bool match=false; for(const auto &word:keywords[i]) if(title.contains(word,Qt::CaseInsensitive)) match=true; if(match) {ui->taskSpecialty->setCurrentText(specialties[i]); break;}}
        refreshCandidates();
    });
    connect(ui->taskSpecialty,&QComboBox::currentIndexChanged,this,[this]{refreshCandidates();});
    connect(ui->taskHours,&QSpinBox::valueChanged,this,[this]{refreshCandidates();});
    connect(ui->candidatesTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const int id=selectedId(ui->candidatesTable); bool fits=false;
        for(const auto &c:candidates) if(c.employee().id()==id) fits=c.fits();
        ui->assignButton->setEnabled(fits&&!ui->taskTitle->text().trimmed().isEmpty());
    });
    connect(ui->assignButton,&QPushButton::clicked,this,&EmployeeModule::assignTask);
    connect(ui->completeTaskButton,&QPushButton::clicked,this,&EmployeeModule::completeTask);
    connect(ui->assignmentsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const int id=selectedId(ui->assignmentsTable); bool enabled=false; for(const auto &a:tasks) if(a.id()==id) enabled=!a.completed(); ui->completeTaskButton->setEnabled(enabled);
    });
    connect(ui->clockInButton,&QPushButton::clicked,this,[this]{report(repo.clockIn(ui->attendanceEmployee->currentData().toInt()),"Arrivée enregistrée (pause initiale : 0 min).");});
    connect(ui->clockOutButton,&QPushButton::clicked,this,[this]{report(repo.clockOut(ui->attendanceEmployee->currentData().toInt()),"Départ enregistré. Corrigez la pause si nécessaire.");});
    connect(ui->saveAttendanceButton,&QPushButton::clicked,this,&EmployeeModule::saveAttendance);
    connect(ui->deleteAttendanceButton,&QPushButton::clicked,this,&EmployeeModule::deleteAttendance);
    connect(ui->attendanceTable,&QTableWidget::cellDoubleClicked,this,[this](int row,int){loadAttendance(row);});
    connect(ui->attendanceTable,&QTableWidget::itemSelectionChanged,this,[this]{ui->deleteAttendanceButton->setEnabled(selectedId(ui->attendanceTable)>0);});
    connect(ui->attendanceSearch,&QLineEdit::textChanged,this,[this]{refreshAttendance();});
    connect(ui->attendanceDayFilter,&QComboBox::currentIndexChanged,this,[this]{refreshAttendance();});
    connect(ui->attendanceArrival,&QTimeEdit::timeChanged,this,[this]{calculation();});
    connect(ui->attendanceDeparture,&QTimeEdit::timeChanged,this,[this]{calculation();});
    connect(ui->attendancePause,&QSpinBox::valueChanged,this,[this]{calculation();});
    connect(ui->employeeTabs,&QTabWidget::currentChanged,this,[this](int index){if(index==4) QTimer::singleShot(0,this,[this]{updateStatistics();}); if(index==0) QTimer::singleShot(0,this,[this]{updateHoursChart();});});
    auto *timer=new QTimer(this); timer->setInterval(60000);
    connect(timer,&QTimer::timeout,this,[this]{ui->attendanceDate->setMaximumDate(QDate::currentDate()); refresh();}); timer->start();
    refresh(); calculation();
}
EmployeeModule::~EmployeeModule() {delete ui;}
void EmployeeModule::setSection(int index) {if(index>=0&&index<5) Fx::switchPage(ui->employeeTabs,index);}
int EmployeeModule::selectedId(QTableWidget *t) const {return !t->selectedItems().isEmpty()&&t->currentRow()>=0&&t->item(t->currentRow(),0)?t->item(t->currentRow(),0)->data(Qt::UserRole).toInt():0;}
void EmployeeModule::report(bool success,const QString &message) {
    if(!success) {DialogSkin::warn(this,"AUREA — Employés",repo.error()); return;}
    refresh(); ui->employeeStatus->setText(message);
}
void EmployeeModule::refresh() {
    if(!initialized) return;
    employees=repo.loadEmployees(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    pointages=repo.attendance(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    tasks=repo.assignments(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    const QString position=ui->positionFilter->currentText(),specialty=ui->taskSpecialty->currentText(); const int employeeId=ui->attendanceEmployee->currentData().toInt();
    QSet<QString> positions,specialties; for(const auto &s:EmployeeRepository::specialties()) specialties.insert(s);
    int available=0; for(const auto &e:employees) {positions.insert(e.position()); specialties.insert(e.specialty()); available+=e.available();}
    {
        QSignalBlocker a(ui->positionFilter),b(ui->taskSpecialty),c(ui->attendanceEmployee);
        QStringList positionsList=positions.values(),specialtiesList=specialties.values(); positionsList.sort(); specialtiesList.sort();
        ui->positionFilter->clear(); ui->positionFilter->addItem("Tous les postes"); ui->positionFilter->addItems(positionsList);
        const int p=ui->positionFilter->findText(position); ui->positionFilter->setCurrentIndex(std::max(0,p));
        ui->taskSpecialty->clear(); ui->taskSpecialty->addItems(specialtiesList); const int s=ui->taskSpecialty->findText(specialty); ui->taskSpecialty->setCurrentIndex(std::max(0,s));
        ui->attendanceEmployee->clear(); for(const auto &e:employees) ui->attendanceEmployee->addItem(e.name()+" · "+e.reference(),e.id());
        ui->attendanceEmployee->setCurrentIndex(std::max(0,ui->attendanceEmployee->findData(employeeId)));
    }
    refreshEmployees(); refreshAttendance(); refreshCandidates(); ui->assignmentsTable->setRowCount(0);
    ui->recentTable->setRowCount(0); ui->todayTable->setRowCount(0); int minutes=0,overtime=0,present=0;
    for(const auto &a:tasks) {
        const QString date=QDateTime::fromString(a.created(),Qt::ISODate).toString("dd/MM/yyyy HH:mm"), state=a.completed()?QStringLiteral("Terminée"):QStringLiteral("En cours");
        addRow(ui->assignmentsTable,{date,archived(a.name(),a.employeeId()),a.title(),a.specialty(),QString::number(a.hours())+" h",state},a.id());
        if(ui->recentTable->rowCount()<5) addRow(ui->recentTable,{date,archived(a.name(),a.employeeId()),a.title(),state},a.id());
    }
    for(const auto &a:pointages) if(a.day()==QDate::currentDate()&&a.employeeId()) {
        minutes+=a.minutes(); overtime+=a.overtime(); ++present;
        addRow(ui->todayTable,{a.name(),hm(a.arrival()),hm(a.departure()),QString::number(a.pause())+" min",duration(a.minutes()),duration(a.overtime())},a.id());
    }
    {
        const int count=employees.size(),high=int(std::count_if(employees.begin(),employees.end(),[](const Employee &e){return e.load()>=90;}));
        ui->kpiEmployeesValue->setText(QString::number(count)); ui->kpiEmployeesSub->setText(count==1?QStringLiteral("fiche"):QStringLiteral("fiches"));
        ui->kpiAvailableValue->setText(QString::number(available)); ui->kpiAvailableSub->setText(QStringLiteral("%1 %").arg(count?available*100/count:0));
        ui->kpiPresentValue->setText(QString::number(present)); ui->kpiPresentSub->setText(QStringLiteral("sur %1").arg(count));
        ui->kpiHoursValue->setText(duration(minutes)); ui->kpiHoursSub->setText(QStringLiteral("dont %1 sup.").arg(duration(overtime)));
        ui->alertButton->setText(high?QStringLiteral("Charge élevée : %1 employé%2   ›").arg(high).arg(high>1?QStringLiteral("s"):QString()):QStringLiteral("Aucune surcharge   ›"));
    }
    updateDashboard();
    ui->completeTaskButton->setEnabled(false); const bool haveEmployees=!employees.isEmpty();
    for(auto *b:{ui->clockInButton,ui->clockOutButton,ui->saveAttendanceButton}) b->setEnabled(haveEmployees);
    ui->employeeStatus->setText("Gestion des employés · base Oracle");
    updateStatistics();
}
void EmployeeModule::updateDashboard() {updateTeamGrid(); updateHoursChart();}
void EmployeeModule::updateTeamGrid() {
    while(QLayoutItem *old=ui->teamGrid->takeAt(0)) {if(old->widget()) old->widget()->deleteLater(); delete old;}
    QVector<Employee> team; for(const auto &e:employees) if(e.available()) team.append(e);
    std::stable_sort(team.begin(),team.end(),[](const Employee &a,const Employee &b){return a.load()<b.load();});
    if(team.isEmpty()) {auto *empty=new QLabel(QStringLiteral("Aucun employé disponible pour le moment.")); empty->setProperty("muted",true); ui->teamGrid->addWidget(empty,0,0,1,3); return;}
    for(int i=0;i<std::min<int>(6,team.size());++i) {
        const Employee &e=team[i];
        auto *tile=new QPushButton; tile->setProperty("tile",true); tile->setCursor(Qt::PointingHandCursor); tile->setMinimumHeight(126); tile->setToolTip(QStringLiteral("Modifier la fiche de %1").arg(e.name()));
        auto *box=new QVBoxLayout(tile); box->setContentsMargins(12,12,12,12); box->setSpacing(4);
        auto *avatar=new QLabel(initials(e.name())); avatar->setProperty("avatar",true); avatar->setFixedSize(38,38); avatar->setAlignment(Qt::AlignCenter);
        auto *name=new QLabel(e.name()); name->setStyleSheet("font-weight:600;"); auto *role=new QLabel(e.specialty()); role->setProperty("muted",true); role->setWordWrap(true);
        auto *row=new QHBoxLayout; row->setSpacing(6); auto *loadLabel=new QLabel("Charge"); loadLabel->setProperty("muted",true); auto *percent=new QLabel(QString::number(e.load())+" %"); percent->setProperty("muted",true);
        row->addWidget(loadLabel); row->addStretch(); row->addWidget(percent);
        auto *bar=new QProgressBar; bar->setRange(0,100); bar->setValue(e.load()); bar->setTextVisible(false); bar->setProperty("high",e.load()>=90);
        box->addWidget(avatar); box->addWidget(name); box->addWidget(role); box->addStretch(); box->addLayout(row); box->addWidget(bar);
        for(QLabel *l:{avatar,name,role,loadLabel,percent}) l->setAttribute(Qt::WA_TransparentForMouseEvents);
        bar->setAttribute(Qt::WA_TransparentForMouseEvents);
        const int id=e.id(); connect(tile,&QPushButton::clicked,this,[this,id]{editEmployee(id);});
        ui->teamGrid->addWidget(tile,i/3,i%3);
    }
    for(int col=0;col<3;++col) ui->teamGrid->setColumnStretch(col,1);
}
void EmployeeModule::resizeEvent(QResizeEvent *event) {QWidget::resizeEvent(event); if(!initialized) return; if(ui->employeeTabs->currentIndex()==4) QTimer::singleShot(0,this,[this]{updateStatistics();}); if(ui->employeeTabs->currentIndex()==0) QTimer::singleShot(0,this,[this]{updateHoursChart();});}
