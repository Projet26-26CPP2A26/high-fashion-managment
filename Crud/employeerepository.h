#pragma once
#include "employee.h"
#include <QStringList>
#include <QVector>

class EmployeeRepository {
public:
    EmployeeRepository();

    QVector<Employee> loadEmployees();
    Employee findEmployee(int id);
    bool addEmployee(Employee &employee);
    bool updateEmployee(const Employee &employee);
    bool deleteEmployee(int id);

    QVector<Attendance> attendance() const;
    bool clockIn(int employeeId, const QTime &time = QTime::currentTime());
    bool clockOut(int employeeId, const QTime &time = QTime::currentTime());
    bool saveAttendance(int employeeId, const QDate &day, const QTime &arrival, const QTime &departure, int pause);
    bool removeAttendance(int id);

    QVector<EmployeeCandidate> rank(const QString &specialty, int hours);
    bool assign(int employeeId, const QString &title, const QString &specialty, int hours);
    QVector<Assignment> assignments() const;
    bool completeAssignment(int id);

    QString error() const;
    static QStringList positions();
    static QStringList specialties();
    static QString validate(const Employee &employee);
    static int taskLoad(int hours);

private:
    bool fail(const QString &message);
    bool emailUsed(const QString &email, int exceptId);
    int assignedLoad(int employeeId) const;

    QString lastError;
    QVector<Attendance> attendanceList;
    QVector<Assignment> assignmentList;
    int nextAttendanceId;
    int nextAssignmentId;
};
