#include "employeedesk.h"
#include "employeetile.h"
#include "dialogskin.h"
#include "UiAnimations.h"
#include "LogoAnimation.h"
#include "polish.h"
#include "submitdialog.h"
#include "taskcatalog.h"
#include "timecard.h"
#include "icons.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSessionManager>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
const QString kWork = QStringLiteral("Actif"), kPause = QStringLiteral("En pause"), kIdle = QStringLiteral("Inactif");
const QString kNoTask = QStringLiteral("Aucune tâche en cours");
const int kIdleLimit = 60;
const int kSleepGap = 30;

int systemIdleSeconds()
{
#ifdef Q_OS_WIN
    LASTINPUTINFO info;
    info.cbSize = sizeof(info);
    if (GetLastInputInfo(&info)) return int((GetTickCount() - info.dwTime) / 1000);
#endif
    return 0;
}

QFrame *card(QLayout *content)
{
    auto *frame = new QFrame;
    frame->setProperty("card", true);
    content->setContentsMargins(20, 16, 20, 16);
    frame->setLayout(content);
    return frame;
}

QLabel *label(const QString &text, const char *role)
{
    auto *l = new QLabel(text);
    l->setTextFormat(Qt::PlainText);
    l->setProperty(role, true);
    return l;
}

QLabel *statCard(QHBoxLayout *row, const QString &title, QLabel **info = nullptr)
{
    auto *box = new QVBoxLayout;
    auto *value = label("00:00:00", "kpi");
    box->addWidget(label(title, "muted"));
    box->addWidget(value);
    auto *note = label(QString(), "muted");
    box->addWidget(note);
    if (info) *info = note;
    row->addWidget(card(box), 1);
    return value;
}

QString minutesText(int seconds) { return QStringLiteral("%1 min").arg((std::max(0, seconds) + 59) / 60); }
}

EmployeeDesk::EmployeeDesk(ActivityStore &store, const Employee &employee, const QString &via, QWidget *parent) : QMainWindow(parent), m_store(store), m_employee(employee)
{
    setWindowTitle(QStringLiteral("AUREA — Espace employé"));
    setMinimumSize(1020, 600);
    resize(1240, 780);
    Fx::fullScreenKey(this);
    QFile theme(":/theme.qss");
    if (theme.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(theme.readAll()) +
                      "QFrame#chip{background:#242427;border:1px solid #3a3a3f;border-radius:12px;} QLabel#clock{font-size:13pt;font-weight:700;color:#f4e7bc;}"
                      "QLabel#state{font-size:9pt;font-weight:600;} QLabel#taskName{font-size:19pt;font-weight:700;color:white;} QLabel#nextName{font-size:13pt;font-weight:600;}"
                      "QPushButton#pause{padding:6px 14px;}");

    m_session.name = m_employee.reference();
    m_session.position = m_employee.position();
    m_session.via = via;
    m_session.state = kWork;
    m_session.task = kNoTask;
    m_session.login = m_session.lastSeen = m_last = QDateTime::currentDateTime();
    m_store.closeLostSessions(m_session.name);
    for (const WorkSession &s : m_store.sessions(m_session.login.date()))
        if (s.name == m_session.name) { m_baseWork += s.work; m_basePause += s.pause; }
    if (!m_store.hasTaskHistory(m_session.name)) {
        for (const TaskModel &m : TaskCatalog::forSpecialty(specialty())) {
            WorkTask t;
            t.who = m_session.name;
            t.title = m.title;
            t.details = m.details;
            t.steps = m.steps;
            t.planned = m.minutes * 60;
            t.specialty = specialty();
            m_store.addTask(t);
        }
    }
    if (m_store.start(m_session)) m_store.addEntry(m_session.name, m_employee.name(), via, QStringLiteral("ARRIVEE"));

    auto *central = new QWidget;
    central->setObjectName("centralwidget");
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(216);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 22, 16, 18);
    side->setSpacing(6);
    auto *logo = new QLabel;
    logo->setObjectName("logo");
    logo->setPixmap(QPixmap(":/assets/logo_aurea.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    auto *word = new QLabel("AUREA");
    word->setObjectName("logoWord");
    word->setAlignment(Qt::AlignCenter);
    side->addWidget(logo);
    LivingLogo::attach(logo);
    side->addWidget(word);
    side->addSpacing(20);
    const QStringList names = {QStringLiteral("  Ma journée"), QStringLiteral("  Mes tâches"), QStringLiteral("  Mon activité")};
    const Icons::Kind kinds[] = {Icons::Grid, Icons::Assign, Icons::Clock};
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton(names[i]);
        b->setProperty("nav", true);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setIcon(Icons::nav(kinds[i]));
        b->setIconSize(QSize(20, 20));
        b->setMinimumHeight(44);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i] { showPage(i); });
        connect(new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this), &QShortcut::activated, this, [this, i] { showPage(i); });
        side->addWidget(b);
        m_nav.append(b);
    }
    side->addStretch();
    auto *logout = new QPushButton(QStringLiteral("  Fin de journée"));
    logout->setProperty("nav", true);
    logout->setIcon(Icons::nav(Icons::Badge));
    logout->setIconSize(QSize(20, 20));
    logout->setMinimumHeight(44);
    side->addWidget(logout);
    root->addWidget(sidebar);

    auto *main = new QVBoxLayout;
    main->setContentsMargins(28, 18, 28, 16);
    main->setSpacing(16);
    auto *top = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName("pageTitle");
    m_subtitle = new QLabel;
    m_subtitle->setObjectName("subtitle");
    titles->setSpacing(0);
    titles->addWidget(m_title);
    titles->addWidget(m_subtitle);
    titles->addStretch();
    top->addLayout(titles);
    top->addStretch();
    auto *chip = new QFrame;
    chip->setObjectName("chip");
    auto *chipRow = new QHBoxLayout(chip);
    chipRow->setContentsMargins(14, 7, 8, 7);
    chipRow->setSpacing(12);
    m_state = new QLabel;
    m_state->setObjectName("state");
    m_clock = new QLabel;
    m_clock->setObjectName("clock");
    m_pauseButton = new QPushButton;
    m_pauseButton->setObjectName("pause");
    m_pauseButton->setProperty("primary", true);
    m_clockInfo = label(QString(), "muted");
    auto *clockBox = new QVBoxLayout;
    clockBox->setSpacing(0);
    clockBox->addWidget(m_clock);
    clockBox->addWidget(m_clockInfo);
    chipRow->addWidget(m_state);
    chipRow->addLayout(clockBox);
    m_chip = chip;
    chip->setCursor(Qt::PointingHandCursor);
    chip->setToolTip(QStringLiteral("Cliquez pour basculer entre temps restant et temps passé"));
    chip->installEventFilter(this);
    chipRow->addWidget(m_pauseButton);
    top->addWidget(chip);
    main->addLayout(top);

    m_pages = new QStackedWidget;
    m_pages->addWidget(createDayPage());
    m_pages->addWidget(createTasksPage());
    m_pages->addWidget(createLogPage());
    main->addWidget(m_pages, 1);
    m_notice = label(QStringLiteral("Transparence : le responsable RH voit vos heures, pauses, inactivité, tâches et l'état du PC. "
                                    "Aucune capture d'écran, aucune frappe clavier n'est enregistrée."), "muted");
    m_notice->setWordWrap(true);
    m_notice->setToolTip(QStringLiteral("F11 : plein écran"));
    main->addWidget(m_notice);
    root->addLayout(main, 1);
    setCentralWidget(central);

    connect(m_pauseButton, &QPushButton::clicked, this, &EmployeeDesk::togglePause);
    connect(logout, &QPushButton::clicked, this, [this] { m_logout = true; close(); });
    connect(qApp, &QGuiApplication::commitDataRequest, this, [this](QSessionManager &) { finish(QStringLiteral("Arrêt ou fermeture de session Windows")); }, Qt::DirectConnection);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { tick(QDateTime::currentDateTime(), systemIdleSeconds()); });
    timer->start(1000);
    auto *sync = new QTimer(this);
    connect(sync, &QTimer::timeout, this, [this] { if (!m_finished) loadTasks(); });
    sync->start(3000);

    TablePolish::apply(this);
    Fx::navIndicator(sidebar, m_nav);
    addLog(QStringLiteral("Connexion — chronomètre démarré"));
    loadTasks();
    if (m_current.id == 0 && !m_tasks.isEmpty() && (m_tasks.first().state == WorkTask::Todo || m_tasks.first().state == WorkTask::Rework)) startTask(m_tasks.first().id);
    showPage(0);
    refresh();
}

QWidget *EmployeeDesk::createDayPage()
{
    auto *page = new QWidget;
    auto *box = new QVBoxLayout(page);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(14);

    auto *current = new QHBoxLayout;
    m_taskRing = new ProgressRing(170);
    auto *info = new QVBoxLayout;
    info->setSpacing(6);
    m_taskName = new QLabel;
    m_taskName->setObjectName("taskName");
    m_taskName->setTextFormat(Qt::PlainText);
    m_taskName->setWordWrap(true);
    m_taskInfo = label(QString(), "muted");
    m_finishButton = new QPushButton(QStringLiteral("Déposer pour revue  ↗"));
    m_finishButton->setProperty("primary", true);
    m_finishButton->setMinimumWidth(220);
    info->addStretch();
    info->addWidget(label(QStringLiteral("Tâche en cours"), "muted"));
    info->addWidget(m_taskName);
    m_details = label(QString(), "muted");
    m_details->setWordWrap(true);
    info->addWidget(m_details);
    info->addWidget(m_taskInfo);
    m_steps = new QVBoxLayout;
    m_steps->setSpacing(6);
    info->addSpacing(4);
    info->addLayout(m_steps);
    info->addSpacing(8);
    info->addWidget(m_finishButton, 0, Qt::AlignLeft);
    info->addStretch();
    current->setSpacing(26);
    current->addWidget(m_taskRing, 2);
    current->addLayout(info, 3);

    m_dayRing = new ProgressRing(130);
    auto *day = new QVBoxLayout;
    day->addWidget(label(QStringLiteral("Ma journée de 8 h"), "section"));
    day->addWidget(m_dayRing, 1);

    auto *next = new QVBoxLayout;
    next->setSpacing(6);
    m_next = new QLabel;
    m_next->setObjectName("nextName");
    m_next->setTextFormat(Qt::PlainText);
    m_next->setWordWrap(true);
    m_nextInfo = label(QString(), "muted");
    m_nextButton = new QPushButton(QStringLiteral("Passer à cette tâche  ›"));
    next->addWidget(label(QStringLiteral("Tâche suivante"), "muted"));
    next->addWidget(m_next);
    next->addWidget(m_nextInfo);
    next->addSpacing(4);
    next->addWidget(m_nextButton);

    auto *right = new QVBoxLayout;
    right->setSpacing(14);
    right->addWidget(card(day), 1);
    right->addWidget(card(next));
    auto *top = new QHBoxLayout;
    top->setSpacing(14);
    top->addWidget(card(current), 5);
    top->addLayout(right, 2);
    box->addLayout(top, 1);

    auto *stats = new QHBoxLayout;
    stats->setSpacing(14);
    stats->addWidget(timeCard(), 3);
    m_idle = statCard(stats, QStringLiteral("Inactivité"));
    m_done = statCard(stats, QStringLiteral("Tâches validées"), &m_quality);
    box->addLayout(stats);
    auto *timeline = new QVBoxLayout;
    m_timelines << new DayTimeline;
    timeline->addWidget(label(QStringLiteral("Ma session en un coup d'œil"), "section"));
    timeline->addWidget(m_timelines.last());
    box->addWidget(card(timeline));

    connect(m_finishButton, &QPushButton::clicked, this, &EmployeeDesk::openSubmit);
    connect(m_nextButton, &QPushButton::clicked, this, [this] {
        for (const WorkTask &t : std::as_const(m_tasks))
            if (t.state == WorkTask::Todo || t.state == WorkTask::Rework) { startTask(t.id); return; }
        addLog(QStringLiteral("Demande de nouvelle tâche"));
        m_store.log(m_session, QStringLiteral("Demande une nouvelle tâche"));
        Fx::toast(this, QStringLiteral("Demande envoyée au responsable RH"));
    });
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    return scroll;
}

QWidget *EmployeeDesk::createTasksPage()
{
    auto *page = new QWidget;
    auto *row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    const QStringList names = {QStringLiteral("À faire"), QStringLiteral("En cours"), QStringLiteral("En revue"), QStringLiteral("Validées")};
    const QStringList colors = {"#b9b4a8", "#e2cf9f", "#f4d27a", "#7fc08a"};
    for (int i = 0; i < 4; ++i) {
        auto *column = new QVBoxLayout;
        auto *head = new QHBoxLayout;
        auto *name = label(names[i], "section");
        name->setStyleSheet("color:" + colors[i]);
        m_counts[i] = label("0", "pill");
        head->addWidget(name);
        head->addWidget(m_counts[i]);
        head->addStretch();
        auto *line = new QFrame;
        line->setFixedHeight(2);
        line->setStyleSheet("background:" + colors[i]);
        auto *holder = new QWidget;
        holder->setObjectName("dashboardContents");
        m_columns[i] = new QVBoxLayout(holder);
        holder->setStyleSheet("QWidget#dashboardContents{background:#1f1f22;border-radius:12px;}");
        m_columns[i]->setContentsMargins(10, 10, 10, 10);
        m_columns[i]->setSpacing(10);
        m_columns[i]->setAlignment(Qt::AlignTop);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(holder);
        column->addLayout(head);
        column->addWidget(line);
        column->addWidget(scroll, 1);
        row->addLayout(column, 1);
    }
    return page;
}

QWidget *EmployeeDesk::createLogPage()
{
    auto *page = new QWidget;
    auto *box = new QVBoxLayout(page);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(14);
    auto *stats = new QHBoxLayout;
    stats->setSpacing(14);
    stats->addWidget(timeCard(), 3);
    m_idleToday = statCard(stats, QStringLiteral("Inactivité"));
    box->addLayout(stats);

    auto *timeline = new QVBoxLayout;
    m_timelines << new DayTimeline;
    timeline->addWidget(label(QStringLiteral("Ma session en un coup d'œil"), "section"));
    timeline->addWidget(m_timelines.last());
    box->addWidget(card(timeline));

    auto *journal = new QVBoxLayout;
    journal->setSpacing(10);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    head->addWidget(label(QStringLiteral("Journal"), "section"));
    head->addStretch();
    m_log = new ActivityLog;
    auto *group = new QButtonGroup(this);
    for (int i = 0; i < 5; ++i) {
        auto *b = m_filters[i] = new QPushButton;
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton{padding:5px 12px;} QPushButton:checked{background:#3a3427;border:1px solid #c9b27c;color:#f1e1b4;}");
        group->addButton(b);
        head->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { m_log->setFilter(i - 1); });
    }
    journal->addLayout(head);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_log);
    scroll->setStyleSheet("QScrollArea,QScrollArea>QWidget>QWidget{background:transparent;}");
    journal->addWidget(scroll, 1);
    box->addWidget(card(journal), 1);
    return page;
}

void EmployeeDesk::showPage(int index)
{
    const QStringList titles = {QStringLiteral("Bonjour, ") + m_employee.name(), QStringLiteral("Mes tâches"), QStringLiteral("Mon activité")};
    const QStringList subtitles = {m_employee.reference() + QStringLiteral(" · ") + m_employee.position(), specialty() + QStringLiteral(" · de « à faire » à « validée »"),
                                          QStringLiteral("Tout ce qui a été enregistré pendant cette session.")};
    m_title->setText(titles[index]);
    m_subtitle->setText(subtitles[index]);
    m_nav[index]->setChecked(true);
    Fx::switchPage(m_pages, index);
}

void EmployeeDesk::togglePause()
{
    tick(QDateTime::currentDateTime(), 0);
    m_paused = !m_paused;
    setState(m_paused ? kPause : kWork, m_paused ? QStringLiteral("Début de pause") : QStringLiteral("Reprise du travail"));
    Fx::toast(this, m_paused ? QStringLiteral("Bonne pause !") : QStringLiteral("Chronomètre relancé"));
}

void EmployeeDesk::loadTasks()
{
    if (m_current.id) m_store.saveTaskTime(m_current.id, m_current.spent);
    const QVector<WorkTask> fresh = m_store.tasks(m_session.name);
    if (!m_store.error().isEmpty()) { m_notice->setText(QStringLiteral("Lecture impossible : ") + m_store.error()); return; }
    int maxId = 0;
    QString added;
    for (const WorkTask &t : fresh) {
        maxId = std::max(maxId, t.id);
        if (m_maxTask >= 0 && t.id > m_maxTask) added = t.title;
    }
    for (const WorkTask &t : fresh) {
        for (const WorkTask &old : std::as_const(m_tasks)) {
            if (old.id != t.id || old.state != WorkTask::Submitted || t.state == WorkTask::Submitted) continue;
            const bool ok = t.state == WorkTask::Done;
            addLog((ok ? QStringLiteral("Validée %1/5 : ").arg(t.rating) : QStringLiteral("À corriger : ")) + t.title + (t.feedback.isEmpty() ? QString() : QStringLiteral(" — ") + t.feedback));
            Fx::toast(this, ok ? QStringLiteral("Dépôt validé  %1/5 ★  %2").arg(t.rating).arg(t.title) : QStringLiteral("Correction demandée : ") + t.title);
        }
    }
    bool changed = m_maxTask < 0 || fresh.size() != m_tasks.size();
    for (int i = 0; !changed && i < fresh.size(); ++i) changed = fresh[i].id != m_tasks[i].id || fresh[i].state != m_tasks[i].state;
    m_maxTask = std::max(m_maxTask, maxId);
    m_tasks = fresh;
    m_current = WorkTask();
    for (const WorkTask &t : fresh) if (t.state == WorkTask::Current) m_current = t;
    m_session.task = m_current.id ? m_current.title : kNoTask;
    if (!added.isEmpty()) {
        addLog(QStringLiteral("Nouvelle tâche reçue : ") + added);
        Fx::toast(this, QStringLiteral("Nouvelle tâche : ") + added);
    }
    if (changed) showTasks();
    refresh();
}

void EmployeeDesk::startTask(int id)
{
    if (m_current.id) m_store.saveTaskTime(m_current.id, m_current.spent);
    if (!m_store.setTaskState(m_session.name, id, WorkTask::Current)) { m_notice->setText(m_store.error()); return; }
    loadTasks();
    setState(m_session.state, QStringLiteral("Tâche démarrée : ") + m_current.title);
}

void EmployeeDesk::setStep(int index, bool done)
{
    if (!m_current.id || index >= m_current.steps.size()) return;
    const int mask = done ? m_current.stepsDone | (1 << index) : m_current.stepsDone & ~(1 << index);
    if (mask == m_current.stepsDone) return;
    m_current.stepsDone = mask;
    m_store.setSteps(m_current.id, mask);
    if (done) {
        addLog(QStringLiteral("Étape faite : ") + m_current.steps[index]);
        m_store.log(m_session, QStringLiteral("Étape faite : ") + m_current.steps[index]);
    }
    refresh();
}

void EmployeeDesk::showSteps()
{
    m_stepsFor = m_current.id;
    while (QLayoutItem *item = m_steps->takeAt(0)) {
        if (item->widget()) { item->widget()->hide(); item->widget()->deleteLater(); }
        delete item;
    }
    for (int i = 0; i < m_current.steps.size(); ++i) {
        auto *box = new QCheckBox(m_current.steps[i]);
        box->setChecked(m_current.stepsDone & (1 << i));
        connect(box, &QCheckBox::toggled, this, [this, i](bool on) { setStep(i, on); });
        m_steps->addWidget(box);
    }
}

void EmployeeDesk::openSubmit()
{
    if (!m_current.id || !m_current.ready() || m_paused) return;
    const int id = m_current.id;
    SubmitDialog dialog(m_current, this);
    if (DialogSkin::run(dialog) == QDialog::Accepted && m_current.id == id) submitTask(dialog.note(), dialog.file());
}

bool EmployeeDesk::submitTask(const QString &note, const QString &file)
{
    if (!m_current.id || !m_current.ready()) return false;
    const WorkTask done = m_current;
    m_store.saveTaskTime(done.id, done.spent);
    if (!m_store.submit(done.id, note, file)) { m_notice->setText(QStringLiteral("Dépôt impossible. ") + m_store.error()); return false; }
    m_current = WorkTask();
    loadTasks();
    setState(m_session.state, QStringLiteral("Dépôt pour revue : ") + done.title + " (" + clockText(done.spent) + ")");
    Fx::toast(this, QStringLiteral("Déposée : en attente de validation"));
    for (const WorkTask &t : std::as_const(m_tasks))
        if (t.state == WorkTask::Todo || t.state == WorkTask::Rework) { startTask(t.id); break; }
    return true;
}

void EmployeeDesk::showTasks()
{
    int counts[4] = {0, 0, 0, 0};
    for (QVBoxLayout *column : m_columns)
        while (QLayoutItem *item = column->takeAt(0)) {
            if (item->widget()) { item->widget()->hide(); item->widget()->deleteLater(); }
            delete item;
        }
    for (const WorkTask &t : std::as_const(m_tasks)) {
        const int c = t.state == WorkTask::Current ? 1 : t.state == WorkTask::Submitted ? 2 : t.state == WorkTask::Done ? 3 : 0;
        ++counts[c];
        auto *box = new QVBoxLayout;
        box->setSpacing(6);
        auto *title = label(t.title, "section");
        title->setWordWrap(true);
        box->addWidget(title);
        auto *details = label(t.details, "muted");
        details->setWordWrap(true);
        box->addWidget(details);
        if (t.state == WorkTask::Done || t.state == WorkTask::Submitted) {
            box->addWidget(label(QStringLiteral("Temps : ") + clockText(t.spent), "muted"));
        } else {
            box->addWidget(label(QStringLiteral("Prévu ") + minutesText(t.planned) + QStringLiteral(" · %1 étapes").arg(t.steps.size()), "muted"));
            auto *progress = new QProgressBar;
            progress->setTextVisible(false);
            progress->setRange(0, std::max(1, t.planned));
            progress->setValue(std::min(t.spent, t.planned));
            progress->setObjectName(QStringLiteral("task%1").arg(t.id));
            box->addWidget(progress);
        }
        if (t.state == WorkTask::Done) {
            auto *stars = label(QString(t.rating, QChar(0x2605)) + QString(5 - t.rating, QChar(0x2606)), "section");
            stars->setStyleSheet("color:#f4d27a");
            box->addWidget(stars);
        }
        if (!t.feedback.isEmpty()) {
            auto *feedback = label(QStringLiteral("« ") + t.feedback + QStringLiteral(" »"), "muted");
            feedback->setStyleSheet(t.state == WorkTask::Done ? "color:#7fc08a" : "color:#e3a06d");
            feedback->setWordWrap(true);
            box->addWidget(feedback);
        }
        if (c == 0) {
            auto *start = new QPushButton(t.state == WorkTask::Rework ? QStringLiteral("Corriger") : QStringLiteral("Démarrer"));
            start->setProperty("primary", t.state == WorkTask::Rework);
            const int id = t.id;

            connect(start, &QPushButton::clicked, this, [this, id] { startTask(id); }, Qt::QueuedConnection);
            box->addWidget(start);
        }
        m_columns[c]->addWidget(card(box));
    }
    static const QStringList hints = {QStringLiteral("Plus rien à démarrer"), QStringLiteral("Démarrez une tâche"), QStringLiteral("Aucun dépôt en attente"), QStringLiteral("Vos tâches validées aujourd'hui")};
    for (int i = 0; i < 4; ++i) {
        m_counts[i]->setText(QString::number(counts[i]));
        if (counts[i]) continue;
        auto *hint = label(hints[i], "muted");
        hint->setAlignment(Qt::AlignCenter);
        hint->setMinimumHeight(120);
        m_columns[i]->addWidget(hint);
    }
    const ReviewStats stats = m_store.reviewStats(m_session.name);
    m_quality->setText(stats.rating > 0 ? QStringLiteral("Qualité %1/5 ★").arg(stats.rating, 0, 'f', 1) : QStringLiteral("Pas encore notée"));
}

void EmployeeDesk::tick(const QDateTime &now, int idleSeconds)
{
    if (m_finished) return;
    const int delta = int(m_last.secsTo(now));
    if (delta < 1) {
        if (delta < 0) m_last = now;
        return;
    }
    m_session.lastSeen = now;
    m_last = m_last.addSecs(delta);
    if (delta > kSleepGap) {
        m_session.off += delta;
        track(QStringLiteral("PC hors ligne"), delta);
        const QString text = QStringLiteral("PC en veille ou suspendu pendant ") + clockText(delta);
        addLog(text);
        m_store.log(m_session, text);
    } else if (m_paused) {
        m_session.pause += delta;
        track(kPause, delta);
        if (!m_pauseWarned && m_basePause + m_session.pause > WorkRules::Pause) {
            m_pauseWarned = true;
            addLog(QStringLiteral("Pause dépassée"));
            m_store.log(m_session, QStringLiteral("Pause dépassée"));
            Fx::toast(this, QStringLiteral("Votre heure de pause est écoulée"));
        }
    } else {
        const bool idle = idleSeconds >= kIdleLimit;
        if (idle && m_session.state == kWork) setState(kIdle, QStringLiteral("Inactif (aucune activité clavier/souris)"));
        else if (!idle && m_session.state == kIdle) setState(kWork, QStringLiteral("Retour d'activité"));
        (idle ? m_session.idle : m_session.work) += delta;
        track(idle ? kIdle : kWork, delta);
        if (!idle && m_current.id) m_current.spent += delta;
    }
    if ((m_sinceSave += delta) >= 3) {
        m_sinceSave = 0;
        if (m_current.id) m_store.saveTaskTime(m_current.id, m_current.spent);
        if (!m_store.save(m_session)) m_notice->setText(QStringLiteral("Enregistrement impossible : ") + m_store.error());
    }
    refresh();
}

void EmployeeDesk::setState(const QString &state, const QString &event)
{
    m_session.state = state;
    addLog(event);
    m_store.log(m_session, event);
    m_store.save(m_session);
    refresh();
}

void EmployeeDesk::addLog(const QString &text)
{
    m_log->add(QDateTime::currentDateTime(), text);
    static const QStringList names = {QStringLiteral("Tout"), QStringLiteral("Présence"), QStringLiteral("Pauses"), QStringLiteral("Tâches"), QStringLiteral("Inactivité")};
    for (int i = 0; i < 5; ++i) m_filters[i]->setText(names[i] + QStringLiteral(" · %1").arg(m_log->count(i - 1)));
}

void EmployeeDesk::refresh()
{
    const int work = m_baseWork + m_session.work, pause = m_basePause + m_session.pause;
    const int workLeft = WorkRules::Day - work, pauseLeft = WorkRules::Pause - pause;
    for (TimeCard *t : std::as_const(m_times)) t->setTimes(work, pause, m_paused, m_session.state == kWork);
    m_clock->setText(m_elapsed ? clockText(work) : clockText(std::abs(workLeft)));
    m_clockInfo->setText((m_elapsed ? QStringLiteral("travaillé · pause ") : workLeft < 0 ? QStringLiteral("heures sup. · pause ") : QStringLiteral("restant · pause "))
                         + (m_elapsed ? QStringLiteral("%1 min").arg(pause / 60) : pauseLeft < 0 ? QStringLiteral("dépassée") : minutesText(pauseLeft)));
    m_state->setText(QStringLiteral("●  ") + m_session.state);
    m_state->setStyleSheet("color:" + statusColor(m_session.state).name());
    m_pauseButton->setText(m_paused ? QStringLiteral("Reprendre") : QStringLiteral("Pause"));

    m_taskName->setText(m_session.task);
    m_finishButton->setEnabled(m_current.id > 0 && m_current.ready() && !m_paused);
    m_finishButton->setToolTip(m_paused ? QStringLiteral("Reprenez le travail avant de déposer") : QStringLiteral("Cochez toutes les étapes pour déposer"));
    m_details->setText(m_current.feedback.isEmpty() ? m_current.details : QStringLiteral("À corriger : ") + m_current.feedback);
    m_details->setStyleSheet(m_current.feedback.isEmpty() ? "" : "color:#e3a06d");
    if (m_stepsFor != m_current.id) showSteps();
    const int left = m_current.planned - m_current.spent;
    const qreal ratio = m_current.planned > 0 ? m_current.spent / qreal(m_current.planned) : 0;
    m_taskRing->setValue(ratio, QStringLiteral("%1 %").arg(int(ratio * 100)), QStringLiteral("de la tâche"));
    m_taskInfo->setText(!m_current.id ? QStringLiteral("Choisissez une tâche dans « Mes tâches ».")
                        : left >= 0 ? clockText(m_current.spent) + QStringLiteral(" passées · reste ") + clockText(left)
                                    : clockText(m_current.spent) + QStringLiteral(" passées · dépassement de ") + clockText(-left));
    m_taskInfo->setStyleSheet(left < 0 ? "color:#e08f8f" : "");
    m_dayRing->setValue(work / qreal(WorkRules::Day), QStringLiteral("%1 %").arg(std::min(100, int(work * 100LL / WorkRules::Day))), QStringLiteral("de la journée"));

    int done = 0;
    const WorkTask *next = nullptr;
    for (const WorkTask &t : std::as_const(m_tasks)) {
        done += t.state == WorkTask::Done;
        if (!next && (t.state == WorkTask::Todo || t.state == WorkTask::Rework)) next = &t;
    }
    m_next->setText(next ? next->title : QStringLiteral("Plus de tâche prévue"));
    m_nextInfo->setText(next ? QStringLiteral("Durée prévue : ") + minutesText(next->planned) : QStringLiteral("Tout est déposé ou validé."));
    m_nextButton->setText(next ? QStringLiteral("Passer à cette tâche  ›") : QStringLiteral("Demander une tâche"));
    m_idle->setText(clockText(m_session.idle));
    m_idleToday->setText(clockText(m_session.idle));
    m_done->setText(QStringLiteral("%1 / %2").arg(done).arg(m_tasks.size()));
    if (m_current.id)
        if (auto *bar = findChild<QProgressBar *>(QStringLiteral("task%1").arg(m_current.id))) bar->setValue(std::min(m_current.spent, m_current.planned));
}

TimeCard *EmployeeDesk::timeCard()
{
    auto *t = new TimeCard;
    t->clicked = [this] { flipTimes(); };
    m_times << t;
    return t;
}

void EmployeeDesk::flipTimes()
{
    m_elapsed = !m_elapsed;
    for (TimeCard *t : std::as_const(m_times)) t->setBack(m_elapsed);
    refresh();
}

void EmployeeDesk::track(const QString &state, int seconds)
{
    for (DayTimeline *t : std::as_const(m_timelines)) t->add(state, seconds);
}

bool EmployeeDesk::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_chip && event->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) flipTimes();
    return QMainWindow::eventFilter(watched, event);
}

void EmployeeDesk::finish(const QString &reason)
{
    if (m_finished) return;
    tick(QDateTime::currentDateTime(), systemIdleSeconds());
    m_finished = true;
    if (m_current.id) m_store.saveTaskTime(m_current.id, m_current.spent);
    m_session.logout = m_session.lastSeen = QDateTime::currentDateTime();
    m_session.endReason = reason;
    m_store.save(m_session);
    m_store.log(m_session, QStringLiteral("Déconnexion : ") + reason);
    m_store.addEntry(m_session.name, m_employee.name(), m_session.via, QStringLiteral("DEPART"));
}

void EmployeeDesk::leaveByBadge()
{
    m_logout = true;
    finish(QStringLiteral("Départ par badge"));
    close();
}

void EmployeeDesk::closeEvent(QCloseEvent *event)
{
    if (!m_finished) {
        if (!DialogSkin::ask(this, QStringLiteral("Terminer la journée ?"), QStringLiteral("Travail aujourd'hui : ") + clockText(m_baseWork + m_session.work), QStringLiteral("Terminer"), QStringLiteral("Continuer"))) {
            m_logout = false;
            event->ignore();
            return;
        }
        finish(m_logout ? QStringLiteral("Fin de journée") : QStringLiteral("Application fermée"));
    }
    event->accept();
    if (m_leaving) return;
    m_leaving = true;
    for (QTimer *t : findChildren<QTimer *>()) t->stop();
    emit closed(m_logout);
}
