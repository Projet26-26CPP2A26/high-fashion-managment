#include "activitystore.h"
#include <QDir>
#include <QSet>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

namespace {
QString iso(const QDateTime &t) { return t.isValid() ? t.toString(Qt::ISODate) : QString(""); }
QDateTime when(const QVariant &v) { return QDateTime::fromString(v.toString(), Qt::ISODate); }
}

QString clockText(int s)
{
    s = qMax(0, s);
    return QStringLiteral("%1:%2:%3").arg(s / 3600, 2, 10, QChar('0')).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
}

QString WorkSession::status() const
{
    if (ended()) return QStringLiteral("Terminé");
    return lost() || state == QStringLiteral("PC coupé") ? QStringLiteral("PC hors ligne") : state;
}

ActivityStore::ActivityStore() : m_connection(QUuid::createUuid().toString()) {}

ActivityStore::~ActivityStore()
{
    if (QSqlDatabase::contains(m_connection)) {
        QSqlDatabase::database(m_connection, false).close();
        QSqlDatabase::removeDatabase(m_connection);
    }
}

bool ActivityStore::run(QSqlQuery &query)
{
    if (query.exec()) { m_error.clear(); return true; }
    m_error = query.lastError().text();
    return false;
}

bool ActivityStore::open(const QString &path)
{
    const QString file = path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/activity.sqlite" : path;
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    db.setDatabaseName(file);
    if (!db.open()) { m_error = db.lastError().text(); return false; }
    for (const char *sql : {"PRAGMA busy_timeout=3000", "PRAGMA journal_mode=WAL", "PRAGMA synchronous=NORMAL",
             "CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, position TEXT, task TEXT, state TEXT,"
             " login TEXT, last_seen TEXT, logout TEXT, end_reason TEXT, work INTEGER, pause INTEGER, idle INTEGER, off INTEGER)",
             "CREATE TABLE IF NOT EXISTS tasks(id INTEGER PRIMARY KEY AUTOINCREMENT, who TEXT, title TEXT, planned INTEGER, spent INTEGER DEFAULT 0, state INTEGER DEFAULT 0, done_at TEXT DEFAULT '')",
             "CREATE TABLE IF NOT EXISTS attendance(employee INTEGER, day TEXT, arrival TEXT, departure TEXT, pause INTEGER, saved_at TEXT, PRIMARY KEY(employee,day))",
             "CREATE TABLE IF NOT EXISTS entries(id INTEGER PRIMARY KEY AUTOINCREMENT, at TEXT, who TEXT, label TEXT, via TEXT, kind TEXT, detail TEXT)",
             "CREATE TABLE IF NOT EXISTS settings(name TEXT PRIMARY KEY, value TEXT)",
             "CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY AUTOINCREMENT, at TEXT, who TEXT, text TEXT)"}) {
        QSqlQuery q(db);
        if (!q.exec(sql)) { m_error = q.lastError().text(); return false; }
    }
    for (const char *sql : {"CREATE INDEX IF NOT EXISTS sessions_name ON sessions(name, login)", "CREATE INDEX IF NOT EXISTS tasks_who ON tasks(who, state)", "CREATE INDEX IF NOT EXISTS events_at ON events(at)"}) {
        QSqlQuery q(db);
        q.exec(sql);
    }

    QSqlQuery known(db);
    bool hasVia = false;
    known.exec("PRAGMA table_info(sessions)");
    while (known.next()) hasVia |= known.value(1).toString() == "via";
    if (!hasVia) {
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE sessions ADD COLUMN via TEXT DEFAULT ''")) { m_error = q.lastError().text(); return false; }
    }

    QSet<QString> columns;
    QSqlQuery info(db);
    info.exec("PRAGMA table_info(tasks)");
    while (info.next()) columns.insert(info.value(1).toString());
    for (const QString &column : {QString("specialty TEXT DEFAULT ''"), QString("details TEXT DEFAULT ''"), QString("steps TEXT DEFAULT ''"), QString("steps_done INTEGER DEFAULT 0"),
                                  QString("note TEXT DEFAULT ''"), QString("file TEXT DEFAULT ''"), QString("feedback TEXT DEFAULT ''"), QString("rating INTEGER DEFAULT 0")}) {
        if (columns.contains(column.section(' ', 0, 0))) continue;
        QSqlQuery q(db);
        if (!q.exec("ALTER TABLE tasks ADD COLUMN " + column)) { m_error = q.lastError().text(); return false; }
    }
    return true;
}

bool ActivityStore::hasLiveSession(const QString &name)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*) FROM sessions WHERE name=? AND logout='' AND last_seen>=?");
    q.addBindValue(name);
    q.addBindValue(iso(QDateTime::currentDateTime().addSecs(-20)));
    return run(q) && q.next() && q.value(0).toInt() > 0;
}

bool ActivityStore::closeLostSessions(const QString &name)
{
    const QString reason = QStringLiteral("Arrêt inattendu (PC éteint ou application fermée)");
    QSqlQuery e(QSqlDatabase::database(m_connection));
    e.prepare("INSERT INTO events(at,who,text) SELECT last_seen,name,? FROM sessions WHERE name=? AND logout=''");
    e.addBindValue(reason);
    e.addBindValue(name);
    if (!run(e)) return false;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE sessions SET logout=last_seen, end_reason=? WHERE name=? AND logout=''");
    q.addBindValue(reason);
    q.addBindValue(name);
    return run(q);
}

bool ActivityStore::start(WorkSession &s)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO sessions(name,position,task,state,login,last_seen,logout,end_reason,work,pause,idle,off,via) VALUES(?,?,?,?,?,?,'','',0,0,0,0,?)");
    for (const QString &v : {s.name, s.position, s.task, s.state, iso(s.login), iso(s.lastSeen), s.via.isNull() ? QString("") : s.via}) q.addBindValue(v);
    if (!run(q)) return false;
    s.id = q.lastInsertId().toInt();
    return log(s, QStringLiteral("Connexion"));
}

bool ActivityStore::save(const WorkSession &s)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE sessions SET task=?,state=?,last_seen=?,logout=?,end_reason=?,work=?,pause=?,idle=?,off=? WHERE id=?");
    for (const QVariant &v : QVariantList{s.task, s.state, iso(s.lastSeen), iso(s.logout), s.endReason, s.work, s.pause, s.idle, s.off, s.id}) q.addBindValue(v);
    return run(q);
}

bool ActivityStore::log(const WorkSession &s, const QString &text)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO events(at,who,text) VALUES(?,?,?)");
    for (const QString &v : {iso(QDateTime::currentDateTime()), s.name, text}) q.addBindValue(v);
    return run(q);
}

QVector<WorkSession> ActivityStore::sessions(const QDate &day)
{
    QVector<WorkSession> list;
    QSqlQuery q(QSqlDatabase::database(m_connection));

    q.prepare("SELECT id,name,position,task,state,login,last_seen,logout,end_reason,work,pause,idle,off,via FROM sessions "
              "WHERE substr(login,1,10)<=? AND substr(last_seen,1,10)>=? ORDER BY login DESC");
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(day.toString(Qt::ISODate));
    if (!run(q)) return list;
    while (q.next()) {
        WorkSession s;
        s.id = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.position = q.value(2).toString();
        s.task = q.value(3).toString();
        s.state = q.value(4).toString();
        s.login = when(q.value(5));
        s.lastSeen = when(q.value(6));
        s.logout = when(q.value(7));
        s.endReason = q.value(8).toString();
        s.work = q.value(9).toInt();
        s.pause = q.value(10).toInt();
        s.idle = q.value(11).toInt();
        s.off = q.value(12).toInt();
        s.via = q.value(13).toString();
        list.append(s);
    }
    return list;
}

QVector<ActivityEvent> ActivityStore::events(const QDate &day, const QString &who)
{
    QVector<ActivityEvent> list;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT at,who,text FROM events WHERE substr(at,1,10)=? AND (?='' OR who=?) ORDER BY at DESC, id DESC LIMIT 100");
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(who.isNull() ? QString("") : who);
    q.addBindValue(who.isNull() ? QString("") : who);
    if (run(q))
        while (q.next()) list.append({when(q.value(0)), q.value(1).toString(), q.value(2).toString()});
    return list;
}

QVector<int> ActivityStore::dailyWork(const QString &who, int days)
{
    QVector<int> out(days, 0);
    const QDate first = QDate::currentDate().addDays(1 - days);
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT substr(login,1,10), SUM(work) FROM sessions WHERE name=? AND substr(login,1,10)>=? GROUP BY 1");
    q.addBindValue(who);
    q.addBindValue(first.toString(Qt::ISODate));
    if (run(q))
        while (q.next()) {
            const int i = int(first.daysTo(QDate::fromString(q.value(0).toString(), Qt::ISODate)));
            if (i >= 0 && i < days) out[i] = q.value(1).toInt();
        }
    return out;
}

QVector<StoredAttendance> ActivityStore::attendance()
{
    QVector<StoredAttendance> list;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT employee,day,arrival,departure,pause FROM attendance ORDER BY day,arrival");
    if (run(q))
        while (q.next())
            list.append({q.value(0).toInt(), QDate::fromString(q.value(1).toString(), Qt::ISODate), QTime::fromString(q.value(2).toString(), "HH:mm"),
                         QTime::fromString(q.value(3).toString(), "HH:mm"), q.value(4).toInt()});
    return list;
}

bool ActivityStore::saveAttendance(const StoredAttendance &a)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT OR REPLACE INTO attendance(employee,day,arrival,departure,pause,saved_at) VALUES(?,?,?,?,?,?)");
    for (const QVariant &v : QVariantList{a.employeeId, a.day.toString(Qt::ISODate), a.arrival.toString("HH:mm"), a.departure.isValid() ? a.departure.toString("HH:mm") : QString(""),
                                          a.pause, iso(QDateTime::currentDateTime())}) q.addBindValue(v);
    return run(q);
}

bool ActivityStore::removeAttendance(int employeeId, const QDate &day)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("DELETE FROM attendance WHERE employee=? AND day=?");
    q.addBindValue(employeeId);
    q.addBindValue(day.toString(Qt::ISODate));
    return run(q);
}

QDateTime ActivityStore::manualAt(int employeeId, const QDate &day)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT saved_at FROM attendance WHERE employee=? AND day=? AND departure<>''");
    q.addBindValue(employeeId);
    q.addBindValue(day.toString(Qt::ISODate));
    return run(q) && q.next() ? when(q.value(0)) : QDateTime::fromSecsSinceEpoch(0);
}

namespace {
const char *kTaskColumns = "SELECT id,who,title,planned,spent,state,specialty,details,steps,steps_done,note,file,feedback,rating FROM tasks ";
}

QVector<WorkTask> ActivityStore::readTasks(QSqlQuery &q)
{
    QVector<WorkTask> list;
    if (!run(q)) return list;
    while (q.next()) {
        WorkTask t;
        t.id = q.value(0).toInt();
        t.who = q.value(1).toString();
        t.title = q.value(2).toString();
        t.planned = q.value(3).toInt();
        t.spent = q.value(4).toInt();
        t.state = q.value(5).toInt();
        t.specialty = q.value(6).toString();
        t.details = q.value(7).toString();
        t.steps = q.value(8).toString().split('\n', Qt::SkipEmptyParts);
        t.stepsDone = q.value(9).toInt();
        t.note = q.value(10).toString();
        t.file = q.value(11).toString();
        t.feedback = q.value(12).toString();
        t.rating = q.value(13).toInt();
        list.append(t);
    }
    return list;
}

QVector<WorkTask> ActivityStore::tasks(const QString &who)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QString(kTaskColumns) + "WHERE who=? AND (state<>2 OR substr(done_at,1,10)=?) "
              "ORDER BY CASE state WHEN 1 THEN 0 WHEN 4 THEN 1 WHEN 0 THEN 2 WHEN 3 THEN 3 ELSE 4 END, id");
    q.addBindValue(who);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    return readTasks(q);
}

QVector<WorkTask> ActivityStore::pendingReviews()
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QString(kTaskColumns) + "WHERE state=3 ORDER BY id");
    return readTasks(q);
}

bool ActivityStore::hasTaskHistory(const QString &who)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*) FROM tasks WHERE who=? AND specialty<>''");
    q.addBindValue(who);
    return run(q) && q.next() && q.value(0).toInt() > 0;
}

int ActivityStore::addTask(const WorkTask &t)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO tasks(who,title,planned,specialty,details,steps) VALUES(?,?,?,?,?,?)");
    for (const QVariant &v : QVariantList{t.who, t.title, t.planned, t.specialty.isNull() ? QString("") : t.specialty, t.details, t.steps.join('\n')}) q.addBindValue(v);
    return run(q) ? q.lastInsertId().toInt() : 0;
}

bool ActivityStore::setTaskState(const QString &who, int id, int state)
{
    if (state == WorkTask::Current) {
        QSqlQuery q(QSqlDatabase::database(m_connection));
        q.prepare("UPDATE tasks SET state=CASE WHEN feedback<>'' THEN 4 ELSE 0 END WHERE who=? AND state=1");
        q.addBindValue(who);
        if (!run(q)) return false;
    }
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET state=? WHERE id=? AND who=? AND state IN (0,1,4)");
    q.addBindValue(state);
    q.addBindValue(id);
    q.addBindValue(who);
    return run(q) && q.numRowsAffected() == 1;
}

bool ActivityStore::saveTaskTime(int id, int spent)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET spent=? WHERE id=?");
    q.addBindValue(spent);
    q.addBindValue(id);
    return run(q);
}

bool ActivityStore::setSteps(int id, int mask)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET steps_done=? WHERE id=?");
    q.addBindValue(mask);
    q.addBindValue(id);
    return run(q);
}

bool ActivityStore::submit(int id, const QString &note, const QString &file)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET state=3, note=?, file=? WHERE id=? AND state=1");
    q.addBindValue(note);
    q.addBindValue(file);
    q.addBindValue(id);
    return run(q) && q.numRowsAffected() == 1;
}

bool ActivityStore::review(int id, bool accept, int rating, const QString &feedback)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET state=?, rating=?, feedback=?, done_at=?, steps_done=CASE WHEN ? THEN steps_done ELSE 0 END WHERE id=? AND state=3");
    q.addBindValue(accept ? WorkTask::Done : WorkTask::Rework);
    q.addBindValue(accept ? rating : 0);
    q.addBindValue(feedback);
    q.addBindValue(accept ? iso(QDateTime::currentDateTime()) : QString(""));
    q.addBindValue(accept ? 1 : 0);
    q.addBindValue(id);
    return run(q) && q.numRowsAffected() == 1;
}

ReviewStats ActivityStore::reviewStats(const QString &who)
{
    ReviewStats r;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*), AVG(NULLIF(rating,0)) FROM tasks WHERE who=? AND state=2");
    q.addBindValue(who);
    if (run(q) && q.next()) { r.accepted = q.value(0).toInt(); r.rating = q.value(1).toDouble(); }
    return r;
}

QVector<WorkTask> ActivityStore::openTasks()
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QString(kTaskColumns) + "WHERE state<>2 ORDER BY id");
    return readTasks(q);
}

bool ActivityStore::completeTask(int id)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("UPDATE tasks SET state=2, done_at=? WHERE id=? AND state<>2");
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(id);
    return run(q);
}

void ActivityStore::begin() { QSqlDatabase::database(m_connection).transaction(); }
void ActivityStore::commit() { QSqlDatabase::database(m_connection).commit(); }

int ActivityStore::insertSession(const WorkSession &s)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO sessions(name,position,task,state,login,last_seen,logout,end_reason,work,pause,idle,off,via) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const QVariant &v : QVariantList{s.name, s.position, s.task, s.state, iso(s.login), iso(s.lastSeen), iso(s.logout), s.endReason, s.work, s.pause, s.idle, s.off, s.via.isNull() ? QString("") : s.via}) q.addBindValue(v);
    return run(q) ? q.lastInsertId().toInt() : 0;
}

int ActivityStore::sessionOn(const QString &name, const QDate &day)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT MAX(id) FROM sessions WHERE name=? AND substr(login,1,10)=?");
    q.addBindValue(name);
    q.addBindValue(day.toString(Qt::ISODate));
    return run(q) && q.next() ? q.value(0).toInt() : 0;
}

bool ActivityStore::logAt(const QString &who, const QString &text, const QDateTime &at)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO events(at,who,text) VALUES(?,?,?)");
    for (const QString &v : {iso(at), who, text}) q.addBindValue(v);
    return run(q);
}

int ActivityStore::acceptedToday()
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*) FROM tasks WHERE state=2 AND rating>0 AND substr(done_at,1,10)=?");
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    return run(q) && q.next() ? q.value(0).toInt() : 0;
}

double ActivityStore::averageRating()
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT AVG(rating) FROM tasks WHERE state=2 AND rating>0");
    return run(q) && q.next() ? q.value(0).toDouble() : 0;
}

int ActivityStore::countEvents(const QString &who, const QString &text, const QDate &day)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*) FROM events WHERE who=? AND text=? AND (?='' OR substr(at,1,10)=?)");
    const QString d = day.isValid() ? day.toString(Qt::ISODate) : QString("");
    for (const QString &v : {who, text, d, d}) q.addBindValue(v);
    return run(q) && q.next() ? q.value(0).toInt() : 0;
}

int ActivityStore::countSessions(const QString &who, const QDate &day)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT COUNT(*) FROM sessions WHERE name=? AND (?='' OR substr(login,1,10)=?)");
    const QString d = day.isValid() ? day.toString(Qt::ISODate) : QString("");
    for (const QString &v : {who, d, d}) q.addBindValue(v);
    return run(q) && q.next() ? q.value(0).toInt() : 0;
}

int ActivityStore::addEntry(const QString &who, const QString &label, const QString &via, const QString &kind, const QString &detail, const QDateTime &at)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT INTO entries(at,who,label,via,kind,detail) VALUES(?,?,?,?,?,?)");
    for (const QString &v : {iso(at.isValid() ? at : QDateTime::currentDateTime()), who, label, via, kind, detail}) q.addBindValue(v.isNull() ? QString("") : v);
    return run(q) ? q.lastInsertId().toInt() : 0;
}

namespace {
AccessEntry readEntry(const QSqlQuery &q)
{
    return {q.value(0).toInt(), when(q.value(1)), q.value(2).toString(), q.value(3).toString(), q.value(4).toString(), q.value(5).toString(), q.value(6).toString()};
}
}

QVector<AccessEntry> ActivityStore::entries(int limit, const QString &via)
{
    QVector<AccessEntry> list;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT id,at,who,label,via,kind,detail FROM entries WHERE (?='' OR via=?) ORDER BY at DESC, id DESC LIMIT ?");
    q.addBindValue(via.isNull() ? QString("") : via);
    q.addBindValue(via.isNull() ? QString("") : via);
    q.addBindValue(limit);
    if (run(q)) while (q.next()) list.append(readEntry(q));
    return list;
}

QVector<AccessEntry> ActivityStore::entriesAfter(int id)
{
    QVector<AccessEntry> list;
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT id,at,who,label,via,kind,detail FROM entries WHERE id>? ORDER BY id");
    q.addBindValue(id);
    if (run(q)) while (q.next()) list.append(readEntry(q));
    return list;
}

bool ActivityStore::clearDay(const QString &who, const QDate &day)
{
    for (const char *sql : {"DELETE FROM sessions WHERE name=? AND substr(login,1,10)=?", "DELETE FROM entries WHERE who=? AND substr(at,1,10)=?", "DELETE FROM events WHERE who=? AND substr(at,1,10)=?"}) {
        QSqlQuery q(QSqlDatabase::database(m_connection));
        q.prepare(sql);
        q.addBindValue(who);
        q.addBindValue(day.toString(Qt::ISODate));
        if (!run(q)) return false;
    }
    return true;
}

QString ActivityStore::setting(const QString &key)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("SELECT value FROM settings WHERE name=?");
    q.addBindValue(key);
    return run(q) && q.next() ? q.value(0).toString() : QString();
}

bool ActivityStore::saveSetting(const QString &key, const QString &value)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare("INSERT OR REPLACE INTO settings(name,value) VALUES(?,?)");
    q.addBindValue(key);
    q.addBindValue(value.isNull() ? QString("") : value);
    return run(q);
}
