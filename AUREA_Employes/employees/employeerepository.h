#pragma once
#include <QDate>
#include <QTime>
#include <QSqlDatabase>
#include <QVector>
#include <QMap>

struct Employee {
    qint64 id=0;
    QString name, email, position, specialty;
    QDate hired;
    bool available=true;
    int baseLoad=0, load=0;
    QString reference() const { return QStringLiteral("EMP-%1").arg(id,3,10,QChar('0')); }
};
struct Attendance {
    qint64 id=0, employeeId=0;
    QString name;
    QDate day;
    QTime arrival, departure;
    int pause=0;
    int minutes(const QTime &now=QTime::currentTime()) const;
    int overtime(const QTime &now=QTime::currentTime()) const;
};
struct Assignment {
    qint64 id=0, employeeId=0;
    QString name, title, specialty, created;
    int hours=0, load=0;
    bool completed=false;
};
struct EmployeeCandidate {
    Employee employee;
    int specialtyPoints=0, loadPoints=0, capacityPoints=0;
    bool fits=false;
    int score() const { return specialtyPoints+loadPoints+capacityPoints; }
};

// Uses the same SQLite connection as the materials repository.
class EmployeeRepository {
public:
    explicit EmployeeRepository(QSqlDatabase database):db(database) {}
    bool initialize(bool demo=false);
    QVector<Employee> all();
    Employee find(qint64 id);
    bool save(Employee &employee);
    bool remove(qint64 id);
    QVector<Attendance> attendance();
    bool clockIn(qint64 employeeId, const QDate &day=QDate::currentDate(), const QTime &time=QTime::currentTime());
    bool clockOut(qint64 employeeId, const QDate &day=QDate::currentDate(), const QTime &time=QTime::currentTime());
    bool saveAttendance(qint64 employeeId, const QDate &day, const QTime &arrival, const QTime &departure, int pause);
    bool removeAttendance(qint64 id);
    QVector<EmployeeCandidate> rank(const QString &specialty, int hours);
    bool assign(qint64 employeeId, const QString &title, const QString &specialty, int hours);
    QVector<Assignment> assignments();
    bool completeAssignment(qint64 id);
    QString error() const { return lastError; }
    static QStringList positions();
    static QStringList specialties();
    static QString validate(const Employee &employee);
    static int taskLoad(int hours);
private:
    QSqlDatabase db;
    QString lastError;
    bool fail(const QString &message, bool rollback=false);
    bool seed(bool wipe=false);
    bool insertAttendance(qint64 employeeId, const QString &name, const QDate &day, const QTime &arrival, const QTime &departure, int pause);
};
