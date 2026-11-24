#pragma once
#include <QDate>
#include <QString>
#include <QTime>

class Employee {
public:
    Employee();

    int id() const;
    QString name() const;
    QString email() const;
    QString position() const;
    QString specialty() const;
    QDate hired() const;
    bool available() const;
    int baseLoad() const;
    int load() const;
    QString reference() const;

    void setId(int value);
    void setName(const QString &value);
    void setEmail(const QString &value);
    void setPosition(const QString &value);
    void setSpecialty(const QString &value);
    void setHired(const QDate &value);
    void setAvailable(bool value);
    void setBaseLoad(int value);
    void setLoad(int value);

private:
    int m_id;
    QString m_name;
    QString m_email;
    QString m_position;
    QString m_specialty;
    QDate m_hired;
    bool m_available;
    int m_baseLoad;
    int m_load;
};

class Attendance {
public:
    Attendance();

    int id() const;
    int employeeId() const;
    QString name() const;
    QDate day() const;
    QTime arrival() const;
    QTime departure() const;
    int pause() const;
    int minutes(const QTime &now = QTime::currentTime()) const;
    int overtime(const QTime &now = QTime::currentTime()) const;

    void setId(int value);
    void setEmployeeId(int value);
    void setName(const QString &value);
    void setDay(const QDate &value);
    void setArrival(const QTime &value);
    void setDeparture(const QTime &value);
    void setPause(int value);

private:
    int m_id;
    int m_employeeId;
    QString m_name;
    QDate m_day;
    QTime m_arrival;
    QTime m_departure;
    int m_pause;
};

class Assignment {
public:
    Assignment();

    int id() const;
    int employeeId() const;
    QString name() const;
    QString title() const;
    QString specialty() const;
    QString created() const;
    int hours() const;
    int load() const;
    bool completed() const;

    void setId(int value);
    void setEmployeeId(int value);
    void setName(const QString &value);
    void setTitle(const QString &value);
    void setSpecialty(const QString &value);
    void setCreated(const QString &value);
    void setHours(int value);
    void setLoad(int value);
    void setCompleted(bool value);

private:
    int m_id;
    int m_employeeId;
    QString m_name;
    QString m_title;
    QString m_specialty;
    QString m_created;
    int m_hours;
    int m_load;
    bool m_completed;
};

class EmployeeCandidate {
public:
    EmployeeCandidate();

    Employee employee() const;
    int specialtyPoints() const;
    int loadPoints() const;
    int capacityPoints() const;
    bool fits() const;
    int score() const;

    void setEmployee(const Employee &value);
    void setSpecialtyPoints(int value);
    void setLoadPoints(int value);
    void setCapacityPoints(int value);
    void setFits(bool value);

private:
    Employee m_employee;
    int m_specialtyPoints;
    int m_loadPoints;
    int m_capacityPoints;
    bool m_fits;
};
