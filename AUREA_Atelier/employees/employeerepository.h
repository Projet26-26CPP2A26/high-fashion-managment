#pragma once
#include <QDate>
#include <QDateTime>
#include <QVariant>
#include <QTime>
#include <QSqlDatabase>
#include <QSqlQuery>
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

// Fonctionne sur SQLite (défaut) ou sur Oracle (pilote QODBC) : le dialecte est choisi d'après le pilote de la connexion.
class EmployeeRepository {
public:
    explicit EmployeeRepository(QSqlDatabase database):db(database),oracle(database.driverName()==QLatin1String("QODBC")) {}
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
    bool oracle;   // vrai si la connexion est Oracle (QODBC)
    QVariant dateValue(const QDate &date) const;           // Oracle : QDate (colonne DATE) ; SQLite : texte ISO
    QVariant dateTimeValue(const QDateTime &time) const;   // Oracle : QDateTime (TIMESTAMP) ; SQLite : texte ISO
    qint64 newId(QSqlQuery &insert, const char *sequence);  // Oracle : SEQUENCE.CURRVAL (lastInsertId() est vide) ; SQLite : lastInsertId()
    bool fail(const QString &message, bool rollback=false);
    bool seed(bool wipe=false);
    bool insertAttendance(qint64 employeeId, const QString &name, const QDate &day, const QTime &arrival, const QTime &departure, int pause);
};
