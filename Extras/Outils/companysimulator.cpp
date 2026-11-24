#include "companysimulator.h"
#include "TransitionAnimation.h"
#include "taskcatalog.h"
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

namespace {
const QString kWork = QStringLiteral("Actif"), kPause = QStringLiteral("En pause"), kIdle = QStringLiteral("Inactif"),
              kOff = QStringLiteral("PC coupé");
const int kStepSeconds = 2;

int reservedId(const QVector<Employee> &list)
{
    int id = 0;
    for (const Employee &e : list) {
        if (!id || e.id() < id) {
            id = e.id();
        }
    }
    return id;
}
QString eventText(const QString &from, const QString &to)
{
    if (to == kPause) {
        return QStringLiteral("Début de pause");
    }
    if (to == kIdle) {
        return QStringLiteral("Inactif (aucune activité clavier/souris)");
    }
    if (to == kOff) {
        return QStringLiteral("Signal du PC perdu");
    }
    if (from == kPause) {
        return QStringLiteral("Reprise du travail");
    }
    if (from == kOff) {
        return QStringLiteral("PC de nouveau en ligne");
    }
    return QStringLiteral("Retour d'activité");
}
}

CompanySimulator::CompanySimulator(ActivityStore &store, const QVector<Employee> &employees, QObject *parent,
    const QString &lockPath, bool demo, const QHash<int, bool> &badges)
    : QObject(parent)
    , m_store(store)
    , m_employees(employees)
    , m_badges(badges)
    , m_demo(demo)
{
    m_lock.reset(new QLockFile(lockPath.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/demo.lock"
            : lockPath));
    if (!m_lock->tryLock(0)) {
        return;
    }
    m_reserved = reservedId(m_employees);
    const QDateTime now = QDateTime::currentDateTime();
    m_store.begin();
    for (const Employee &e : std::as_const(m_employees)) {
        remember(e.reference(), 0, now.date());
    }
    if (m_demo) {
        seedHistory(now.date());
    }
    adopt(now);
    if (m_demo) {
        populate(now);
    }
    m_store.commit();
    m_next = now.addSecs(6);
    step(now);
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        step(QDateTime::currentDateTime());
    });
    timer->start(kStepSeconds * 1000);
}

bool CompanySimulator::simulated(const Employee &e) const
{
    return m_demo && e.available() && e.id() != m_reserved && !m_badges.contains(e.id());
}

void CompanySimulator::remember(const QString &who, int session, const QDate &day)
{
    QPair<int, int> total(0, 0);
    for (const WorkSession &s : m_store.sessions(day)) {
        if (s.name != who || s.id == session || s.login.date() != day) {
            continue;
        }
        total.first += s.work;
        total.second += s.pause;
    }
    m_base.insert(who, total);
}

int CompanySimulator::dayWork(const QString &who) const
{
    return m_base.value(who).first + m_last.value(who).work;
}
int CompanySimulator::dayPause(const QString &who) const
{
    return m_base.value(who).second + m_last.value(who).pause;
}

QString CompanySimulator::viaOf(const Employee &e)
{
    return e.id() % 3 == 0 ? QStringLiteral("LIGNE") : QStringLiteral("BADGE");
}

void CompanySimulator::adopt(const QDateTime &now)
{
    QHash<QString, WorkSession> latest;
    for (const WorkSession &s : m_store.sessions(now.date())) {
        if (!latest.contains(s.name)) {
            latest.insert(s.name, s);
        }
    }
    for (const Employee &e : std::as_const(m_employees)) {
        MorphOverlay::breathe();
        const bool real = m_badges.contains(e.id());
        if (!simulated(e) && !real) {
            continue;
        }
        WorkSession s = latest.value(e.reference());
        if (real && e.id() != m_reserved && m_badges.value(e.id()) && (!s.id || s.ended())) {
            arrive(e, now, now, QStringLiteral("BADGE"), false);
            continue;
        }
        if (simulated(e) && s.id && s.login.date() == now.date() && s.lastSeen.secsTo(now) > 4 * 3600) {
            m_store.clearDay(e.reference(), now.date());
            m_store.removeAttendance(e.id(), now.date());
            continue;
        }
        if (!s.id || s.ended()) {
            continue;
        }
        const qint64 gap = std::max<qint64>(0, s.lastSeen.secsTo(now));
        if (gap < 25 && real) {
            continue;
        }
        if (gap > 4 * 3600 || s.login.date() != now.date() || (real && !m_badges.value(e.id()))) {
            s.logout = s.lastSeen;
            s.endReason = QStringLiteral("Fin de journée");
            m_store.save(s);
            continue;
        }
        remember(e.reference(), s.id, now.date());
        const auto base = m_base.value(e.reference());
        const int rest = s.state == kPause
            ? int(std::min<qint64>(gap, std::clamp(WorkRules::Pause - base.second - s.pause, 0, 600)))
            : 0;
        s.pause += rest;
        s.work +=
            real ? int(gap) - rest : std::clamp(int(gap) - rest, 0, std::max(0, WorkRules::Day - base.first - s.work));
        s.state = kWork;
        s.lastSeen = now;
        if (s.via.isEmpty()) {
            s.via = real ? QStringLiteral("BADGE") : viaOf(e);
        }
        m_store.save(s);
        m_last.insert(e.reference(), s);
        m_until.insert(e.reference(), now.addSecs(QRandomGenerator::global()->bounded(120, 900)));
    }
}

void CompanySimulator::populate(const QDateTime &now)
{
    auto *rng = QRandomGenerator::global();
    QVector<Employee> absent;
    int total = 0;
    for (const Employee &e : std::as_const(m_employees)) {
        if (!simulated(e)) {
            continue;
        }
        ++total;
        if (!m_last.contains(e.reference())) {
            absent.append(e);
        }
    }
    const int wanted = total * 7 / 10 - int(m_last.size());
    const QDateTime open(now.date(), QTime(0, 1));
    const int span = std::max(5, int(std::min<qint64>(165, open.secsTo(now) / 60 * 9 / 10)));
    QVector<QPair<int, Employee>> first;
    for (int i = 0; i < wanted && !absent.isEmpty(); ++i) {
        first.append(qMakePair(int(rng->bounded(3, span)), absent.takeAt(rng->bounded(absent.size()))));
    }
    std::sort(first.begin(), first.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });
    for (const auto &p : std::as_const(first)) {
        arrive(p.second, std::max(open, now.addSecs(-p.first * 60 - int(rng->bounded(60)))), now, viaOf(p.second));
    }
    if (first.size() > 4) {
        change(first[1].second, kPause, now.addSecs(-95));
    }
}

void CompanySimulator::arrive(
    const Employee &e, const QDateTime &at, const QDateTime &now, const QString &via, bool entry)
{
    auto *rng = QRandomGenerator::global();
    WorkSession s;
    s.name = e.reference();
    s.position = e.position();
    s.via = via;
    s.state = kWork;
    s.login = at;
    s.lastSeen = now;
    const int since = int(std::max<qint64>(0, at.secsTo(now)));
    s.pause = since > 5400 ? int(rng->bounded(480, 1500)) : 0;
    s.idle = since > 1800 ? int(rng->bounded(60, 300)) : 0;
    s.work = std::max(0, since - s.pause - s.idle);
    s.id = m_store.insertSession(s);
    if (!s.id) {
        return;
    }
    remember(s.name, s.id, now.date());
    m_store.logAt(s.name, QStringLiteral("Connexion"), at);
    if (entry) {
        m_store.addEntry(s.name, e.name(), via, QStringLiteral("ARRIVEE"), QString(), at);
    }
    m_last.insert(s.name, s);
    m_until.insert(s.name, now.addSecs(rng->bounded(150, 1200)));
}

void CompanySimulator::change(const Employee &e, const QString &state, const QDateTime &now)
{
    WorkSession &s = m_last[e.reference()];
    if (!s.id || s.state == state) {
        return;
    }
    auto *rng = QRandomGenerator::global();
    m_store.logAt(s.name, eventText(s.state, state), now);
    if (s.via == "BADGE" && (state == kPause || s.state == kPause)) {
        m_store.addEntry(s.name, e.name(), s.via, state == kPause ? QStringLiteral("PAUSE") : QStringLiteral("RETOUR"),
            QString(), now);
    }
    s.state = state;
    m_until.insert(s.name,
        now.addSecs(state == kPause
                ? std::clamp(int(rng->bounded(150, 480)), 1, std::max(1, WorkRules::Pause - dayPause(s.name)))
                : state == kIdle ? rng->bounded(60, 150)
                                 : rng->bounded(240, 1500)));
}

void CompanySimulator::leave(const Employee &e, const QDateTime &now, const QString &reason, bool entry)
{
    WorkSession s = m_last.take(e.reference());
    if (!s.id) {
        return;
    }
    m_base[s.name].first += s.work;
    m_base[s.name].second += s.pause;
    s.logout = s.lastSeen = now;
    s.endReason = reason;
    m_store.save(s);
    m_store.logAt(s.name, QStringLiteral("Déconnexion : ") + reason, now);
    if (entry) {
        m_store.addEntry(s.name, e.name(), s.via, QStringLiteral("DEPART"), QString(), now);
    }
    m_until.insert(s.name, now.addSecs(QRandomGenerator::global()->bounded(180, 600)));
}

void CompanySimulator::badge(int employeeId, bool arrival)
{
    if (!m_lock->isLocked()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    for (const Employee &e : std::as_const(m_employees)) {
        if (e.id() != employeeId) {
            continue;
        }
        m_badges.insert(employeeId, arrival);
        if (arrival && !m_last.contains(e.reference())) {
            arrive(e, now, now, QStringLiteral("BADGE"), false);
        } else if (!arrival) {
            leave(e, now, QStringLiteral("Départ par badge"), false);
        }
        m_store.addEntry(e.reference(), e.name(), QStringLiteral("BADGE"),
            arrival ? QStringLiteral("ARRIVEE") : QStringLiteral("DEPART"));
    }
}

void CompanySimulator::traffic(const QDateTime &now)
{
    auto *rng = QRandomGenerator::global();
    QVector<Employee> absent, active, paused, idle;
    for (const Employee &e : std::as_const(m_employees)) {
        if (!simulated(e)) {
            continue;
        }
        const QString who = e.reference();
        if (!m_last.contains(who)) {
            if (m_until.value(who) <= now && m_base.value(who).first < WorkRules::Day - 1800) {
                absent.append(e);
            }
            continue;
        }
        const QString state = m_last[who].state;
        (state == kPause ? paused : state == kIdle ? idle : active).append(e);
    }
    for (const Employee &e : std::as_const(paused)) {
        if (m_until.value(e.reference()) <= now) {
            return change(e, kWork, now);
        }
    }
    for (const Employee &e : std::as_const(idle)) {
        if (m_until.value(e.reference()) <= now) {
            return change(e, kWork, now);
        }
    }
    for (const Employee &e : std::as_const(active)) {
        if (dayWork(e.reference()) >= WorkRules::Day) {
            return leave(e, now, QStringLiteral("Fin de journée"));
        }
    }
    const int inside = int(active.size() + paused.size() + idle.size()), total = inside + int(absent.size());
    const double full = total ? inside / double(total) : 1;
    if (full >= 0.7 && m_refusal.isValid() && m_refusal.secsTo(now) > 240 && rng->bounded(5) == 0) {
        m_refusal = now;
        const bool office = rng->bounded(2);
        m_store.addEntry(QString(), office ? QStringLiteral("Badge inconnu") : QStringLiteral("Identifiant refusé"),
            office ? QStringLiteral("BADGE") : QStringLiteral("LIGNE"), QStringLiteral("REFUSE"),
            office ? QString::number(quint32(rng->generate()), 16).toUpper().rightJustified(8, '0')
                   : QStringLiteral("stagiaire"));
        return;
    }
    if (!m_refusal.isValid()) {
        m_refusal = now.addSecs(-120);
    }
    const int roll = rng->bounded(100);
    const bool canArrive = !absent.isEmpty(), canPause = !active.isEmpty() && paused.size() < 3;
    if (canArrive && (roll < (full < 0.7 ? 75 : full < 0.9 ? 35 : 10) || !canPause)) {
        const Employee e = absent[rng->bounded(absent.size())];
        return arrive(e, now, now, viaOf(e));
    }
    if (canPause && (roll < 80 || full <= 0.75)) {
        for (int i = 0; i < 6; ++i) {
            const Employee e = active[rng->bounded(active.size())];
            if (m_last[e.reference()].login.secsTo(now) > 240
                && (roll % 4 == 0 || dayPause(e.reference()) < WorkRules::Pause - 240)) {
                return change(e, roll % 4 == 0 ? kIdle : kPause, now);
            }
        }
    }
    if (full > 0.75 && active.size() > 3) {
        Employee oldest = active.first();
        for (const Employee &e : std::as_const(active)) {
            if (m_last[e.reference()].work > m_last[oldest.reference()].work) {
                oldest = e;
            }
        }
        if (m_last[oldest.reference()].work > 5400) {
            leave(oldest, now, QStringLiteral("Rendez-vous extérieur"));
        }
    }
}

CompanySimulator::Plan CompanySimulator::plan(const Employee &e, const QDate &day)
{
    QRandomGenerator rng(quint32(e.id() * 7919 + day.toJulianDay() * 104729));
    const auto pick = [&](int low, int high) {
        return low + int(rng.bounded(high - low + 1));
    };
    Plan p;

    const bool late = e.id() % 2 == 1;
    p.arrival = QDateTime(day, QTime(late ? 12 : 7, 50)).addSecs(pick(0, 35 * 60));
    const bool cut = rng.bounded(8) == 0;
    const int total = WorkRules::Day - pick(0, 6) * 60;
    const int lunch = pick(40, 46) * 60, coffee = pick(9, 13) * 60;
    const int first = pick(95, 125) * 60, second = pick(60, 90) * 60, third = pick(90, 120) * 60;
    p.parts = {
        {kWork, first}, {kIdle, pick(2, 6) * 60}, {kWork, second}, {kPause, lunch}, {kWork, third}, {kPause, coffee}};
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
            if (part.state == kOff) {
                s.lastSeen = now.addSecs(-used);
            }
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
        if (!e.available()) {
            continue;
        }
        if (e.id() != reserved && m_store.hasSessionOn(e.reference(), today)) {
            continue;
        }
        for (int back = 10; back >= 1; --back) {
            const QDate day = today.addDays(-back);
            if (day.dayOfWeek() == 7 || day < e.hired() || m_store.hasSessionOn(e.reference(), day)) {
                continue;
            }
            const Plan p = plan(e, day);
            WorkSession s = at(p, QDateTime(day, QTime(23, 59)));
            s.name = e.reference();
            s.position = e.position();
            s.task = TaskCatalog::forSpecialty(e.specialty()).first().title;
            m_store.insertSession(s);
            m_store.saveAttendance({e.id(), day, QTime(s.login.time().hour(), s.login.time().minute()),
                QTime(s.logout.time().hour(), s.logout.time().minute()), s.pause / 60});
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
    if (open >= 2) {
        return;
    }
    const QVector<TaskModel> models = TaskCatalog::forSpecialty(e.specialty());
    for (int i = 0; i < models.size() && open < 3; ++i) {
        const TaskModel &m = models[(i + e.id()) % models.size()];
        if (titles.contains(m.title) && open > 0) {
            continue;
        }
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
    const bool first = s.task.isEmpty();
    WorkTask current;
    const QVector<WorkTask> list = m_store.tasks(who);
    for (const WorkTask &t : list) {
        if (t.state == WorkTask::Current) {
            current = t;
        }
    }
    if (!current.id) {
        for (const WorkTask &t : list) {
            if (t.state != WorkTask::Todo && t.state != WorkTask::Rework) {
                continue;
            }
            if (m_store.setTaskState(who, t.id, WorkTask::Current)) {
                current = t;
                s.task = t.title;

                if (t.spent == 0 && first && s.login.secsTo(s.lastSeen) > 300) {
                    current.spent = int(QRandomGenerator::global()->bounded(std::max(1, t.planned * 8 / 10)));
                }
                m_store.log(s, QStringLiteral("Tâche démarrée : ") + t.title);
            }
            break;
        }
    }
    if (!current.id) {
        s.task.clear();
        return;
    }
    s.task = current.title;
    current.spent += seconds;
    m_store.saveTaskTime(current.id, current.spent);

    const int real = current.planned * (85 + current.id * 7 % 20) / 100;
    int mask = 0;
    for (int i = 0; i < current.steps.size(); ++i) {
        if (current.spent >= qint64(real) * (i + 1) / (current.steps.size() + 0.2)) {
            mask |= 1 << i;
        }
    }
    if ((mask | current.stepsDone) != current.stepsDone) {
        m_store.setSteps(current.id, mask | current.stepsDone);
    }
    if (current.spent < real) {
        return;
    }
    m_store.setSteps(current.id, (1 << current.steps.size()) - 1);
    if (!m_store.submit(current.id, QStringLiteral("Travail terminé et contrôlé, prêt pour validation."), QString())) {
        return;
    }
    m_store.log(s, QStringLiteral("Dépôt pour revue : ") + current.title);
    if (m_store.pendingReviews().size() > 5) {
        m_store.review(current.id, true, 4 + current.id % 2, QStringLiteral("Validé par le chef d'atelier."));
    }
}

void CompanySimulator::step(const QDateTime &now)
{
    const int elapsed = m_previous.isValid() ? int(std::clamp<qint64>(m_previous.secsTo(now), 0, 30)) : 0;
    const bool newDay = m_previous.isValid() && m_previous.date() != now.date();
    m_previous = now;
    m_store.begin();
    if (newDay) {
        for (const Employee &e : std::as_const(m_employees)) {
            leave(e, now.addSecs(-1), QStringLiteral("Fin de journée"), false);
        }
    }
    if (m_demo && now >= m_next) {
        traffic(now);
        m_next = now.addSecs(QRandomGenerator::global()->bounded(11, 32));
    }
    for (const Employee &e : std::as_const(m_employees)) {
        MorphOverlay::breathe();
        const QString who = e.reference();
        if (!m_last.contains(who)) {
            continue;
        }
        WorkSession &s = m_last[who];
        if (m_store.sessionOn(who, s.login.date()) != s.id) {
            m_last.remove(who);
            m_badges.insert(e.id(), true);
            continue;
        }
        (s.state == kPause ? s.pause : s.state == kIdle ? s.idle : s.work) += elapsed;
        s.lastSeen = now;
        if (simulated(e)) {
            if (s.state == kPause && dayPause(who) >= WorkRules::Pause) {
                s.pause = std::max(0, WorkRules::Pause - m_base.value(who).second);
                change(e, kWork, now);
            }
            if (dayWork(who) >= WorkRules::Day) {
                s.work = std::max(0, WorkRules::Day - m_base.value(who).first);
                leave(e, now, QStringLiteral("Fin de journée"));
                continue;
            }
        }
        if (m_demo && e.id() != m_reserved) {
            fillTasks(e);
            if (s.state == kWork) {
                work(e, s, elapsed);
            } else {
                for (const WorkTask &t : m_store.tasks(who)) {
                    if (t.state == WorkTask::Current) {
                        s.task = t.title;
                    }
                }
            }
        }
        m_store.save(s);
    }
    m_store.commit();
}
