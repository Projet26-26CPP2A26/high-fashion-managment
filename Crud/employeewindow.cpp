#include "employeewindow.h"
#include "ui_employeewindow.h"
#include "employeemodule.h"
#include "activityboard.h"
#include "badgepage.h"
#include "passagefeed.h"
#include "companylink.h"
#include "companysimulator.h"
#include "polish.h"
#include "dialogskin.h"
#include "UiAnimations.h"
#include "LogoAnimation.h"
#include "TransitionAnimation.h"
#include "reviewboard.h"
#include "icons.h"
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QShortcut>
#include <QStackedWidget>
#include <QSqlDatabase>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>

EmployeeWindow::EmployeeWindow(ActivityStore &store, bool demo, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::EmployeeWindow)
    , employeeModule(nullptr)
    , board(nullptr)
    , pages(nullptr)
{
    ui->setupUi(this);

    QFile theme(QStringLiteral(":/theme.qss"));
    if (theme.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(theme.readAll()));
    }
    MorphOverlay::breathe();
    employeeModule = new EmployeeModule(this);
    MorphOverlay::breathe();
    MorphOverlay::breathe();
    board = new ActivityBoard(store, this);
    MorphOverlay::breathe();
    pages = new QStackedWidget(this);
    pages->addWidget(board);
    pages->addWidget(employeeModule);
    reviews = new ReviewBoard(*board, this);
    pages->addWidget(reviews);
    reviewButton = new QPushButton(QStringLiteral("  Revues"), this);
    reviewButton->setProperty("nav", true);
    reviewButton->setCheckable(true);
    reviewButton->setIcon(Icons::nav(Icons::Check));
    reviewButton->setIconSize(QSize(20, 20));
    reviewButton->setMinimumHeight(44);
    reviewButton->setCursor(Qt::PointingHandCursor);
    ui->sidebarLayout->insertWidget(ui->sidebarLayout->indexOf(ui->navEmployees), reviewButton);
    connect(reviewButton, &QPushButton::clicked, this, &EmployeeWindow::showReviews);
    badges = new BadgePage(employeeModule->repository(), store, this);
    pages->addWidget(badges);
    badgeButton = new QPushButton(QStringLiteral("  Badges"), this);
    badgeButton->setProperty("nav", true);
    badgeButton->setCheckable(true);
    badgeButton->setIcon(Icons::nav(Icons::Badge));
    badgeButton->setIconSize(QSize(20, 20));
    badgeButton->setMinimumHeight(44);
    badgeButton->setCursor(Qt::PointingHandCursor);
    ui->sidebarLayout->insertWidget(ui->sidebarLayout->indexOf(ui->navAttendance) + 1, badgeButton);
    connect(badgeButton, &QPushButton::clicked, this, &EmployeeWindow::showBadges);
    employeeModule->findChild<QWidget *>("attendancePage")->layout()->addWidget(new PassageFeed(store));
    connect(badges, &BadgePage::attendanceChanged, employeeModule, &EmployeeModule::refresh);
    if (employeeModule->ready()) {
        auto *simulator = new CompanySimulator(
            store, employeeModule->repository().loadEmployees(), this, QString(), demo, badges->holders());
        connect(badges, &BadgePage::passed, simulator, &CompanySimulator::badge);
        connect(employeeModule, &QObject::destroyed, simulator, &QObject::deleteLater);
        auto *people = new QTimer(this);
        people->start(15000);
        connect(people, &QTimer::timeout, simulator, [this, simulator] {
            const auto list = employeeModule->repository().loadEmployees();
            if (employeeModule->repository().error().isEmpty()) {
                simulator->setEmployees(list);
            }
        });
    }
    if (employeeModule->ready()) {
        link = new CompanyLink(employeeModule->repository(), store, *board, this);
        board->assignHandler = [this](int id, const QString &title, int minutes) {
            return link->assignTask(id, title, minutes);
        };
        connect(link, &CompanyLink::changed, employeeModule, &EmployeeModule::refresh);
    }
    for (const AccessEntry &e : store.entries(200)) {
        lastEntry = std::max(lastEntry, e.id);
    }
    auto *badge = new QTimer(this);
    badge->start(1000);
    connect(badge, &QTimer::timeout, this, [this, &store] {
        for (const AccessEntry &e : store.entriesAfter(lastEntry)) {
            lastEntry = std::max(lastEntry, e.id);
            if (e.kind != "REFUSE") {
                continue;
            }
            if (pages->currentIndex() != 3) {
                ++refusals;
            }
        }
        badgeButton->setText(refusals ? QStringLiteral("  Badges   ·  %1").arg(refusals) : QStringLiteral("  Badges"));
    });
    connect(badge, &QTimer::timeout, this, [this] {
        const int waiting = int(board->reviews().size());
        reviewButton->setText(waiting ? QStringLiteral("  Revues   ·  %1").arg(waiting) : QStringLiteral("  Revues"));
    });
    MorphOverlay::breathe();
    ui->contentLayout->addWidget(pages);
    MorphOverlay::breathe();
    auto *logout = new QPushButton(QStringLiteral("  Déconnexion"), this);
    logout->setProperty("nav", true);
    logout->setIcon(Icons::nav(Icons::Badge));
    logout->setIconSize(QSize(20, 20));
    logout->setMinimumHeight(44);
    logout->setCursor(Qt::PointingHandCursor);
    ui->sidebarLayout->addWidget(logout);
    connect(logout, &QPushButton::clicked, this, [this] {
        logoutRequested = true;
        close();
    });
    employeeModule->findChild<QTabWidget *>("employeeTabs")->tabBar()->hide();
    ui->logo->setPixmap(QPixmap(QStringLiteral(":/assets/logo_aurea.png"))
            .scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const Icons::Kind kinds[] = {Icons::Grid, Icons::People, Icons::Assign, Icons::Clock};
    ui->helpButton->setIcon(Icons::nav(Icons::Help));
    ui->helpButton->setIconSize(QSize(20, 20));
    ui->helpButton->setCursor(Qt::PointingHandCursor);

    ui->globalSearch->addAction(QIcon(Icons::pixmap(Icons::Search, QColor("#8f8c86"), 16)), QLineEdit::LeadingPosition);
    connect(ui->globalSearch, &QLineEdit::textChanged, this, [this](const QString &text) {
        employeeModule->findChild<QLineEdit *>("employeeSearch")->setText(text);
        board->setSearch(text);
        if (!text.trimmed().isEmpty() && pages->currentIndex() == 1) {
            setPage(1);
        }
    });
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    const QList<QPushButton *> buttons = {ui->navDashboard, ui->navEmployees, ui->navAssignment, ui->navAttendance};
    for (int i = 0; i < buttons.size(); ++i) {
        group->addButton(buttons[i], i);
        buttons[i]->setIcon(Icons::nav(kinds[i]));
        buttons[i]->setIconSize(QSize(20, 20));
        buttons[i]->setCursor(Qt::PointingHandCursor);
        connect(buttons[i], &QPushButton::clicked, this, [this, i] {
            setPage(i);
        });
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i] {
            setPage(i);
        });
    }
    connect(employeeModule->findChild<QTabWidget *>("employeeTabs"), &QTabWidget::currentChanged, this, [this](int i) {
        setPage(i);
    });
    connect(ui->helpButton, &QPushButton::clicked, this, [this] {
        DialogSkin::info(this, "Guide AUREA",
            QStringLiteral(
                "Employés : ajouter, modifier, supprimer, rechercher, filtrer, trier et exporter en PDF.\n\n"
                "Affectation : modèle de tâche, spécialité, durée, classement puis confirmation. Une surcharge est refusée. Terminer une tâche libère sa charge.\n\n"
                "Pointage : tous les passages en direct, au bureau et en ligne. Cliquez sur un passage pour voir la journée.\n\nBadges : carte Arduino, association des badges et accès refusés.\n\n"
                "Statistiques : bouton « Statistiques » de la page Employés ; « Retour » pour en sortir.\n\n"
                "Ctrl+1 à Ctrl+4 : changer de page. F11 : plein écran. Double-clic sur une fiche pour modifier.\n\nBase active : ")
                + QSqlDatabase::database().databaseName());
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        ui->dateLabel->setText(QLocale().toString(QDateTime::currentDateTime(), "dddd d MMMM yyyy · HH:mm"));
    });
    timer->start(20000);
    ui->dateLabel->setText(QLocale().toString(QDateTime::currentDateTime(), "dddd d MMMM yyyy · HH:mm"));
    group->addButton(reviewButton, 5);
    group->addButton(badgeButton, 6);
    Fx::navIndicator(ui->sidebar,
        {ui->navDashboard, reviewButton, ui->navEmployees, ui->navAssignment, ui->navAttendance, badgeButton});
    MorphOverlay::breathe();
    TablePolish::apply(this);
    MorphOverlay::breathe();
    Fx::fullScreenKey(this);
    setPage(0);
    LivingLogo::attach(ui->logo);
}
EmployeeWindow::~EmployeeWindow()
{
    delete ui;
}
void EmployeeWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    if (leaving) {
        return;
    }
    leaving = true;
    for (QTimer *t : findChildren<QTimer *>()) {
        t->stop();
    }
    emit closed(logoutRequested);
}
bool EmployeeWindow::ready() const
{
    return employeeModule->ready();
}
void EmployeeWindow::setSidebarVisible(bool visible)
{
    ui->sidebar->setVisible(visible);
}
QList<QPushButton *> EmployeeWindow::navButtons() const
{
    return {ui->navDashboard, ui->navEmployees, ui->navAssignment, ui->navAttendance};
}
QPushButton *EmployeeWindow::helpButton() const
{
    return ui->helpButton;
}
void EmployeeWindow::showReviews()
{
    ui->pageTitle->setText(QStringLiteral("Revues"));
    ui->subtitle->setText(QStringLiteral("Dépôts des employés : validez, notez ou demandez une correction."));
    reviewButton->setChecked(true);
    Fx::switchPage(pages, 2);
    reviews->enter();
}
void EmployeeWindow::showBadges()
{
    ui->pageTitle->setText(QStringLiteral("Badges"));
    ui->subtitle->setText(QStringLiteral("Carte Arduino, titulaires des badges et accès refusés."));
    badgeButton->setChecked(true);
    refusals = 0;
    badges->enter();
    Fx::switchPage(pages, 3);
}
void EmployeeWindow::setPage(int index)
{
    static const QStringList titles = {
        "Tableau de bord", "Employés", "Affectation intelligente", "Pointage", "Statistiques"};
    static const QStringList subtitles = {"Activité, temps de travail et pauses de l'équipe, en direct.",
        "Fiches, disponibilité et charge de travail.", "Spécialité, disponibilité et capacité : un choix expliqué.",
        "Tous les passages, au bureau et en ligne, en direct.", "Comprendre la répartition de votre effectif."};
    if (index < 0 || index >= titles.size()) {
        return;
    }
    employeeModule->setSection(index);
    Fx::switchPage(pages, index == 0 ? 0 : 1);
    ui->pageTitle->setText(titles[index]);
    ui->subtitle->setText(subtitles[index]);
    const QList<QPushButton *> buttons = {ui->navDashboard, ui->navEmployees, ui->navAssignment, ui->navAttendance};
    buttons[index == 4 ? 1 : index]->setChecked(true);
}
