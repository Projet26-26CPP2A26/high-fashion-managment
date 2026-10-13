#include "employeewindow.h"
#include "ui_employeewindow.h"
#include "employeemodule.h"
#include "pdfexport.h"
#include "icons.h"
#include <QApplication>
#include <QButtonGroup>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QShortcut>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

EmployeeWindow::EmployeeWindow(QSqlDatabase database,bool demo,QWidget *parent)
    :QMainWindow(parent),ui(new Ui::EmployeeWindow),employeeModule(nullptr) {
    ui->setupUi(this);
    // Le style est propre à ce module (il ne modifie pas celui du module Clients).
    QFile theme(QStringLiteral(":/theme.qss")); if(theme.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(theme.readAll()));
    employeeModule=new EmployeeModule(database,demo,this);
    ui->contentLayout->addWidget(employeeModule);
    employeeModule->findChild<QTabWidget*>("employeeTabs")->tabBar()->hide();
    ui->logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo_aurea.png")).scaled(70,70,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    const Icons::Kind kinds[]={Icons::Grid,Icons::People,Icons::Assign,Icons::Clock};
    ui->helpButton->setIcon(Icons::nav(Icons::Help)); ui->helpButton->setIconSize(QSize(20,20)); ui->helpButton->setCursor(Qt::PointingHandCursor);
    // Recherche globale : filtre la liste des employés et ouvre cette page.
    ui->globalSearch->addAction(QIcon(Icons::pixmap(Icons::Search,QColor("#8f8c86"),16)),QLineEdit::LeadingPosition);
    connect(ui->globalSearch,&QLineEdit::textChanged,this,[this](const QString &text){
        employeeModule->findChild<QLineEdit*>("employeeSearch")->setText(text);
        if(!text.trimmed().isEmpty()) setPage(1);
    });
    auto *group=new QButtonGroup(this); group->setExclusive(true);
    const QList<QPushButton*> buttons={ui->navDashboard,ui->navEmployees,ui->navAssignment,ui->navAttendance};
    for(int i=0;i<buttons.size();++i) {
        group->addButton(buttons[i],i); buttons[i]->setIcon(Icons::nav(kinds[i])); buttons[i]->setIconSize(QSize(20,20)); buttons[i]->setCursor(Qt::PointingHandCursor);
        connect(buttons[i],&QPushButton::clicked,this,[this,i]{setPage(i);});
        auto *shortcut=new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i+1)),this);
        connect(shortcut,&QShortcut::activated,this,[this,i]{setPage(i);});
    }
    connect(employeeModule->findChild<QTabWidget*>("employeeTabs"),&QTabWidget::currentChanged,this,[this](int i){setPage(i);});
    connect(ui->helpButton,&QPushButton::clicked,this,[this,database]{
        QMessageBox::information(this,"AUREA — Guide",QStringLiteral(
            "Employés : ajouter, modifier, supprimer, rechercher, filtrer, trier et exporter en PDF.\n\n"
            "Affectation : modèle de tâche, spécialité, durée, classement puis confirmation. Une surcharge est refusée. Terminer une tâche libère sa charge.\n\n"
            "Pointage : arrivée / départ ou horaires manuels. Déduisez la pause réelle. Au-delà de 8 h nettes : heures supplémentaires.\n\n"
            "Statistiques : bouton « Statistiques » de la page Employés ; « Retour » pour en sortir.\n\n"
            "Ctrl+1 à Ctrl+4 : changer de page. Double-clic sur une fiche pour modifier.\n\nBase active : ")+database.databaseName());
    });
    auto *timer=new QTimer(this);
    connect(timer,&QTimer::timeout,this,[this]{ui->dateLabel->setText(QLocale().toString(QDateTime::currentDateTime(),"dddd d MMMM yyyy · HH:mm"));}); timer->start(20000);
    ui->dateLabel->setText(QLocale().toString(QDateTime::currentDateTime(),"dddd d MMMM yyyy · HH:mm"));
    setPage(0);
}
EmployeeWindow::~EmployeeWindow() {delete ui;}
bool EmployeeWindow::ready() const {return employeeModule->ready();}
void EmployeeWindow::setSidebarVisible(bool visible) {ui->sidebar->setVisible(visible);}
QList<QPushButton*> EmployeeWindow::navButtons() const {return {ui->navDashboard,ui->navEmployees,ui->navAssignment,ui->navAttendance};}
QPushButton *EmployeeWindow::helpButton() const {return ui->helpButton;}
void EmployeeWindow::setPage(int index) {
    static const QStringList titles={"Tableau de bord","Employés","Affectation intelligente","Pointage","Statistiques"};
    static const QStringList subtitles={"L'équipe de votre atelier, au quotidien.","Fiches, disponibilité et charge de travail.","Spécialité, disponibilité et capacité : un choix expliqué.","Présence, pauses et heures supplémentaires.","Comprendre la répartition de votre effectif."};
    if(index<0||index>=titles.size()) return;
    employeeModule->setSection(index); ui->pageTitle->setText(titles[index]); ui->subtitle->setText(subtitles[index]);
    const QList<QPushButton*> buttons={ui->navDashboard,ui->navEmployees,ui->navAssignment,ui->navAttendance}; buttons[index==4?1:index]->setChecked(true);  // Statistiques s'ouvre depuis Employés : on garde « Employés » allumé.
}
void EmployeeWindow::renderScreenshots(const QString &directory) {
    QDir().mkpath(directory); resize(1380,900);
    const QStringList names={"01-dashboard","02-employes","03-affectation","04-pointage","05-statistiques"};
    employeeModule->findChild<QLineEdit*>("taskTitle")->setText("Broderie de fils d'or sur manteau");
    for(int i=0;i<names.size();++i) {setPage(i); QApplication::processEvents(); grab().save(QDir(directory).filePath(names[i]+".png"));}
    Reports::writePdf(QDir(directory).filePath("06-employes.pdf"),employeeModule->employeesHtml(),nullptr,true);
    Reports::writePdf(QDir(directory).filePath("07-statistiques.pdf"),employeeModule->statisticsHtml(),nullptr,true);
}
