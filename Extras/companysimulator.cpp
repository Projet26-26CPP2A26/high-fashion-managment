#include "companysimulator.h"
#include "TransitionAnimation.h"
#include "taskcatalog.h"
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

namespace {
const QString kWork = QStringLiteral("Actif"), kPause = QStringLiteral("En pause"), kIdle = QStringLiteral("Inactif"), kOff = QStringLiteral("PC coupé");
const int kStepSeconds = 2;

int reservedId(const QVector<Employee> &list)
{
    int id = 0;
    for (const Employee &e : list) if (!id || e.id() < id) id = e.id();
    return id;
}
QString eventText(const QString &from, const QString &to)
{
    if (to == kPause) return QStringLiteral("Début de pause");
    if (to == kIdle) return QStringLiteral("Inactif (aucune activité clavier/souris)");
    if (to == kOff) return QStringLiteral("Signal du PC perdu");
    return from == kPause ? QStringLiteral("Reprise du travail") : from == kOff ? QStringLiteral("PC de nouveau en ligne") : QStringLiteral("Retour d'activité");
}
}

CompanySimulator::CompanySimulator(ActivityStore &store, const QVector<Employee> &employees, QObject *parent, const QString &lockPath)
    : QObject(parent), m_store(store), m_employees(employees)
{
    m_lock.reset(new QLockFile(lockPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/demo.lock" : lockPath));
    if (!m_lock->tryLock(0)) return;
    m_store.begin();
    seedHistory(QDate::currentDate());
    m_store.commit();
    step(QDateTime::currentDateTime());
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { step(QDateTime::currentDateTime()); });
    timer->start(kStepSeconds * 1000);
}

CompanySimulator::Plan CompanySimulator::plan(const Employee &e, const QDate &day)
{
    QRandomGenerator rng(quint32(e.id() * 7919 + day.toJulianDay() * 104729));
    const auto pick = [&](int low, int high) { return low + int(rng.bounded(high - low + 1)); };
    Plan p;

    const bool late = e.id() % 2 == 1;
    p.arrival = QDateTime(day, QTime(late ? 12 : 7, 50)).addSecs(pick(0, 35 * 60));
    const bool overtime = rng.bounded(4) == 0, longPause = rng.bounded(9) == 0, cut = rng.bounded(8) == 0;
    const int total = WorkRules::Day + (overtime ? pick(20, 70) * 60 : pick(-4, 6) * 60);
    const int lunch = longPause ? pick(52, 58) * 60 : pick(40, 46) * 60, coffee = pick(9, 13) * 60;
    const int first = pick(95, 125) * 60, second = pick(60, 90) * 60, third = pick(90, 120) * 60;
    p.parts = {{kWork, first}, {kIdle, pick(2, 6) * 60}, {kWork, second}, {kPause, lunch}, {kWork, third}, {kPause, coffee}};
    int left = total - first - second - third;
    if (cut) {
        const int before = pick(20, 50) * 60;
        p.parts += {Part{kWork, before}, Part{kOff, pick(3, 7) * 60}};
        left -= before;
    }
    const int half = left / 2;
    p.parts += {Part{kWork, half}, Part{kIdle, pick(2, 5) * 60}, Part{kWork, left - half}};
    return p;
}

WorkSession CompanySimulator::at(const Plan &p, const QDateTime &now)
{
    WorkSession s;
    s.login = p.arrival;
    s.lastSeen = now;
    qint64 left = p.arrival.secsTo(now);
    for (const Part &part : p.parts) {
        const int used = int(std::min<qint64>(left, part.seconds));
        (part.state == kWork ? s.work : part.state == kPause ? s.pause : part.state == kIdle ? s.idle : s.off) += used;
        s.state = part.state;
        left -= used;
        if (used < part.seconds) {
            if (part.state == kOff) s.lastSeen = now.addSecs(-used);
            return s;
        }
    }
    s.logout = s.lastSeen = now.addSecs(-left);
    s.endReason = QStringLiteral("Fin de journée");
    return s;
}

void CompanySimulator::seedHistory(const QDate &today)
{
    const int reserved = reservedId(m_employees);
    for (const Employee &e : std::as_const(m_employees)) {
        MorphOverlay::breathe();
        if (!e.available()) continue;
        if (e.id() != reserved && m_store.hasSessionOn(e.reference(), today)) continue;
        for (int back = 10; back >= 1; --back) {
            const QDate day = today.addDays(-back);
            if (day.dayOfWeek() == 7 || day < e.hired() || m_store.hasSessionOn(e.reference(), day)) continue;
            const Plan p = plan(e, day);
            WorkSession s = at(p, QDateTime(day, QTime(23, 59)));
            s.name = e.reference();
            s.position = e.position();
            s.task = TaskCatalog::forSpecialty(e.specialty()).first().title;
            m_store.insertSession(s);
            m_store.saveAttendance({e.id(), day, QTime(s.login.time().hour(), s.login.time().minute()), QTime(s.logout.time().hour(), s.logout.time().minute()), s.pause / 60});
        }
    }
}

void CompanySimulator::fillTasks(const Employee &e)
{
    int open = 0;
    QStringList titles;
    for (const WorkTask &t : m_store.tasks(e.reference())) {
        open += t.state == WorkTask::Todo || t.state == WorkTask::Current || t.state == WorkTask::Rework;
        titles << t.title;
    }
    if (open >= 2) return;
    const QVector<TaskModel> models = TaskCatalog::forSpecialty(e.specialty());
    for (int i = 0; i < models.size() && open < 3; ++i) {
        const TaskModel &m = models[(i + e.id()) % models.size()];
        if (titles.contains(m.title) && open > 0) continue;
        WorkTask t;
        t.who = e.reference();
        t.title = m.title;
        t.details = m.details;
        t.steps = m.steps;
        t.planned = m.minutes * 60;
        t.specialty = e.specialty();
        m_store.addTask(t);
        ++open;
    }
}

void CompanySimulator::work(const Employee &e, WorkSession &s, int seconds)
{
    const QString who = e.reference();
    WorkTask current;
    const QVector<WorkTask> list = m_store.tasks(who);
    for (const WorkTask &t : list) if (t.state == WorkTask::Current) current = t;
    if (!current.id) {
        for (const WorkTask &t : list) {
            if (t.state != WorkTask::Todo && t.state != WorkTask::Rework) continue;
            if (m_store.setTaskState(who, t.id, WorkTask::Current)) {
                current = t;
                s.task = t.title;

                if (t.spent == 0 && m_last.value(who).login.isNull()) current.spent = int(QRandomGenerator::global()->bounded(std::max(1, t.planned * 8 / 10)));
                m_store.log(s, QStringLiteral("Tâche démarrée : ") + t.title);
            }
            break;
        }
    }
    if (!current.id) { s.task.clear(); return; }
    s.task = current.title;
    current.spent += seconds;
    m_store.saveTaskTime(current.id, current.spent);

    const int real = current.planned * (85 + current.id * 7 % 20) / 100;
    int mask = 0;
    for (int i = 0; i < current.steps.size(); ++i) if (current.spent >= qint64(real) * (i + 1) / (current.steps.size() + 0.2)) mask |= 1 << i;
    if ((mask | current.stepsDone) != current.stepsDone) m_store.setSteps(current.id, mask | current.stepsDone);
    if (current.spent < real) return;
    m_store.setSteps(current.id, (1 << current.steps.size()) - 1);
    if (!m_store.submit(current.id, QStringLiteral("Travail terminé et contrôlé, prêt pour validation."), QString())) return;
    m_store.log(s, QStringLiteral("Dépôt pour revue : ") + current.title);
    if (m_store.pendingReviews().size() > 5)
        m_store.review(current.id, true, 4 + current.id % 2, QStringLiteral("Validé par le chef d'atelier."));
}

void CompanySimulator::step(const QDateTime &now)
{
    const int elapsed = m_previous.isValid() ? int(std::clamp<qint64>(m_previous.secsTo(now), 0, 30)) : 0;
    m_previous = now;
    const int reserved = reservedId(m_employees);
    m_store.begin();
    for (const Employee &e : std::as_const(m_employees)) {
        MorphOverlay::breathe();
        if (!e.available() || e.id() == reserved) continue;
        const QString who = e.reference();
        const Plan p = plan(e, now.date());
        if (now < p.arrival) continue;
        WorkSession s = at(p, now);
        WorkSession before = m_last.value(who);
        s.name = who;
        s.position = e.position();
        s.task = before.task;
        const bool sameDay = before.id > 0 && before.login.date() == now.date();
        s.id = sameDay ? before.id : m_store.sessionOn(who, now.date());
        if (!s.id) {
            s.id = m_store.insertSession(s);
            QDateTime t = p.arrival;
            m_store.logAt(who, QStringLiteral("Connexion"), t);
            QString from;
            for (const Part &part : p.parts) {
                if (t >= now) break;
                if (!from.isEmpty()) m_store.logAt(who, eventText(from, part.state), t);
                from = part.state;
                t = t.addSecs(part.seconds);
            }
            if (s.ended()) m_store.logAt(who, QStringLiteral("Déconnexion : Fin de journée"), s.logout);
        } else if (sameDay && !before.ended()) {
            if (s.ended()) m_store.logAt(who, QStringLiteral("Déconnexion : Fin de journée"), s.logout);
            else if (s.state != before.state) m_store.logAt(who, eventText(before.state, s.state), now);
        }
        if (!s.ended()) {
            fillTasks(e);
            if (s.state == kWork) work(e, s, elapsed);
            else for (const WorkTask &t : m_store.tasks(who)) if (t.state == WorkTask::Current) s.task = t.title;
        }
        if (!(sameDay && before.ended())) m_store.save(s);
        m_last.insert(who, s);
    }
    m_store.commit();
}
