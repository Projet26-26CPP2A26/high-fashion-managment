#include "employeemodule.h"
#include "ui_employees.h"
#include "pdfexport.h"
#include "icons.h"
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QLinearGradient>
#include <QGridLayout>
#include <QLocale>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
QString duration(int minutes) {return QStringLiteral("%1 h %2").arg(minutes/60).arg(minutes%60,2,10,QChar('0'));}
QString hm(const QTime &time) {return time.isValid()?time.toString("HH:mm"):QStringLiteral("—");}
QString availability(const Employee &e) {return e.available?QStringLiteral("Disponible"):QStringLiteral("Indisponible");}
QString archived(const QString &name,qint64 id) {return id?name:name+QStringLiteral(" (supprimé)");}
void setupTable(QTableWidget *t,const QList<int> &widths) {
    t->verticalHeader()->hide(); t->verticalHeader()->setDefaultSectionSize(42); t->setFrameShape(QFrame::NoFrame); t->setFocusPolicy(Qt::NoFocus); t->setWordWrap(false);
    t->horizontalHeader()->setStretchLastSection(true); t->setSortingEnabled(false);
    t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    for(int i=0;i<widths.size();++i) t->setColumnWidth(i,widths[i]);
}
// Couleur des états (vert = bon, doré = en cours, rouge = à surveiller), comme la colonne « Statut » de la maquette.
QColor statusColor(const QString &text) {
    if(text==QLatin1String("Disponible")||text==QLatin1String("Terminée")||text==QLatin1String("Terminé")||text==QLatin1String("Suffisante")) return QColor("#7fc08a");
    if(text==QLatin1String("Indisponible")||text==QLatin1String("Insuffisante")||text==QLatin1String("À corriger")) return QColor("#e08f8f");
    if(text==QLatin1String("En cours")) return QColor("#e6d3a3");
    return QColor();
}
void addRow(QTableWidget *t,const QStringList &values,qint64 id=0) {
    const int row=t->rowCount(); t->insertRow(row);
    for(int col=0;col<values.size();++col) {
        auto *item=new QTableWidgetItem(values[col]); item->setData(Qt::UserRole,id);
        const QColor colour=statusColor(values[col]); if(colour.isValid()) item->setForeground(colour);
        t->setItem(row,col,item);
    }
}
QString initials(const QString &name) {
    QString out; for(const QString &part:name.split(QLatin1Char(' '),Qt::SkipEmptyParts)) {if(out.size()<2) out+=part.at(0).toUpper();}
    return out.isEmpty()?QStringLiteral("?"):out;
}
struct BarRow {QString label; double value; QString text; bool high;};
// Barres horizontales sur fond de piste : effectif par spécialité et charge par employé.
QPixmap barsImage(const QVector<BarRow> &rows,double maxValue,int width,int labelWidth,qreal ratio) {
    const int rowHeight=30,height=std::max(70,int(rows.size())*rowHeight+6);
    QPixmap pixmap(int(width*ratio),int(height*ratio)); pixmap.setDevicePixelRatio(ratio); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing); p.setFont(QFont(QStringLiteral("Inter"),9));
    const int barX=labelWidth+10,textWidth=46,barWidth=std::max(40,width-barX-textWidth-8); const QFontMetrics metrics(p.font()); int y=0;
    for(const auto &r:rows) {
        p.setPen(QColor("#d9d5cb")); p.drawText(QRect(0,y,labelWidth,rowHeight),Qt::AlignLeft|Qt::AlignVCenter,metrics.elidedText(r.label,Qt::ElideRight,labelWidth));
        const QRectF track(barX,y+rowHeight/2.0-4,barWidth,8); p.setPen(Qt::NoPen); p.setBrush(QColor("#34343a")); p.drawRoundedRect(track,4,4);
        const double length=barWidth*std::clamp(maxValue>0?r.value/maxValue:0.0,0.0,1.0);
        if(length>0) {
            QLinearGradient gradient(barX,0,barX+barWidth,0);
            if(r.high) {gradient.setColorAt(0,QColor("#c27575")); gradient.setColorAt(1,QColor("#e8a3a3"));} else {gradient.setColorAt(0,QColor("#bfa370")); gradient.setColorAt(1,QColor("#ecdcae"));}
            p.setBrush(gradient); p.drawRoundedRect(QRectF(barX,track.top(),std::max(8.0,length),8),4,4);
        }
        p.setPen(QColor(r.high?"#e8a3a3":"#e2cf9f")); p.drawText(QRect(barX+barWidth+8,y,textWidth,rowHeight),Qt::AlignLeft|Qt::AlignVCenter,r.text);
        y+=rowHeight;
    }
    if(rows.isEmpty()) {p.setPen(QColor("#8f8c86")); p.drawText(QRect(0,0,width,height),Qt::AlignCenter,QStringLiteral("Aucun employé enregistré"));}
    return pixmap;
}
// Courbe lissée (Catmull-Rom) avec aire dorée, comme « Sales Overview ».
QPixmap hoursChartImage(const QVector<double> &values,const QStringList &labels,int width,int height,qreal ratio) {
    QPixmap pixmap(int(width*ratio),int(height*ratio)); pixmap.setDevicePixelRatio(ratio); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing); p.setFont(QFont(QStringLiteral("Inter"),8));
    double top=1; for(double v:values) top=std::max(top,v); top=std::ceil(top/2.0)*2.0; if(top<4) top=4;
    const QRectF plot(46,10,width-62,height-44); const int n=values.size();
    for(int i=0;i<=4;++i) {
        const double y=plot.bottom()-plot.height()*i/4.0;
        p.setPen(QPen(QColor("#323236"),1)); p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));
        p.setPen(QColor("#8f8c86")); p.drawText(QRectF(0,y-9,plot.left()-8,18),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("%1 h").arg(top*i/4.0,0,'f',top*i/4.0==int(top*i/4.0)?0:1));
    }
    QVector<QPointF> pts; for(int i=0;i<n;++i) pts.append(QPointF(plot.left()+plot.width()*i/std::max(1,n-1),plot.bottom()-plot.height()*values[i]/top));
    for(int i=0;i<n;++i) {p.setPen(QColor("#8f8c86")); p.drawText(QRectF(pts[i].x()-30,plot.bottom()+8,60,18),Qt::AlignCenter,labels[i]);}
    if(n<2) return pixmap;
    QPainterPath line; line.moveTo(pts[0]);
    for(int i=0;i<n-1;++i) {
        const QPointF p0=pts[std::max(0,i-1)],p1=pts[i],p2=pts[i+1],p3=pts[std::min(n-1,i+2)];
        QPointF c1=p1+(p2-p0)/6.0,c2=p2-(p3-p1)/6.0;
        c1.setY(std::clamp(c1.y(),plot.top(),plot.bottom())); c2.setY(std::clamp(c2.y(),plot.top(),plot.bottom()));
        line.cubicTo(c1,c2,p2);
    }
    QPainterPath area=line; area.lineTo(pts.last().x(),plot.bottom()); area.lineTo(pts.first().x(),plot.bottom()); area.closeSubpath();
    QLinearGradient fill(0,plot.top(),0,plot.bottom()); fill.setColorAt(0,QColor(230,211,163,95)); fill.setColorAt(1,QColor(230,211,163,0));
    p.setPen(Qt::NoPen); p.setBrush(fill); p.drawPath(area);
    QPen pen(QColor("#ecdcae"),2.2); pen.setCapStyle(Qt::RoundCap); p.setPen(pen); p.setBrush(Qt::NoBrush); p.drawPath(line);
    p.setPen(QPen(QColor("#232326"),2)); p.setBrush(QColor("#ecdcae")); p.drawEllipse(pts.last(),4.5,4.5);
    return pixmap;
}
bool confirm(QWidget *parent,const QString &text) {
    QMessageBox box(QMessageBox::Question,"AUREA",text,QMessageBox::NoButton,parent);
    auto *yes=box.addButton("Confirmer",QMessageBox::AcceptRole); auto *no=box.addButton("Annuler",QMessageBox::RejectRole); box.setDefaultButton(no); box.exec(); return box.clickedButton()==yes;
}
QString document(const QString &title,const QString &body) {
    return QStringLiteral("<html><body style='font-family:Inter,Segoe UI;font-size:9pt;color:#222'><h1>AUREA — %1</h1><p>Atelier de couture · %2</p>%3</body></html>")
        .arg(title.toHtmlEscaped(),QDate::currentDate().toString("dd/MM/yyyy"),body);
}
QString headers(const QStringList &values) {QString out="<tr bgcolor='#e5d5a1'>"; for(const auto &v:values) out+="<th align='left'>"+v.toHtmlEscaped()+"</th>"; return out+"</tr>";}
QString htmlRow(const QStringList &values) {QString out="<tr>"; for(const auto &v:values) out+="<td>"+v.toHtmlEscaped()+"</td>"; return out+"</tr>";}
}
EmployeeModule::EmployeeModule(QSqlDatabase database,bool demo,QWidget *parent):QWidget(parent),ui(new Ui::EmployeeModule),repo(database) {
    ui->setupUi(this); initialized=repo.initialize(demo);
    // Marges : la fenêtre principale fournit déjà l'espace autour du contenu.
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
    connect(ui->recommendButton,&QPushButton::clicked,this,[this]{if(ui->taskTitle->text().trimmed().isEmpty()) {QMessageBox::warning(this,"AUREA","Saisissez l'intitulé de la tâche."); return;} refreshCandidates();});
    connect(ui->taskTemplate,&QComboBox::currentIndexChanged,this,[this](int index){
        if(index<1) return;
        const QStringList specialties=EmployeeRepository::specialties(); const QList<int> hours={16,12,10,8,6,4};
        ui->taskTitle->setText(ui->taskTemplate->currentText()); ui->taskSpecialty->setCurrentText(specialties[index-1]); ui->taskHours->setValue(hours[index-1]);
    });
    connect(ui->taskTitle,&QLineEdit::textChanged,this,[this](const QString &title){
        const QStringList specialties=EmployeeRepository::specialties();
        const QList<QStringList> keywords={{"couture","coudre","assemblage","robe","veste","manteau"},{"broderie","broder","perle","paillette"},{"patron","gradation","coupe"},{"drapé","moulage","mannequin"},{"finition","ourlet","repassage","doublure"},{"qualité","contrôle","inspection"}};
        // Specific techniques take precedence over a generic garment name.
        for(int i:QList<int>{1,2,3,4,5,0}) {bool match=false; for(const auto &word:keywords[i]) if(title.contains(word,Qt::CaseInsensitive)) match=true; if(match) {ui->taskSpecialty->setCurrentText(specialties[i]); break;}}
        refreshCandidates();
    });
    connect(ui->taskSpecialty,&QComboBox::currentIndexChanged,this,[this]{refreshCandidates();});
    connect(ui->taskHours,&QSpinBox::valueChanged,this,[this]{refreshCandidates();});
    connect(ui->candidatesTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const qint64 id=selectedId(ui->candidatesTable); bool fits=false;
        for(const auto &c:candidates) if(c.employee.id==id) fits=c.fits;
        ui->assignButton->setEnabled(fits&&!ui->taskTitle->text().trimmed().isEmpty());
    });
    connect(ui->assignButton,&QPushButton::clicked,this,&EmployeeModule::assignTask);
    connect(ui->completeTaskButton,&QPushButton::clicked,this,&EmployeeModule::completeTask);
    connect(ui->assignmentsTable,&QTableWidget::itemSelectionChanged,this,[this]{
        const qint64 id=selectedId(ui->assignmentsTable); bool enabled=false; for(const auto &a:tasks) if(a.id==id) enabled=!a.completed; ui->completeTaskButton->setEnabled(enabled);
    });
    connect(ui->clockInButton,&QPushButton::clicked,this,[this]{report(repo.clockIn(ui->attendanceEmployee->currentData().toLongLong()),"Arrivée enregistrée (pause initiale : 0 min).");});
    connect(ui->clockOutButton,&QPushButton::clicked,this,[this]{report(repo.clockOut(ui->attendanceEmployee->currentData().toLongLong()),"Départ enregistré. Corrigez la pause si nécessaire.");});
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
void EmployeeModule::setSection(int index) {if(index>=0&&index<5) ui->employeeTabs->setCurrentIndex(index);}
qint64 EmployeeModule::selectedId(QTableWidget *t) const {return !t->selectedItems().isEmpty()&&t->currentRow()>=0&&t->item(t->currentRow(),0)?t->item(t->currentRow(),0)->data(Qt::UserRole).toLongLong():0;}
void EmployeeModule::report(bool success,const QString &message) {
    if(!success) {QMessageBox::warning(this,"AUREA — Employés",repo.error()); return;}
    refresh(); ui->employeeStatus->setText(message);
}
void EmployeeModule::refresh() {
    if(!initialized) return;
    employees=repo.all(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    pointages=repo.attendance(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    tasks=repo.assignments(); if(!repo.error().isEmpty()) {ui->employeeStatus->setText(repo.error()); return;}
    const QString position=ui->positionFilter->currentText(),specialty=ui->taskSpecialty->currentText(); const qint64 employeeId=ui->attendanceEmployee->currentData().toLongLong();
    QSet<QString> positions,specialties; for(const auto &s:EmployeeRepository::specialties()) specialties.insert(s);
    int available=0; for(const auto &e:employees) {positions.insert(e.position); specialties.insert(e.specialty); available+=e.available;}
    {
        QSignalBlocker a(ui->positionFilter),b(ui->taskSpecialty),c(ui->attendanceEmployee);
        QStringList positionsList=positions.values(),specialtiesList=specialties.values(); positionsList.sort(); specialtiesList.sort();
        ui->positionFilter->clear(); ui->positionFilter->addItem("Tous les postes"); ui->positionFilter->addItems(positionsList);
        const int p=ui->positionFilter->findText(position); ui->positionFilter->setCurrentIndex(std::max(0,p));
        ui->taskSpecialty->clear(); ui->taskSpecialty->addItems(specialtiesList); const int s=ui->taskSpecialty->findText(specialty); ui->taskSpecialty->setCurrentIndex(std::max(0,s));
        ui->attendanceEmployee->clear(); for(const auto &e:employees) ui->attendanceEmployee->addItem(e.name+" · "+e.reference(),e.id);
        ui->attendanceEmployee->setCurrentIndex(std::max(0,ui->attendanceEmployee->findData(employeeId)));
    }
    refreshEmployees(); refreshAttendance(); refreshCandidates(); ui->assignmentsTable->setRowCount(0);
    ui->recentTable->setRowCount(0); ui->todayTable->setRowCount(0); int minutes=0,overtime=0,present=0;
    for(const auto &a:tasks) {
        const QString date=QDateTime::fromString(a.created,Qt::ISODate).toString("dd/MM/yyyy HH:mm"), state=a.completed?QStringLiteral("Terminée"):QStringLiteral("En cours");
        addRow(ui->assignmentsTable,{date,archived(a.name,a.employeeId),a.title,a.specialty,QString::number(a.hours)+" h",state},a.id);
        if(ui->recentTable->rowCount()<5) addRow(ui->recentTable,{date,archived(a.name,a.employeeId),a.title,state},a.id);
    }
    for(const auto &a:pointages) if(a.day==QDate::currentDate()&&a.employeeId) {
        minutes+=a.minutes(); overtime+=a.overtime(); ++present;
        addRow(ui->todayTable,{a.name,hm(a.arrival),hm(a.departure),QString::number(a.pause)+" min",duration(a.minutes()),duration(a.overtime())},a.id);
    }
    {
        const int count=employees.size(),high=int(std::count_if(employees.begin(),employees.end(),[](const Employee &e){return e.load>=90;}));
        ui->kpiEmployeesValue->setText(QString::number(count)); ui->kpiEmployeesSub->setText(count==1?QStringLiteral("fiche"):QStringLiteral("fiches"));
        ui->kpiAvailableValue->setText(QString::number(available)); ui->kpiAvailableSub->setText(QStringLiteral("%1 %").arg(count?available*100/count:0));
        ui->kpiPresentValue->setText(QString::number(present)); ui->kpiPresentSub->setText(QStringLiteral("sur %1").arg(count));
        ui->kpiHoursValue->setText(duration(minutes)); ui->kpiHoursSub->setText(QStringLiteral("dont %1 sup.").arg(duration(overtime)));
        ui->alertButton->setText(high?QStringLiteral("Charge élevée : %1 employé%2   ›").arg(high).arg(high>1?QStringLiteral("s"):QString()):QStringLiteral("Aucune surcharge   ›"));
    }
    updateDashboard();
    ui->completeTaskButton->setEnabled(false); const bool haveEmployees=!employees.isEmpty();
    for(auto *b:{ui->clockInButton,ui->clockOutButton,ui->saveAttendanceButton}) b->setEnabled(haveEmployees);
    ui->employeeStatus->setText("Gestion des employés · sauvegarde SQLite immédiate");
    updateStatistics();
}
void EmployeeModule::refreshEmployees() {
    filtered.clear(); const QString search=ui->employeeSearch->text().trimmed();
    for(const auto &e:employees) {
        if(!search.isEmpty()&&!QStringList{e.name,e.email,e.reference()}.join(" ").contains(search,Qt::CaseInsensitive)) continue;
        if(ui->availabilityFilter->currentIndex()==1&&!e.available) continue;
        if(ui->availabilityFilter->currentIndex()==2&&e.available) continue;
        if(ui->positionFilter->currentIndex()>0&&ui->positionFilter->currentText()!=e.position) continue;
        filtered.append(e);
    }
    const int sort=ui->employeeSort->currentIndex();
    std::sort(filtered.begin(),filtered.end(),[sort](const Employee &a,const Employee &b) {
        if(sort==2) return a.id<b.id;
        if(sort==3&&a.hired!=b.hired) return a.hired>b.hired;
        if(sort==4&&a.hired!=b.hired) return a.hired<b.hired;
        if(sort==5&&a.specialty!=b.specialty) return QString::localeAwareCompare(a.specialty,b.specialty)<0;
        if(sort==6&&a.load!=b.load) return a.load<b.load;
        if(sort==7&&a.load!=b.load) return a.load>b.load;
        const int compare=QString::localeAwareCompare(a.name,b.name); return sort==1?compare>0:compare<0;
    });
    ui->employeesTable->setRowCount(0);
    for(const auto &e:filtered) addRow(ui->employeesTable,{e.reference(),e.name,e.email,e.position,e.specialty,e.hired.toString("dd/MM/yyyy"),availability(e),QString::number(e.load)+" %"},e.id);
    ui->employeeCount->setText(QStringLiteral("%1 sur %2 employé(s)").arg(filtered.size()).arg(employees.size()));
    ui->editEmployeeButton->setEnabled(false); ui->deleteEmployeeButton->setEnabled(false); ui->employeesPdfButton->setEnabled(!filtered.isEmpty());
}
void EmployeeModule::editEmployee(qint64 id) {
    Employee e=id?repo.find(id):Employee{}; if(id&&!e.id) return;
    QDialog dialog(this); dialog.setWindowTitle(id?"Modifier un employé":"Ajouter un employé"); dialog.setMinimumWidth(530);
    QFormLayout form(&dialog); form.setSpacing(12);
    QLineEdit name(e.name),email(e.email); name.setMaxLength(120); email.setMaxLength(180);
    QComboBox position,specialty,available; position.setEditable(true); specialty.setEditable(true);
    position.addItems(EmployeeRepository::positions()); specialty.addItems(EmployeeRepository::specialties()); available.addItems({"Disponible","Indisponible"});
    if(id) {position.setCurrentText(e.position); specialty.setCurrentText(e.specialty); available.setCurrentIndex(e.available?0:1);}
    QDateEdit hired(id?e.hired:QDate::currentDate()); hired.setCalendarPopup(true); hired.setDisplayFormat("dd/MM/yyyy"); hired.setMinimumDate(QDate(1970,1,1)); hired.setMaximumDate(QDate::currentDate());
    QSpinBox load; load.setRange(0,100); load.setSuffix(" %"); load.setValue(e.baseLoad);
    QLabel identifier(id?e.reference():"Automatique à l'enregistrement"); form.addRow("Identifiant",&identifier);
    form.addRow("Nom",&name); form.addRow("E-mail",&email); form.addRow("Poste",&position); form.addRow("Spécialité",&specialty);
    form.addRow("Date d'embauche",&hired); form.addRow("Disponibilité",&available); form.addRow("Charge initiale (hors tâches)",&load);
    QLabel error; error.setWordWrap(true); error.setStyleSheet("color:#f0b9bb"); form.addRow(&error);
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel); buttons.button(QDialogButtonBox::Save)->setText("Enregistrer"); buttons.button(QDialogButtonBox::Cancel)->setText("Annuler"); form.addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{e.name=name.text(); e.email=email.text(); e.position=position.currentText(); e.specialty=specialty.currentText(); e.hired=hired.date(); e.available=available.currentIndex()==0; e.baseLoad=load.value(); if(repo.save(e)) dialog.accept(); else error.setText(repo.error());});
    if(dialog.exec()==QDialog::Accepted) {refresh(); ui->employeeStatus->setText("Fiche employé enregistrée.");}
}
void EmployeeModule::deleteEmployee() {
    const auto e=repo.find(selectedId(ui->employeesTable)); if(!e.id) return;
    if(confirm(this,"Supprimer "+e.name+" ? Ses pointages et affectations restent dans l'historique.")) report(repo.remove(e.id),"Employé supprimé ; historique conservé.");
}
void EmployeeModule::refreshCandidates() {
    const qint64 previous=selectedId(ui->candidatesTable); ui->candidatesTable->setRowCount(0); candidates=repo.rank(ui->taskSpecialty->currentText(),ui->taskHours->value());
    for(const auto &c:candidates) {
        addRow(ui->candidatesTable,{c.employee.name,c.employee.specialty,QString::number(c.employee.load)+" %",QString::number(c.specialtyPoints),QString::number(c.loadPoints),QString::number(c.capacityPoints),QString::number(c.score()),c.fits?QStringLiteral("Suffisante"):QStringLiteral("Insuffisante")},c.employee.id);
        if(!c.fits) ui->candidatesTable->item(ui->candidatesTable->rowCount()-1,7)->setForeground(QColor("#f0b9bb"));
    }
    ui->assignButton->setEnabled(false);
    if(!candidates.isEmpty()) {int row=0; for(int i=0;i<candidates.size();++i) if(candidates[i].employee.id==previous) row=i; ui->candidatesTable->selectRow(row);}
}
void EmployeeModule::assignTask() {
    const qint64 id=selectedId(ui->candidatesTable); if(!id) return;
    if(confirm(this,QStringLiteral("Affecter « %1 » (%2 h) à %3 ?").arg(ui->taskTitle->text()).arg(ui->taskHours->value()).arg(repo.find(id).name)))
        report(repo.assign(id,ui->taskTitle->text(),ui->taskSpecialty->currentText(),ui->taskHours->value()),"Tâche affectée ; charge actualisée.");
}
void EmployeeModule::completeTask() {
    const qint64 id=selectedId(ui->assignmentsTable); if(id&&confirm(this,"Marquer cette tâche comme terminée et libérer sa charge ?")) report(repo.completeAssignment(id),"Tâche terminée ; capacité libérée.");
}
void EmployeeModule::refreshAttendance() {
    ui->attendanceTable->setRowCount(0); const QString search=ui->attendanceSearch->text().trimmed();
    for(const auto &a:pointages) {
        if(!search.isEmpty()&&!a.name.contains(search,Qt::CaseInsensitive)) continue;
        if(ui->attendanceDayFilter->currentIndex()==1&&a.day!=QDate::currentDate()) continue;
        const bool live=a.day==QDate::currentDate()&&!a.departure.isValid();
        const QString state=a.departure.isValid()?QStringLiteral("Terminé"):live?QStringLiteral("En cours"):QStringLiteral("À corriger");
        addRow(ui->attendanceTable,{a.day.toString("dd/MM/yyyy"),archived(a.name,a.employeeId),hm(a.arrival),hm(a.departure),QString::number(a.pause)+" min",a.departure.isValid()||live?duration(a.minutes()):QStringLiteral("—"),a.departure.isValid()||live?duration(a.overtime()):QStringLiteral("—"),state},a.id);
    }
    ui->deleteAttendanceButton->setEnabled(false);
}
void EmployeeModule::calculation() {
    const int elapsed=ui->attendanceArrival->time().secsTo(ui->attendanceDeparture->time())/60, pause=ui->attendancePause->value();
    ui->attendanceCalculation->setText(elapsed<=0?QStringLiteral("Le départ doit suivre l'arrivée."):pause>=elapsed?QStringLiteral("La pause doit être inférieure à la présence."):
        QStringLiteral("Travail net : %1 · heures supplémentaires : %2").arg(duration(elapsed-pause),duration(std::max(0,elapsed-pause-480))));
}
void EmployeeModule::saveAttendance() {
    const qint64 id=ui->attendanceEmployee->currentData().toLongLong(); const QDate day=ui->attendanceDate->date();
    for(const auto &a:pointages) if(a.employeeId==id&&a.day==day) {if(!confirm(this,"Un pointage existe pour cette date. Remplacer ses horaires et sa pause ?")) return; break;}
    report(repo.saveAttendance(id,day,ui->attendanceArrival->time(),ui->attendanceDeparture->time(),ui->attendancePause->value()),"Pointage enregistré.");
}
void EmployeeModule::loadAttendance(int row) {
    if(row<0||!ui->attendanceTable->item(row,0)) return;
    const qint64 id=ui->attendanceTable->item(row,0)->data(Qt::UserRole).toLongLong();
    for(const auto &a:pointages) if(a.id==id) {
        if(!a.employeeId) {ui->employeeStatus->setText("Historique d'un employé supprimé : consultation uniquement."); return;}
        ui->attendanceEmployee->setCurrentIndex(ui->attendanceEmployee->findData(a.employeeId)); ui->attendanceDate->setDate(a.day); ui->attendanceArrival->setTime(a.arrival);
        ui->attendanceDeparture->setTime(a.departure.isValid()?a.departure:QTime::currentTime()); ui->attendancePause->setValue(a.pause); calculation(); return;
    }
}
void EmployeeModule::deleteAttendance() {const qint64 id=selectedId(ui->attendanceTable); if(id&&confirm(this,"Supprimer définitivement ce pointage ?")) report(repo.removeAttendance(id),"Pointage supprimé.");}
QString EmployeeModule::employeesHtml() const {
    QString body=QStringLiteral("<p>%1 employé(s) affiché(s) · recherche : %2 · %3 · %4 · %5</p><table width='100%' cellspacing='0' cellpadding='5' border='1'>")
        .arg(filtered.size()).arg(ui->employeeSearch->text().toHtmlEscaped(),ui->availabilityFilter->currentText().toHtmlEscaped(),ui->positionFilter->currentText().toHtmlEscaped(),ui->employeeSort->currentText().toHtmlEscaped());
    body+=headers({"ID","Nom","E-mail","Poste","Spécialité","Embauche","Disponibilité","Charge"});
    for(const auto &e:filtered) body+=htmlRow({e.reference(),e.name,e.email,e.position,e.specialty,e.hired.toString("dd/MM/yyyy"),availability(e),QString::number(e.load)+" %"});
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
    if(!Reports::writePdf(path,statistics?statisticsHtml():employeesHtml(),&error,true)) QMessageBox::warning(this,"AUREA",error); else ui->employeeStatus->setText("PDF enregistré : "+path);
}
void EmployeeModule::updateDashboard() {updateTeamGrid(); updateHoursChart();}
void EmployeeModule::updateHoursChart() {
    const QDate today=QDate::currentDate(); QVector<double> hours(7,0.0); QStringList labels;
    for(int i=0;i<7;++i) labels<<QLocale().dayName(today.addDays(i-6).dayOfWeek(),QLocale::ShortFormat);
    for(const auto &a:pointages) {const int offset=int(a.day.daysTo(today)); if(offset>=0&&offset<7&&a.arrival.isValid()) hours[6-offset]+=a.minutes()/60.0;}
    const int width=std::max(420,ui->hoursChart->width()),height=210;
    ui->hoursChart->setPixmap(hoursChartImage(hours,labels,width,height,devicePixelRatioF()));
}
void EmployeeModule::updateTeamGrid() {
    while(QLayoutItem *old=ui->teamGrid->takeAt(0)) {if(old->widget()) old->widget()->deleteLater(); delete old;}
    QVector<Employee> team; for(const auto &e:employees) if(e.available) team.append(e);
    std::stable_sort(team.begin(),team.end(),[](const Employee &a,const Employee &b){return a.load<b.load;});
    if(team.isEmpty()) {auto *empty=new QLabel(QStringLiteral("Aucun employé disponible pour le moment.")); empty->setProperty("muted",true); ui->teamGrid->addWidget(empty,0,0,1,3); return;}
    for(int i=0;i<std::min<int>(6,team.size());++i) {
        const Employee &e=team[i];
        auto *tile=new QPushButton; tile->setProperty("tile",true); tile->setCursor(Qt::PointingHandCursor); tile->setMinimumHeight(126); tile->setToolTip(QStringLiteral("Modifier la fiche de %1").arg(e.name));
        auto *box=new QVBoxLayout(tile); box->setContentsMargins(12,12,12,12); box->setSpacing(4);
        auto *avatar=new QLabel(initials(e.name)); avatar->setProperty("avatar",true); avatar->setFixedSize(38,38); avatar->setAlignment(Qt::AlignCenter);
        auto *name=new QLabel(e.name); name->setStyleSheet("font-weight:600;"); auto *role=new QLabel(e.specialty); role->setProperty("muted",true); role->setWordWrap(true);
        auto *row=new QHBoxLayout; row->setSpacing(6); auto *loadLabel=new QLabel("Charge"); loadLabel->setProperty("muted",true); auto *percent=new QLabel(QString::number(e.load)+" %"); percent->setProperty("muted",true);
        row->addWidget(loadLabel); row->addStretch(); row->addWidget(percent);
        auto *bar=new QProgressBar; bar->setRange(0,100); bar->setValue(e.load); bar->setTextVisible(false); bar->setProperty("high",e.load>=90);
        box->addWidget(avatar); box->addWidget(name); box->addWidget(role); box->addStretch(); box->addLayout(row); box->addWidget(bar);
        for(QLabel *l:{avatar,name,role,loadLabel,percent}) l->setAttribute(Qt::WA_TransparentForMouseEvents);
        bar->setAttribute(Qt::WA_TransparentForMouseEvents);
        const qint64 id=e.id; connect(tile,&QPushButton::clicked,this,[this,id]{editEmployee(id);});
        ui->teamGrid->addWidget(tile,i/3,i%3);
    }
    for(int col=0;col<3;++col) ui->teamGrid->setColumnStretch(col,1);
}
void EmployeeModule::updateStatistics() {
    QMap<QString,int> specialties,positions; int available=0,loadSum=0;
    for(const auto &e:employees) {++specialties[e.specialty]; ++positions[e.position]; available+=e.available; if(e.available) loadSum+=e.load;}
    ui->employeeStatsTable->setRowCount(0);
    for(auto i=specialties.cbegin();i!=specialties.cend();++i) addRow(ui->employeeStatsTable,{"Spécialité",i.key(),QString::number(i.value())});
    addRow(ui->employeeStatsTable,{"Disponibilité","Disponible",QString::number(available)}); addRow(ui->employeeStatsTable,{"Disponibilité","Indisponible",QString::number(employees.size()-available)});
    for(auto i=positions.cbegin();i!=positions.cend();++i) addRow(ui->employeeStatsTable,{"Poste",i.key(),QString::number(i.value())});
    ui->employeeStatsSummary->setText(QStringLiteral("%1 employé(s) · %2 disponible(s) · %3 indisponible(s)").arg(employees.size()).arg(available).arg(employees.size()-available));
    // Cartes : effectif, charge moyenne, heures et heures supplémentaires du mois en cours.
    const QDate today=QDate::currentDate(); int monthMinutes=0,monthOvertime=0;
    for(const auto &a:pointages) if(a.arrival.isValid()&&a.day.year()==today.year()&&a.day.month()==today.month()) {monthMinutes+=a.minutes(); monthOvertime+=a.overtime();}
    ui->statStaffValue->setText(QString::number(employees.size())); ui->statStaffSub->setText(QStringLiteral("%1 disponible(s)").arg(available));
    ui->statLoadValue->setText(QStringLiteral("%1 %").arg(available?loadSum/available:0)); ui->statLoadSub->setText(QStringLiteral("équipe disponible"));
    ui->statHoursValue->setText(QStringLiteral("%1 h").arg(monthMinutes/60)); ui->statHoursSub->setText(QLocale().monthName(today.month()));
    ui->statOvertimeValue->setText(QStringLiteral("%1 h").arg(monthOvertime/60)); ui->statOvertimeSub->setText(QStringLiteral("au-delà de 8 h nettes"));
    // Graphiques : barres par spécialité (effectif) et par employé (charge, rouge à partir de 90 %).
    QVector<QPair<QString,int>> bySpecialty; for(auto i=specialties.cbegin();i!=specialties.cend();++i) bySpecialty.append(qMakePair(i.key(),i.value()));
    std::stable_sort(bySpecialty.begin(),bySpecialty.end(),[](const auto &a,const auto &b){return a.second>b.second;});
    QVector<BarRow> specialtyRows; int most=1; for(const auto &s:bySpecialty) {most=std::max(most,s.second); specialtyRows.append(BarRow{s.first,double(s.second),QString::number(s.second),false});}
    QVector<Employee> byLoad=employees; std::stable_sort(byLoad.begin(),byLoad.end(),[](const Employee &a,const Employee &b){return a.load>b.load;});
    QVector<BarRow> loadRows; for(const auto &e:byLoad) loadRows.append(BarRow{e.name,double(e.load),QStringLiteral("%1 %").arg(e.load),e.load>=90});
    const qreal ratio=devicePixelRatioF();
    ui->employeeChart->setPixmap(barsImage(specialtyRows,most,std::max(320,ui->employeeChart->width()),150,ratio));
    ui->loadChart->setPixmap(barsImage(loadRows,100,std::max(320,ui->loadChart->width()),120,ratio));
}
void EmployeeModule::resizeEvent(QResizeEvent *event) {QWidget::resizeEvent(event); if(!initialized) return; if(ui->employeeTabs->currentIndex()==4) QTimer::singleShot(0,this,[this]{updateStatistics();}); if(ui->employeeTabs->currentIndex()==0) QTimer::singleShot(0,this,[this]{updateHoursChart();});}
