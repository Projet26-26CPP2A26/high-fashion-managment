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
    CompanySimulator(ActivityStore &store, const QVector<Employee> &employees, QObject *parent, const QString &lockPath = QString(), bool demo = true, const QHash<int, bool> &badges = {});
    void setEmployees(const QVector<Employee> &employees) { m_employees = employees; }
    void step(const QDateTime &now);
    void badge(int employeeId, bool arrival);

    class Part {
    public:
        QString state;
        int seconds;
    };
    class Plan {
    public:
        QDateTime arrival;
        QVector<Part> parts;
    };
    static Plan plan(const Employee &employee, const QDate &day);
    static WorkSession at(const Plan &plan, const QDateTime &now);

private:
    void seedHistory(const QDate &today);
    void work(const Employee &employee, WorkSession &session, int seconds);
    void fillTasks(const Employee &employee);
    void populate(const QDateTime &now);
    void adopt(const QDateTime &now);
    void arrive(const Employee &employee, const QDateTime &at, const QDateTime &now, const QString &via, bool entry = true);
    void change(const Employee &employee, const QString &state, const QDateTime &now);
    void leave(const Employee &employee, const QDateTime &now, const QString &reason, bool entry = true);
    void traffic(const QDateTime &now);
    int dayWork(const QString &who) const;
    int dayPause(const QString &who) const;
    void remember(const QString &who, int session, const QDate &day);
    static QString viaOf(const Employee &employee);
    bool simulated(const Employee &employee) const;

    ActivityStore &m_store;
    QVector<Employee> m_employees;
    std::unique_ptr<QLockFile> m_lock;
    QHash<QString, WorkSession> m_last;
    QDateTime m_previous, m_next, m_refusal;
    QHash<QString, QDateTime> m_until;
    QHash<QString, QPair<int, int>> m_base;
    QHash<int, bool> m_badges;
    bool m_demo = true;
    int m_reserved = 0;
};
