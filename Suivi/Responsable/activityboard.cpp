#include "activityboard.h"
#include "employeepopup.h"
#include "employeetile.h"
#include "dialogskin.h"
#include "UiAnimations.h"
#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
enum Column { Name, Position, Status, Task, In, Out, Work, Over, Pause, Idle, Off, Percent, Count };
const QStringList kHeaders = {QStringLiteral("Employé"), QStringLiteral("Poste"), QStringLiteral("Statut"),
    QStringLiteral("Tâche en cours"), QStringLiteral("Arrivée"), QStringLiteral("Départ"), QStringLiteral("Travail"),
    QStringLiteral("Heures sup."), QStringLiteral("Pause"), QStringLiteral("Inactif"), QStringLiteral("PC veille"),
    QStringLiteral("Activité")};
const QString kActive = QStringLiteral("Actif"), kPause = QStringLiteral("En pause"), kIdle = QStringLiteral("Inactif"),
              kOffline = QStringLiteral("PC hors ligne"), kDone = QStringLiteral("Terminé"),
              kAbsent = QStringLiteral("Non connecté"), kWatch = QStringLiteral("À surveiller");

QString timeText(const QDateTime &t)
{
    return t.isValid() ? t.toString("HH:mm") : QStringLiteral("—");
}

QLabel *kpi(QHBoxLayout *row, const QString &title, const QString &color)
{
    auto *frame = new QFrame;
    frame->setProperty("card", true);
    auto *box = new QVBoxLayout(frame);
    box->setContentsMargins(18, 12, 18, 12);
    auto *label = new QLabel(title);
    label->setProperty("muted", true);
    auto *value = new QLabel("0");
    value->setProperty("kpi", true);
    value->setStyleSheet("color:" + color);
    box->addWidget(label);
    box->addWidget(value);
    row->addWidget(frame, 1);
    return value;
}
}

ActivityBoard::ActivityBoard(ActivityStore &store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);
    auto *cards = new QHBoxLayout;
    cards->setSpacing(14);
    m_online = kpi(cards, QStringLiteral("Connectés"), "#7fc08a");
    m_paused = kpi(cards, QStringLiteral("En pause"), "#e6c27a");
    m_work = kpi(cards, QStringLiteral("Heures travaillées"), "#ecdcae");
    m_overtime = kpi(cards, QStringLiteral("Heures supplémentaires"), "#e3a06d");
    root->addLayout(cards);

    m_filter = new QComboBox;
    m_filter->addItems({QStringLiteral("Tous les statuts"), kWatch, kActive, kPause, kIdle, kOffline, kDone, kAbsent});
    m_day = new QDateEdit(QDate::currentDate());
    m_day->setCalendarPopup(true);
    m_day->setDisplayFormat("dd/MM/yyyy");
    m_day->setMaximumDate(QDate::currentDate());
    m_alert = new QPushButton;
    m_alert->setProperty("danger", true);
    m_status = new QLabel;
    m_status->setProperty("muted", true);
    m_viewButton = new QPushButton(QStringLiteral("Vue tableau"));
    auto *csv = new QPushButton(QStringLiteral("Exporter Excel"));
    csv->setProperty("primary", true);
    auto *tools = new QHBoxLayout;
    tools->setSpacing(12);
    tools->addWidget(m_filter);
    tools->addWidget(m_day);
    tools->addWidget(m_alert);
    tools->addStretch();
    tools->addWidget(m_status);
    tools->addWidget(m_viewButton);
    tools->addWidget(csv);
    root->addLayout(tools);

    m_table = new QTableWidget(0, Count);
    m_table->setHorizontalHeaderLabels(kHeaders);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(40);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setStretchLastSection(true);
    const int widths[Count] = {134, 110, 138, 150, 66, 66, 86, 96, 86, 86, 86, 100};
    for (int i = 0; i < Count; ++i) {
        m_table->setColumnWidth(i, widths[i]);
    }
    m_table->horizontalHeader()->setSortIndicator(Name, Qt::AscendingOrder);

    auto *holder = new QWidget;
    holder->setObjectName("dashboardContents");
    m_grid = new QGridLayout(holder);
    m_grid->setContentsMargins(0, 0, 6, 0);
    m_grid->setSpacing(12);
    m_grid->setAlignment(Qt::AlignTop);
    m_empty = new QLabel(QStringLiteral("Aucun employé ne correspond à ces critères."));
    m_empty->setProperty("muted", true);
    m_grid->addWidget(m_empty, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(holder);
    m_views = new QStackedWidget;
    m_views->addWidget(scroll);
    m_views->addWidget(m_table);
    root->addWidget(m_views, 1);

    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] {
        refresh();
    });
    connect(m_day, &QDateEdit::dateChanged, this, [this] {
        refresh();
    });
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        openEmployee(m_table->item(row, Name)->data(Qt::UserRole).toString());
    });
    connect(m_viewButton, &QPushButton::clicked, this, [this] {
        Fx::switchPage(m_views, 1 - m_views->currentIndex());
        m_viewButton->setText(m_views->currentIndex() ? QStringLiteral("Vue cartes") : QStringLiteral("Vue tableau"));
    });
    connect(m_alert, &QPushButton::clicked, this, [this] {
        m_filter->setCurrentIndex(m_filter->currentIndex() == 1 ? 0 : 1);
    });
    connect(csv, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Exporter"),
            "activite_" + m_day->date().toString("yyyy-MM-dd") + ".csv", "Excel CSV (*.csv)");
        if (!path.isEmpty() && !exportCsv(path)) {
            DialogSkin::warn(
                this, "AUREA", QStringLiteral("Impossible d'écrire ce fichier (est-il ouvert dans Excel ?)."));
        }
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        const QDate today = QDate::currentDate();
        if (m_day->maximumDate() != today) {
            const bool followed = m_day->date() == m_day->maximumDate();
            m_day->setMaximumDate(today);
            if (followed) {
                m_day->setDate(today);
            }
        }
        refresh();
        notify();
    });
    timer->start(1000);
    auto *pulse = new QTimer(this);
    connect(pulse, &QTimer::timeout, this, [this] {
        if (!isVisible() || m_views->currentIndex() != 0) {
            return;
        }
        m_phase = m_phase > 1 ? 0 : m_phase + 0.012;
        for (EmployeeTile *t : std::as_const(m_tiles)) {
            if (t->isVisible()) {
                t->setPulse(std::min(1.0, m_phase));
            }
        }
    });
    pulse->start(33);
    refresh();
}

QString ActivityBoard::reference(int id) const
{
    for (auto it = m_people.begin(); it != m_people.end(); ++it) {
        if (it->id() == id) {
            return it.key();
        }
    }
    return QString();
}

QString ActivityBoard::displayName(const QString &reference) const
{
    return m_people.contains(reference) ? m_people.value(reference).name() : reference;
}

WorkSession ActivityBoard::session(const QString &reference) const
{
    for (const WorkSession &s : m_rows) {
        if (s.name == reference) {
            return s;
        }
    }
    return WorkSession();
}

void ActivityBoard::setEmployees(const QVector<Employee> &employees)
{
    m_people.clear();
    for (const Employee &e : employees) {
        m_people.insert(e.reference(), e);
    }
    refresh();
}

void ActivityBoard::setSearch(const QString &text)
{
    m_search = text.trimmed();
    refresh();
}

QString ActivityBoard::assignTask(const QString &reference, const QString &title, int minutes)
{
    const int id = employee(reference).id();
    return id && assignHandler ? assignHandler(id, title, minutes) : QStringLiteral("Affectation indisponible.");
}

void ActivityBoard::openEmployee(const QString &reference)
{
    if (!m_people.contains(reference)) {
        return;
    }
    if (!m_popup) {
        m_popup = new EmployeePopup(*this, m_store, window());
    }
    m_popup->openFor(reference);
}

int ActivityBoard::connected()
{
    int n = 0;
    for (const WorkSession &s : sessionsToday()) {
        n += s.status() == kActive || s.status() == kPause || s.status() == kIdle;
    }
    return n;
}

QVector<WorkSession> ActivityBoard::sessionsToday()
{
    return collect(QDate::currentDate());
}

QVector<WorkSession> ActivityBoard::collect(const QDate &day)
{
    QHash<QString, WorkSession> byPerson;
    for (const WorkSession &s : m_store.sessions(day)) {
        if (!m_people.contains(s.name)) {
            continue;
        }
        auto found = byPerson.find(s.name);
        if (found == byPerson.end()) {
            byPerson.insert(s.name, s);
            continue;
        }
        found->work += s.work;
        found->pause += s.pause;
        found->idle += s.idle;
        found->off += s.off;
        found->login = std::min(found->login, s.login);
    }
    QVector<WorkSession> out;
    for (auto it = m_people.begin(); it != m_people.end(); ++it) {
        WorkSession s = byPerson.value(it.key());
        s.name = it.key();
        s.position = it->position();
        if (!s.login.isValid()) {
            s.state = kAbsent;
        }
        if (day == QDate::currentDate()) {
            const QVector<WorkTask> tasks = m_store.tasks(s.name);
            s.tasksTotal = int(tasks.size());
            for (const WorkTask &t : tasks) {
                if (t.state == WorkTask::Current) {
                    s.taskSpent = t.spent;
                    s.taskPlanned = t.planned;
                    s.task = t.title;
                } else if (t.state == WorkTask::Done) {
                    ++s.tasksDone;
                } else if (s.nextTask.isEmpty() && t.state != WorkTask::Submitted) {
                    s.nextTask = t.title;
                }
            }
        }
        out.append(s);
    }
    std::sort(out.begin(), out.end(), [this](const WorkSession &a, const WorkSession &b) {
        const bool x = a.login.isValid() && !a.ended(), y = b.login.isValid() && !b.ended();
        if (x != y) {
            return x;
        }
        const int c = QString::localeAwareCompare(displayName(a.name), displayName(b.name));
        return c ? c < 0 : a.name < b.name;
    });
    return out;
}

void ActivityBoard::refresh()
{
    const bool live = m_day->date() == QDate::currentDate();
    m_rows = collect(m_day->date());
    const QString error = m_store.error();
    int online = 0, paused = 0, work = 0, overtime = 0, watch = 0;
    QVector<WorkSession> rows;
    for (const WorkSession &s : std::as_const(m_rows)) {
        const QString status = s.status();
        const bool alert = !sessionAlerts(s).isEmpty();
        work += s.work;
        overtime += std::max(0, s.work - WorkRules::Day);
        online += status == kActive || status == kPause || status == kIdle;
        paused += status == kPause;
        watch += alert;
        const QString wanted = m_filter->currentText();
        if (m_filter->currentIndex() > 0 && (wanted == kWatch ? !alert : status != wanted)) {
            continue;
        }
        if (!m_search.isEmpty()
            && !(displayName(s.name) + ' ' + s.name + ' ' + s.position + ' ' + s.task)
                .contains(m_search, Qt::CaseInsensitive)) {
            continue;
        }
        rows.append(s);
    }
    Fx::countTo(m_online, online);
    Fx::countTo(m_paused, paused);
    m_work->setText(clockText(work));
    m_overtime->setText(clockText(overtime));
    m_alert->setVisible(watch > 0);
    m_alert->setText(QStringLiteral("▲  %1 à surveiller").arg(watch));
    m_status->setText(!error.isEmpty() ? QStringLiteral("Erreur : ") + error
            : live                     ? QStringLiteral("●  En direct · ") + QTime::currentTime().toString("HH:mm:ss")
                                       : QStringLiteral("Historique"));

    m_table->setSortingEnabled(false);
    m_table->setRowCount(int(rows.size()));
    for (int row = 0; row < rows.size(); ++row) {
        const WorkSession &s = rows[row];
        const QVariantList values = {displayName(s.name), s.position, s.status(),
            s.task.isEmpty() ? QStringLiteral("—") : s.task, timeText(s.login), timeText(s.logout), clockText(s.work),
            clockText(s.work - WorkRules::Day), clockText(s.pause), clockText(s.idle), clockText(s.off),
            s.activePercent()};
        for (int col = 0; col < Count; ++col) {
            QTableWidgetItem *item = m_table->item(row, col);
            if (!item) {
                m_table->setItem(row, col, item = new QTableWidgetItem);
            }
            item->setData(Qt::DisplayRole, values[col]);
        }
        m_table->item(row, Name)->setData(Qt::UserRole, s.name);
        m_table->item(row, Out)->setToolTip(s.endReason);
    }
    m_table->setSortingEnabled(true);
    refreshTiles(rows);
    if (m_popup) {
        m_popup->refresh();
    }
}

void ActivityBoard::refreshTiles(const QVector<WorkSession> &rows)
{
    QStringList order;
    for (const WorkSession &s : rows) {
        const QString key = s.name;
        order << key;
        EmployeeTile *&tile = m_tiles[key];
        if (!tile) {
            tile = new EmployeeTile;
            tile->enter(int(order.size()) * 18);
            tile->clicked = [this, key] {
                openEmployee(key);
            };
        }
        WorkSession shown = s;
        shown.name = displayName(s.name);
        tile->setSession(shown, m_popup && m_popup->reference() == key);
    }
    if (order == m_order) {
        return;
    }
    m_order = order;
    m_columns = 0;
    layoutTiles();
}

void ActivityBoard::layoutTiles()
{
    const int columns = std::max(1, (m_views->width() - 10) / 264);
    if (columns == m_columns) {
        return;
    }
    for (int c = 0; c < std::max(columns, m_grid->columnCount()); ++c) {
        m_grid->setColumnStretch(c, c < columns ? 1 : 0);
    }
    m_columns = columns;
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        m_grid->removeWidget(it.value());
        if (!m_order.contains(it.key())) {
            it.value()->hide();
            it.value()->deleteLater();
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }
    for (int i = 0; i < m_order.size(); ++i) {
        EmployeeTile *tile = m_tiles[m_order[i]];
        m_grid->addWidget(tile, i / columns, i % columns);
        tile->show();
    }
    m_empty->setVisible(m_order.isEmpty());
}

void ActivityBoard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, [this] {
        layoutTiles();
    });
}

void ActivityBoard::notify()
{
    const QVector<ActivityEvent> events = m_store.events(QDate::currentDate());
    for (const ActivityEvent &e : events) {
        if (!m_people.contains(e.who) || !e.text.startsWith(QStringLiteral("Dépôt"))) {
            continue;
        }
        const QString latest = e.at.toString(Qt::ISODate) + e.who + e.text;
        if (!m_lastEvent.isEmpty() && latest != m_lastEvent && window()->isVisible()) {
            Fx::toast(window(), displayName(e.who) + QStringLiteral(" — ") + e.text);
        }
        m_lastEvent = latest;
        return;
    }
    m_lastEvent = "-";
}

QVector<WorkTask> ActivityBoard::reviews()
{
    QVector<WorkTask> out;
    for (const WorkTask &t : m_store.pendingReviews()) {
        if (m_people.contains(t.who)) {
            out.append(t);
        }
    }
    return out;
}

bool ActivityBoard::review(const WorkTask &task, bool accept, int rating, const QString &feedback)
{
    if (!m_store.review(task.id, accept, rating, feedback)) {
        return false;
    }
    WorkSession who;
    who.name = task.who;
    m_store.log(who,
        (accept ? QStringLiteral("Dépôt validé (%1/5) : ").arg(rating) : QStringLiteral("Correction demandée : "))
            + task.title);
    refresh();
    return true;
}

bool ActivityBoard::exportCsv(const QString &path) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto line = [](const QStringList &values) {
        QStringList out;
        for (QString v : values) {
            if (!v.isEmpty() && QStringLiteral("=+-@").contains(v[0])) {
                v.prepend('\'');
            }
            out << '"' + v.replace('"', "\"\"") + '"';
        }
        return (out.join(';') + "\r\n").toUtf8();
    };
    file.write("\xEF\xBB\xBF");
    file.write(line(kHeaders));
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList values;
        for (int col = 0; col < Count; ++col) {
            values << m_table->item(row, col)->text() + (col == Percent ? " %" : "");
        }
        file.write(line(values));
    }
    return file.commit();
}
