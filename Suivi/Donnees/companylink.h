#pragma once
#include <QHash>
#include <QObject>
#include <QSet>

class ActivityBoard;
class ActivityStore;
class EmployeeRepository;

class CompanyLink : public QObject {
    Q_OBJECT
public:
    CompanyLink(EmployeeRepository &repository, ActivityStore &store, ActivityBoard &board, QObject *parent);
    void sync();
    QString assignTask(int employeeId, const QString &title, int minutes);

signals:
    void changed();

private:
    void loadAttendance();
    bool syncAttendance();
    bool syncTasks();

    EmployeeRepository &m_repository;
    ActivityStore &m_store;
    ActivityBoard &m_board;
    QHash<int, int> m_taskOf;
    QSet<int> m_knownTasks;
    QHash<QString, QString> m_saved;
};
