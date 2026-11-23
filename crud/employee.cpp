#include "employee.h"
#include <algorithm>

Employee::Employee()
    : m_id(0), m_available(true), m_baseLoad(0), m_load(0)
{
}

int Employee::id() const { return m_id; }
QString Employee::name() const { return m_name; }
QString Employee::email() const { return m_email; }
QString Employee::position() const { return m_position; }
QString Employee::specialty() const { return m_specialty; }
QDate Employee::hired() const { return m_hired; }
bool Employee::available() const { return m_available; }
int Employee::baseLoad() const { return m_baseLoad; }
int Employee::load() const { return m_load; }

QString Employee::reference() const
{
    return QString("EMP-%1").arg(m_id, 3, 10, QChar('0'));
}

void Employee::setId(int value) { m_id = value; }
void Employee::setName(const QString &value) { m_name = value; }
void Employee::setEmail(const QString &value) { m_email = value; }
void Employee::setPosition(const QString &value) { m_position = value; }
void Employee::setSpecialty(const QString &value) { m_specialty = value; }
void Employee::setHired(const QDate &value) { m_hired = value; }
void Employee::setAvailable(bool value) { m_available = value; }
void Employee::setBaseLoad(int value) { m_baseLoad = value; }
void Employee::setLoad(int value) { m_load = value; }

Attendance::Attendance()
    : m_id(0), m_employeeId(0), m_pause(0)
{
}

int Attendance::id() const { return m_id; }
int Attendance::employeeId() const { return m_employeeId; }
QString Attendance::name() const { return m_name; }
QDate Attendance::day() const { return m_day; }
QTime Attendance::arrival() const { return m_arrival; }
QTime Attendance::departure() const { return m_departure; }
int Attendance::pause() const { return m_pause; }

int Attendance::minutes(const QTime &now) const
{
    QTime end = m_departure;
    if (!end.isValid() && m_day == QDate::currentDate())
        end = now;
    if (!end.isValid())
        return 0;
    return std::max(0, m_arrival.secsTo(end) / 60 - m_pause);
}

int Attendance::overtime(const QTime &now) const
{
    return std::max(0, minutes(now) - 480);
}

void Attendance::setId(int value) { m_id = value; }
void Attendance::setEmployeeId(int value) { m_employeeId = value; }
void Attendance::setName(const QString &value) { m_name = value; }
void Attendance::setDay(const QDate &value) { m_day = value; }
void Attendance::setArrival(const QTime &value) { m_arrival = value; }
void Attendance::setDeparture(const QTime &value) { m_departure = value; }
void Attendance::setPause(int value) { m_pause = value; }

Assignment::Assignment()
    : m_id(0), m_employeeId(0), m_hours(0), m_load(0), m_completed(false)
{
}

int Assignment::id() const { return m_id; }
int Assignment::employeeId() const { return m_employeeId; }
QString Assignment::name() const { return m_name; }
QString Assignment::title() const { return m_title; }
QString Assignment::specialty() const { return m_specialty; }
QString Assignment::created() const { return m_created; }
int Assignment::hours() const { return m_hours; }
int Assignment::load() const { return m_load; }
bool Assignment::completed() const { return m_completed; }

void Assignment::setId(int value) { m_id = value; }
void Assignment::setEmployeeId(int value) { m_employeeId = value; }
void Assignment::setName(const QString &value) { m_name = value; }
void Assignment::setTitle(const QString &value) { m_title = value; }
void Assignment::setSpecialty(const QString &value) { m_specialty = value; }
void Assignment::setCreated(const QString &value) { m_created = value; }
void Assignment::setHours(int value) { m_hours = value; }
void Assignment::setLoad(int value) { m_load = value; }
void Assignment::setCompleted(bool value) { m_completed = value; }

EmployeeCandidate::EmployeeCandidate()
    : m_specialtyPoints(0), m_loadPoints(0), m_capacityPoints(0), m_fits(false)
{
}

Employee EmployeeCandidate::employee() const { return m_employee; }
int EmployeeCandidate::specialtyPoints() const { return m_specialtyPoints; }
int EmployeeCandidate::loadPoints() const { return m_loadPoints; }
int EmployeeCandidate::capacityPoints() const { return m_capacityPoints; }
bool EmployeeCandidate::fits() const { return m_fits; }

int EmployeeCandidate::score() const
{
    return m_specialtyPoints + m_loadPoints + m_capacityPoints;
}

void EmployeeCandidate::setEmployee(const Employee &value) { m_employee = value; }
void EmployeeCandidate::setSpecialtyPoints(int value) { m_specialtyPoints = value; }
void EmployeeCandidate::setLoadPoints(int value) { m_loadPoints = value; }
void EmployeeCandidate::setCapacityPoints(int value) { m_capacityPoints = value; }
void EmployeeCandidate::setFits(bool value) { m_fits = value; }
