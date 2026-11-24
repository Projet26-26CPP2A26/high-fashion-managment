#include "oraclemodulecheck.h"
#include "employeerepository.h"
#include "oracleconnection.h"
#include <QDate>
#include <QDateTime>
#include <QStringList>

namespace {
class Log {
public:
    QStringList lines;
    int failed = 0;
    bool check(bool ok, const QString &text, const QString &error = QString())
    {
        lines << (ok ? QStringLiteral("[ OK ]    ") : QStringLiteral("[ ÉCHEC ] ")) + text
                + (!ok && !error.isEmpty() ? QStringLiteral(" : ") + error : QString());
        if (!ok) {
            ++failed;
        }
        return ok;
    }
};

void employeeSteps(EmployeeRepository &repo, Log &log, const QString &stamp, int &createdId)
{
    repo.loadEmployees();
    if (!log.check(repo.error().isEmpty(), "Lecture de la table employees", repo.error())) {
        return;
    }

    Employee e;
    e.setName("Essai Oracle Élodie");
    e.setEmail(QString("oracle.%1@aurea.invalid").arg(stamp));
    e.setPosition("Couturier");
    e.setSpecialty("Couture haute précision");
    e.setHired(QDate(2020, 1, 1));
    e.setBaseLoad(35);
    if (!log.check(repo.addEmployee(e), "Création (INSERT)", repo.error())) {
        return;
    }
    createdId = e.id();

    const Employee back = repo.findEmployee(createdId);
    log.check(back.id() == createdId && back.name() == e.name() && back.email() == e.email()
            && back.position() == e.position() && back.specialty() == e.specialty() && back.hired() == e.hired()
            && back.available() && back.baseLoad() == 35,
        "Relecture identique (accents, DATE, booléen, charge)", repo.error());

    Employee duplicate = e;
    duplicate.setEmail(e.email().toUpper());
    const bool duplicated = repo.addEmployee(duplicate);
    log.check(
        !duplicated && repo.error().contains("déjà utilisée"), "E-mail en double refusé (casse ignorée)", repo.error());
    if (duplicated) {
        repo.deleteEmployee(duplicate.id());
    }

    const QString title = "Tâche d'essai " + stamp;
    log.check(repo.assign(createdId, title, e.specialty(), 8), "Affectation (mémoire)", repo.error());
    log.check(repo.findEmployee(createdId).load() == 55, "Charge recalculée (35 + 20)", repo.error());
    int taskId = 0;
    QString created;
    const QVector<Assignment> tasks = repo.assignments();
    for (const Assignment &a : tasks) {
        if (a.employeeId() == createdId && a.title() == title) {
            taskId = a.id();
            created = a.created();
        }
    }
    log.check(taskId > 0 && QDateTime::fromString(created, Qt::ISODate).isValid(), "Tâche relue", repo.error());
    log.check(repo.completeAssignment(taskId) && repo.findEmployee(createdId).load() == 35,
        "Tâche terminée, charge libérée", repo.error());

    const QDate today = QDate::currentDate();
    log.check(repo.clockIn(createdId, QTime(8, 0)), "Pointage : arrivée (mémoire)", repo.error());
    log.check(!repo.clockIn(createdId, QTime(8, 5)), "Pointage : double arrivée refusée");
    log.check(repo.clockOut(createdId, QTime(17, 0)), "Pointage : départ", repo.error());
    log.check(
        repo.saveAttendance(createdId, today, QTime(9, 0), QTime(17, 0), 30), "Pointage : correction", repo.error());
    int count = 0, attendanceId = 0, minutes = 0;
    const QVector<Attendance> list = repo.attendance();
    for (const Attendance &a : list) {
        if (a.employeeId() == createdId && a.day() == today) {
            ++count;
            attendanceId = a.id();
            minutes = a.minutes();
        }
    }
    log.check(count == 1 && minutes == 450, "Pointage : une seule ligne, 450 min", repo.error());
    log.check(repo.removeAttendance(attendanceId), "Pointage : suppression", repo.error());

    Employee changed = back;
    changed.setBaseLoad(40);
    changed.setAvailable(false);
    log.check(repo.updateEmployee(changed), "Modification (UPDATE)", repo.error());
    const Employee after = repo.findEmployee(createdId);
    log.check(after.baseLoad() == 40 && !after.available(), "Modification relue en base");

    Employee missing = back;
    missing.setId(999999999);
    missing.setEmail("absent." + e.email());
    log.check(!repo.updateEmployee(missing), "Modification d'un employé inexistant refusée");
}
}

namespace OracleCheck {
Report runModules()
{
    Log log;
    OracleConnection &oracle = OracleConnection::instance();
    if (log.check(oracle.open(), "Connexion Oracle", oracle.errorMessage())) {
        EmployeeRepository repo;
        int id = 0;
        employeeSteps(repo, log, QString::number(QDateTime::currentMSecsSinceEpoch()), id);
        if (id > 0) {
            log.check(repo.deleteEmployee(id), "Suppression (DELETE)", repo.error());
            log.check(repo.findEmployee(id).id() == 0 && repo.error().isEmpty(), "Employé supprimé absent de la base",
                repo.error());
            log.check(!repo.deleteEmployee(id), "Suppression d'un employé inexistant refusée");
        }
    }
    oracle.shutdown();
    Report report;
    report.ok = log.failed == 0;
    log.lines << QString()
              << (report.ok ? QStringLiteral("RÉSULTAT : le module Employés fonctionne sur Oracle.")
                            : QStringLiteral("RÉSULTAT : %1 contrôle(s) en échec.").arg(log.failed));
    report.text = log.lines.join(QLatin1Char('\n'));
    return report;
}

}
