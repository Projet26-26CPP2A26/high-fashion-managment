#pragma once
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

class QSqlQuery;

struct WorkSession {
    int id = 0;
    QString name;
    QString position, task, state, endReason, via;
    QDateTime login, lastSeen, logout;
    int work = 0, pause = 0, idle = 0, off = 0;
    QString nextTask;
    int taskSpent = 0, taskPlanned = 0, tasksDone = 0, tasksTotal = 0;

    bool ended() const { return logout.isValid(); }

    bool lost() const { return !ended() && lastSeen.secsTo(QDateTime::currentDateTime()) > 20; }
    QString status() const;

    int activePercent() const { const int total = work + idle + off; return total > 0 ? int(work * 100LL / total) : 0; }
};

namespace WorkRules {
const int Day = 8 * 3600, Pause = 3600;
}

struct WorkTask {
    enum State { Todo, Current, Done, Submitted, Rework };
    int id = 0, planned = 0, spent = 0, state = Todo, stepsDone = 0, rating = 0;
    QString who, title, specialty, details, note, file, feedback;
    QStringList steps;
    bool ready() const { return stepsDone == (1 << steps.size()) - 1; }
};

struct StoredAttendance {
    int employeeId;
    QDate day;
    QTime arrival, departure;
    int pause;
};

struct ReviewStats {
    int accepted = 0;
    double rating = 0;
};

struct ActivityEvent {
    QDateTime at;
    QString who, text;
};

struct AccessEntry {
    int id = 0;
    QDateTime at;
    QString who, label, via, kind, detail;
};

class ActivityStore {
public:
    ActivityStore();
    ~ActivityStore();
    bool open(const QString &path = QString());
    QString error() const { return m_error; }

    void begin();
    void commit();
    int insertSession(const WorkSession &session);
    int sessionOn(const QString &name, const QDate &day);
    bool hasSessionOn(const QString &name, const QDate &day) { return sessionOn(name, day) > 0; }
    bool logAt(const QString &who, const QString &text, const QDateTime &at);
    int countEvents(const QString &who, const QString &text, const QDate &day);
    int countSessions(const QString &who, const QDate &day);
    int acceptedToday();
    double averageRating();
    bool hasLiveSession(const QString &name);
    bool closeLostSessions(const QString &name);
    bool start(WorkSession &session);
    bool save(const WorkSession &session);
    bool log(const WorkSession &session, const QString &text);
    QVector<WorkSession> sessions(const QDate &day);
    QVector<ActivityEvent> events(const QDate &day, const QString &who = QString());
    QVector<WorkTask> tasks(const QString &who);
    bool hasTaskHistory(const QString &who);
    int addTask(const WorkTask &task);
    QVector<WorkTask> openTasks();
    bool completeTask(int id);
    QVector<int> dailyWork(const QString &who, int days);
    QVector<StoredAttendance> attendance();
    bool saveAttendance(const StoredAttendance &a);
    bool removeAttendance(int employeeId, const QDate &day);
    QDateTime manualAt(int employeeId, const QDate &day);
    bool setSteps(int id, int mask);
    bool submit(int id, const QString &note, const QString &file);
    bool review(int id, bool accept, int rating, const QString &feedback);
    QVector<WorkTask> pendingReviews();
    ReviewStats reviewStats(const QString &who);
    bool setTaskState(const QString &who, int id, int state);
    bool saveTaskTime(int id, int spent);
    int addEntry(const QString &who, const QString &label, const QString &via, const QString &kind, const QString &detail = QString(), const QDateTime &at = QDateTime());
    QVector<AccessEntry> entries(int limit, const QString &via = QString());
    QVector<AccessEntry> entriesAfter(int id);
    bool clearDay(const QString &who, const QDate &day);
    QString setting(const QString &key);
    bool saveSetting(const QString &key, const QString &value);

private:
    bool run(QSqlQuery &query);
    QVector<WorkTask> readTasks(QSqlQuery &query);
    QString m_connection, m_error;
};

QString clockText(int seconds);
