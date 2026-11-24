#pragma once
#include "activitystore.h"
#include "employee.h"
#include <QHash>
#include <QLockFile>
#include <QObject>
#include <memory>

class CompanySimulator : public QObject {
    Q_OBJECT
public:
    CompanySimulator(ActivityStore &store, const QVector<Employee> &employees, QObject *parent, const QString &lockPath = QString());
    void setEmployees(const QVector<Employee> &employees) { m_employees = employees; }
    void step(const QDateTime &now);

    struct Part { QString state; int seconds; };
    struct Plan { QDateTime arrival; QVector<Part> parts; };
    static Plan plan(const Employee &employee, const QDate &day);
    static WorkSession at(const Plan &plan, const QDateTime &now);

private:
    void seedHistory(const QDate &today);
    void work(const Employee &employee, WorkSession &session, int seconds);
    void fillTasks(const Employee &employee);

    ActivityStore &m_store;
    QVector<Employee> m_employees;
    std::unique_ptr<QLockFile> m_lock;
    QHash<QString, WorkSession> m_last;
    QDateTime m_previous;
};
