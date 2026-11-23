#include "companylink.h"
#include "activityboard.h"
#include "activitystore.h"
#include "employeerepository.h"
#include "morphoverlay.h"
#include "taskcatalog.h"
#include <QTimer>
#include <algorithm>

namespace {
QString keyOf(int employeeId, const QDate &day) { return QStringLiteral("%1|%2").arg(employeeId).arg(day.toString(Qt::ISODate)); }
QTime minute(const QDateTime &t) { return QTime(t.time().hour(), t.time().minute()); }
int hoursOf(int seconds) { return std::clamp((seconds + 3599) / 3600, 1, 80); }
}

CompanyLink::CompanyLink(EmployeeRepository &repository, ActivityStore &store, ActivityBoard &board, QObject *parent)
    : QObject(parent), m_repository(repository), m_store(store), m_board(board)
{
    loadAttendance();
    sync();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CompanyLink::sync);
    timer->start(2000);
}

void CompanyLink::loadAttendance()
{
    for (const StoredAttendance &a : m_store.attendance()) {
        MorphOverlay::breathe();
        if (a.departure.isValid()) m_repository.saveAttendance(a.employeeId, a.day, a.arrival, a.departure, a.pause);
        else if (a.day == QDate::currentDate()) m_repository.clockIn(a.employeeId, a.arrival);
        m_saved.insert(keyOf(a.employeeId, a.day), QString());
    }
}

void CompanyLink::sync()
{
    const QVector<Employee> employees = m_repository.loadEmployees();
    if (!m_repository.error().isEmpty()) return;
    m_board.setEmployees(employees);
    const bool attendance = syncAttendance(), tasks = syncTasks();
    if (attendance || tasks) emit changed();
}

bool CompanyLink::syncAttendance()
{
    bool touched = false;
    const QDate today = QDate::currentDate();

    for (const WorkSession &s : m_board.sessionsToday()) {
        MorphOverlay::breathe();
        const int id = m_board.employee(s.name).id();
        if (!id || !s.login.isValid() || s.login.date() != today) continue;
        const Attendance *found = nullptr;
        const QVector<Attendance> list = m_repository.attendance();
        for (const Attendance &a : list) if (a.employeeId() == id && a.day() == today) found = &a;
        if (!found) touched |= m_repository.clockIn(id, minute(s.login));
        if (s.ended()) {
            const QTime arrival = found ? found->arrival() : minute(s.login), departure = minute(s.logout);
            const int presence = arrival.secsTo(departure) / 60, pause = std::min(240, s.pause / 60);
            if (presence > pause && (!found || found->departure() != departure) && s.logout > m_store.manualAt(id, today))
                touched |= m_repository.saveAttendance(id, today, arrival, departure, pause);
        }
    }

    QSet<QString> present;
    for (const Attendance &a : m_repository.attendance()) {
        if (!a.employeeId()) continue;
        const QString key = keyOf(a.employeeId(), a.day());
        const QString value = a.arrival().toString("HH:mm") + a.departure().toString("HH:mm") + QString::number(a.pause());
        present.insert(key);
        if (m_saved.value(key) == value) continue;
        if (m_store.saveAttendance({a.employeeId(), a.day(), a.arrival(), a.departure(), a.pause()})) m_saved.insert(key, value);
    }
    for (const QString &key : m_saved.keys()) {
        if (present.contains(key)) continue;
        const QDate day = QDate::fromString(key.section('|', 1), Qt::ISODate);
        if (m_store.removeAttendance(key.section('|', 0, 0).toInt(), day)) m_saved.remove(key);
    }
    return touched;
}

bool CompanyLink::syncTasks()
{
    bool touched = false;
    const QVector<WorkTask> tasks = m_store.openTasks();
    QSet<int> open;
    for (const WorkTask &t : tasks) {
        MorphOverlay::breathe();
        open.insert(t.id);
        if (m_knownTasks.contains(t.id)) continue;
        m_knownTasks.insert(t.id);
        const int id = m_board.employee(t.who).id();
        if (!id) continue;
        if (m_repository.assign(id, t.title, t.specialty.isEmpty() ? m_board.employee(t.who).specialty() : t.specialty, hoursOf(t.planned))) {
            m_taskOf.insert(m_repository.assignments().first().id(), t.id);
            touched = true;
        }
    }
    const QVector<Assignment> list = m_repository.assignments();
    for (const Assignment &a : list) {
        if (!a.employeeId()) continue;
        if (!m_taskOf.contains(a.id())) {
            if (a.completed()) continue;
            WorkTask t;
            t.who = m_board.reference(a.employeeId());
            t.title = a.title();
            t.specialty = a.specialty();
            t.planned = a.hours() * 3600;
            t.details = QStringLiteral("Affectée par le responsable RH.");
            for (const TaskModel &m : TaskCatalog::forSpecialty(t.specialty)) if (m.title == t.title) { t.details = m.details; t.steps = m.steps; }
            const int taskId = m_store.addTask(t);
            if (!taskId) continue;
            m_taskOf.insert(a.id(), taskId);
            m_knownTasks.insert(taskId);
            open.insert(taskId);
            WorkSession who;
            who.name = t.who;
            m_store.log(who, QStringLiteral("Tâche affectée par le responsable RH : ") + t.title);
            touched = true;
        }
        const int taskId = m_taskOf.value(a.id());
        if (a.completed()) { if (open.contains(taskId)) m_store.completeTask(taskId); }
        else if (!open.contains(taskId)) touched |= m_repository.completeAssignment(a.id());
    }
    return touched;
}

QString CompanyLink::assignTask(int employeeId, const QString &title, int minutes)
{
    const Employee e = m_repository.findEmployee(employeeId);
    if (!m_repository.assign(employeeId, title, e.specialty(), hoursOf(minutes * 60))) return m_repository.error();

    WorkTask t;
    t.who = e.reference();
    t.title = title;
    t.specialty = e.specialty();
    t.planned = minutes * 60;
    t.details = QStringLiteral("Affectée par le responsable RH.");
    for (const TaskModel &m : TaskCatalog::forSpecialty(t.specialty)) if (m.title == title) { t.details = m.details; t.steps = m.steps; }
    const int assignment = m_repository.assignments().first().id();
    const int taskId = m_store.addTask(t);
    if (!taskId) {
        m_repository.completeAssignment(assignment);
        return m_store.error();
    }
    m_taskOf.insert(assignment, taskId);
    m_knownTasks.insert(taskId);
    WorkSession who;
    who.name = t.who;
    m_store.log(who, QStringLiteral("Tâche affectée par le responsable RH : ") + title);
    emit changed();
    return QString();
}
