#pragma once
#include "activitystore.h"
#include "employee.h"
#include <QMainWindow>

class DayTimeline;
class ProgressRing;
class QLabel;
class QPushButton;
class QStackedWidget;
class ActivityLog;
class TimeCard;
class QHBoxLayout;
class QVBoxLayout;

class EmployeeDesk : public QMainWindow {
    Q_OBJECT
public:
    EmployeeDesk(ActivityStore &store, const Employee &employee, const QString &via = QStringLiteral("LIGNE"), QWidget *parent = nullptr);
    bool ready() const { return m_session.id > 0; }
    const WorkSession &session() const { return m_session; }
    void tick(const QDateTime &now, int idleSeconds);
    void finish(const QString &reason);
    void leaveByBadge();
    void showPage(int index);
    bool submitTask(const QString &note, const QString &file);
    int workToday() const { return m_baseWork + m_session.work; }
    void setStep(int index, bool done);
    QString specialty() const { return m_employee.specialty(); }

signals:
    void closed(bool logout);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *createDayPage();
    QWidget *createTasksPage();
    QWidget *createLogPage();
    void setState(const QString &state, const QString &event);
    void togglePause();
    void loadTasks();
    void startTask(int id);
    void showTasks();
    void showSteps();
    void openSubmit();
    void addLog(const QString &text);
    void refresh();
    void flipTimes();
    TimeCard *timeCard();
    void track(const QString &state, int seconds);

    ActivityStore &m_store;
    Employee m_employee;
    WorkSession m_session;
    QVector<WorkTask> m_tasks;
    WorkTask m_current;
    QDateTime m_last;
    int m_baseWork = 0, m_basePause = 0;
    int m_sinceSave = 0, m_maxTask = -1;
    bool m_paused = false, m_logout = false, m_leaving = false, m_finished = false, m_pauseWarned = false, m_elapsed = false;
    QLabel *m_title, *m_subtitle, *m_clock, *m_clockInfo, *m_state, *m_notice;
    QLabel *m_taskName, *m_taskInfo, *m_next, *m_nextInfo, *m_idle, *m_idleToday, *m_done;
    QPushButton *m_pauseButton, *m_finishButton, *m_nextButton;
    QVector<QPushButton *> m_nav;
    ProgressRing *m_taskRing, *m_dayRing;
    QVector<DayTimeline *> m_timelines;
    QVector<TimeCard *> m_times;
    QWidget *m_chip;
    QPushButton *m_filters[5];
    QStackedWidget *m_pages;
    QVBoxLayout *m_columns[4], *m_steps;
    QLabel *m_counts[4];
    QLabel *m_details, *m_quality;
    int m_stepsFor = -1;
    ActivityLog *m_log;
};
