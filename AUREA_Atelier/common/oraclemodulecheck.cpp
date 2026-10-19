#include "oraclemodulecheck.h"
#include "database.h"
#include "employeerepository.h"
#include "inventoryrepository.h"
#include "oracleconnection.h"
#include "productionstore.h"
#include "sqldialect.h"
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

struct Log {
    QStringList lines;
    int failed = 0;
    bool check(bool condition, const QString &text) {
        lines << (condition ? QStringLiteral("[ OK ]    ") : QStringLiteral("[ ÉCHEC ] ")) + text;
        if (!condition) ++failed;
        return condition;
    }
    void info(const QString &text) { lines << QStringLiteral("[ info ]  ") + text; }
};

void employeeSteps(Database &database, Log &log, qint64 &createdEmployee, const QString &stamp) {
    EmployeeRepository repo(database.database());
    if (!log.check(repo.initialize(false), QStringLiteral("Employés : schéma Oracle présent (initialize)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()))) return;

    Employee e;
    e.name = QStringLiteral("Essai Oracle Élodie");
    e.email = QStringLiteral("oracle.%1@aurea.invalid").arg(stamp);
    e.position = QStringLiteral("Couturier");
    e.specialty = QStringLiteral("Couture haute précision");
    e.hired = QDate(2020, 1, 1);
    e.baseLoad = 35;
    const bool saved = repo.save(e);
    if (!log.check(saved && e.id > 0, QStringLiteral("Employés : création (INSERT + séquence) id=%1").arg(e.id) + (saved ? QString() : QStringLiteral(" : ") + repo.error()))) return;
    createdEmployee = e.id;

    const Employee back = repo.find(e.id);
    log.check(back.id == e.id && back.name == e.name && back.email == e.email && back.hired == QDate(2020, 1, 1) && back.available && back.baseLoad == 35,
              QStringLiteral("Employés : relecture identique (accents, DATE, booléen, charge)"));
    log.check(repo.all().size() >= 1, QStringLiteral("Employés : liste (all)"));

    Employee duplicate = e;
    duplicate.id = 0;
    duplicate.email = e.email.toUpper();
    const bool duplicated = repo.save(duplicate);
    log.check(!duplicated && repo.error().contains(QStringLiteral("déjà utilisée")), QStringLiteral("Employés : e-mail en double refusé sans tenir compte de la casse (ORA-00001)")
                                                                                             + (duplicated ? QStringLiteral(" — accepté à tort !") : QStringLiteral(" : ") + repo.error()));

    Employee changed = repo.find(e.id);
    changed.baseLoad = 40;
    changed.available = false;
    log.check(repo.save(changed) && repo.find(e.id).baseLoad == 40 && !repo.find(e.id).available, QStringLiteral("Employés : modification (UPDATE, booléen → 0)"));
    changed = repo.find(e.id);
    changed.available = true;
    log.check(repo.save(changed), QStringLiteral("Employés : remise en disponibilité"));

    const QString title = QStringLiteral("Tâche d'essai Oracle %1").arg(stamp);
    log.check(repo.assign(e.id, title, e.specialty, 8) && repo.find(e.id).load == 60, QStringLiteral("Employés : affectation d'une tâche (charge 40 + 20 = 60)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()));
    qint64 assignmentId = 0;
    for (const Assignment &a : repo.assignments())
        if (a.title == title && a.employeeId == e.id) { assignmentId = a.id; log.check(a.load == 20 && !a.completed && a.created.startsWith(QDate::currentDate().toString(Qt::ISODate)), QStringLiteral("Employés : tâche relue (TIMESTAMP « %1 »)").arg(a.created)); }
    log.check(assignmentId > 0 && repo.completeAssignment(assignmentId) && repo.find(e.id).load == 40, QStringLiteral("Employés : tâche terminée, charge revenue à 40"));

    const QDate today = QDate::currentDate();
    log.check(repo.saveAttendance(e.id, today, QTime(8, 0), QTime(17, 0), 60), QStringLiteral("Employés : pointage (INSERT)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()));
    log.check(repo.saveAttendance(e.id, today, QTime(9, 0), QTime(17, 0), 30), QStringLiteral("Employés : même jour modifié (UPDATE, pas de doublon)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()));
    int count = 0; qint64 attendanceId = 0; int minutes = 0;
    for (const Attendance &a : repo.attendance()) if (a.employeeId == e.id && a.day == today) { ++count; attendanceId = a.id; minutes = a.minutes(); }
    log.check(count == 1 && minutes == 450, QStringLiteral("Employés : un seul pointage pour le jour, 450 min (relu : %1 ligne(s), %2 min)").arg(count).arg(minutes));
    log.check(attendanceId > 0 && repo.removeAttendance(attendanceId), QStringLiteral("Employés : pointage supprimé"));
}

void productionSteps(Database &database, Log &log, qint64 employeeId, QVector<int> &createdTasks) {
    TaskStore store(database.database());
    if (!log.check(store.init(false), QStringLiteral("Production : schéma Oracle présent (init)") + (store.error().isEmpty() ? QString() : QStringLiteral(" : ") + store.error()))) return;

    Task t;
    t.order = 4242;
    t.step = QStringLiteral("Patronage");
    t.employeeId = int(employeeId);
    t.priority = QStringLiteral("Haute");
    t.start = QDate::currentDate();
    t.end = QDate::currentDate().addDays(5);
    t.progress = 10;
    const bool saved = store.save(t);
    if (!log.check(saved && t.id > 0, QStringLiteral("Production : création d'une tâche (INSERT + séquence) id=%1").arg(t.id) + (saved ? QString() : QStringLiteral(" : ") + store.error()))) return;
    createdTasks.append(t.id);

    bool found = false;
    for (const Task &x : store.all())
        if (x.id == t.id) {
            found = true;
            log.check(x.order == 4242 && x.step == t.step && x.start == t.start && x.end == t.end && x.priority == QStringLiteral("Haute") && x.progress == 10 && x.employeeId == employeeId,
                      QStringLiteral("Production : relecture identique (dates, priorité, employé)"));
            log.check(x.employeeName == QStringLiteral("Essai Oracle Élodie"), QStringLiteral("Production : nom de l'employé lu dans la table employees (« %1 »)").arg(x.employeeName));
        }
    log.check(found, QStringLiteral("Production : tâche retrouvée dans la liste (all)"));

    t.progress = 100;
    t.employeeId = 0;   // NULL
    bool updated = store.save(t);
    bool nullOk = false, doneOk = false;
    for (const Task &x : store.all()) if (x.id == t.id) { nullOk = x.employeeId == 0; doneOk = x.done(); }
    log.check(updated && nullOk && doneOk, QStringLiteral("Production : modification (UPDATE, avancement 100 %, employé NULL)") + (updated ? QString() : QStringLiteral(" : ") + store.error()));

    Task bad = t;
    bad.id = 0;
    bad.priority = QStringLiteral("Urgente");
    log.check(!store.save(bad), QStringLiteral("Production : priorité invalide refusée"));
}

void cleanup(Database &database, Log &log, qint64 employeeId, const QVector<int> &tasks, const QString &stamp, const QString &lot) {
    QSqlDatabase db = database.database();
    bool ok = true;
    for (int id : tasks) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM tache WHERE id=?"));
        q.addBindValue(id);
        ok = q.exec() && ok;
    }
    {
        QSqlQuery q(db);   // tâches d'affectation conservées après suppression de l'employé (ON DELETE SET NULL)
        q.prepare(QStringLiteral("DELETE FROM employee_assignments WHERE title=?"));
        q.addBindValue(QStringLiteral("Tâche d'essai Oracle %1").arg(stamp));
        ok = q.exec() && ok;
    }
    if (employeeId > 0) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM employee_attendance WHERE employee_id=?"));
        q.addBindValue(int(employeeId));
        ok = q.exec() && ok;
        EmployeeRepository repo(db);
        const bool removed = repo.remove(employeeId);
        log.check(removed && repo.find(employeeId).id == 0 && !repo.remove(employeeId), QStringLiteral("Employés : suppression, puis suppression d'un employé inexistant refusée") + (removed ? QString() : QStringLiteral(" : ") + repo.error()));
    }
    if (!lot.isEmpty()) {   // mouvements orphelins du test Matières
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM movements WHERE lot=?"));
        q.addBindValue(lot);
        ok = q.exec() && ok;
    }
    QSqlQuery left(db);
    left.prepare(QStringLiteral("SELECT (SELECT COUNT(*) FROM employees WHERE email LIKE ?) + (SELECT COUNT(*) FROM movements WHERE lot=?) FROM dual"));
    left.addBindValue(QStringLiteral("oracle.%1@aurea.invalid").arg(stamp));
    left.addBindValue(lot);
    const bool clean = left.exec() && left.next() && left.value(0).toInt() == 0;
    log.check(ok && clean, QStringLiteral("Nettoyage : aucune donnée d'essai ne reste"));
}


// ---------------------------------------------------------------- Matières
void materialSteps(Log &log, const QString &stamp, QString &lotOut) {
    InventoryRepository repo;
    if (!log.check(repo.openOracle(false), QStringLiteral("Matières : connexion et schéma Oracle (openOracle)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()))) return;
    const QString lot = QStringLiteral("ORA-TST-%1").arg(stamp);
    lotOut = lot;

    Material m;
    m.name = QStringLiteral("Soie d'essai Oracle"); m.type = QStringLiteral("Tissu"); m.color = QStringLiteral("Ivoire");
    m.unit = QStringLiteral("m"); m.supplier = QStringLiteral("Fournisseur d'essai"); m.lot = lot;
    m.quantity = 10; m.minimum = 2; m.price = 12.5;
    const bool saved = repo.save(m, {});
    if (!log.check(saved && m.id > 0, QStringLiteral("Matières : création (INSERT + séquence) id=%1").arg(m.id) + (saved ? QString() : QStringLiteral(" : ") + repo.error()))) return;

    const Material back = repo.find(m.id);
    log.check(back.id == m.id && back.name == m.name && back.lot == lot && back.quantity == 10 && back.minimum == 2 && back.price == 12.5 && back.unit == QStringLiteral("m"),
              QStringLiteral("Matières : relecture identique (accents, décimaux)"));
    log.check(repo.findLot(lot.toLower()).id == m.id, QStringLiteral("Matières : recherche du lot sans tenir compte de la casse"));

    Material duplicate = m;
    duplicate.id = 0;
    duplicate.lot = lot.toLower();
    const bool dup = repo.save(duplicate, {});
    log.check(!dup && repo.error().contains(QStringLiteral("existe déjà")), QStringLiteral("Matières : lot en double refusé sans tenir compte de la casse (ORA-00001)") + (dup ? QStringLiteral(" — accepté à tort !") : QStringLiteral(" : ") + repo.error()));

    log.check(repo.move(m.id, 5, QStringLiteral("Réception d'essai")) && repo.find(m.id).quantity == 15, QStringLiteral("Matières : réception +5 (UPDATE + mouvement)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()));
    const bool tooMuch = repo.move(m.id, -100, QStringLiteral("Sortie impossible"));
    log.check(!tooMuch && repo.error().contains(QStringLiteral("Stock insuffisant")) && repo.find(m.id).quantity == 15, QStringLiteral("Matières : sortie supérieure au stock refusée, transaction annulée"));
    Material edited = repo.find(m.id);
    edited.quantity = 12;
    log.check(repo.save(edited, QStringLiteral("Correction d'essai")) && repo.find(m.id).quantity == 12, QStringLiteral("Matières : correction du stock (UPDATE)") + (repo.error().isEmpty() ? QString() : QStringLiteral(" : ") + repo.error()));

    int movements = 0; bool datesOk = true;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const Movement &h : repo.history())
        if (h.lot == lot) {
            ++movements;
            const QDateTime when = QDateTime::fromString(h.date, Qt::ISODateWithMs);
            datesOk = datesOk && when.isValid() && qAbs(when.secsTo(now)) < 600;
        }
    log.check(movements == 3, QStringLiteral("Matières : historique des mouvements (3 attendus : initial, réception, correction ; lus : %1)").arg(movements));
    log.check(datesOk && movements > 0, QStringLiteral("Matières : horodatage TIMESTAMP relu comme UTC, au même format que SQLite"));

    log.check(repo.remove(m.id) && repo.find(m.id).id == 0 && !repo.remove(m.id), QStringLiteral("Matières : suppression, puis suppression d'une matière inexistante refusée"));
    QSqlQuery orphan(repo.database());
    orphan.prepare(QStringLiteral("SELECT COUNT(*) FROM movements WHERE lot=? AND material_id IS NULL"));
    orphan.addBindValue(lot);
    log.check(orphan.exec() && orphan.next() && orphan.value(0).toInt() == 3, QStringLiteral("Matières : mouvements conservés après suppression (ON DELETE SET NULL)"));
}

}   // namespace

namespace OracleCheck {

Report runModules() {
    Log log;
    Report report;
    OracleConnection::useDefaultConnection(true);   // connexion Oracle unique (singleton)
    {
        Database database;
        if (!database.openOracle()) {
            log.check(false, QStringLiteral("Connexion Oracle : ") + database.error());
        } else {
            log.check(true, QStringLiteral("Connexion Oracle ouverte"));
            const QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
            qint64 employeeId = 0;
            QVector<int> tasks;
            employeeSteps(database, log, employeeId, stamp);
            if (employeeId > 0) productionSteps(database, log, employeeId, tasks);
            QString lot;
            materialSteps(log, stamp, lot);
            cleanup(database, log, employeeId, tasks, stamp, lot);
        }
    }   // Database (et ses poignées) détruite avant shutdown()
    OracleConnection::instance().shutdown();
    report.ok = log.failed == 0;
    log.lines << QString() << (report.ok ? QStringLiteral("RÉSULTAT : les trois modules fonctionnent sur Oracle.")
                                          : QStringLiteral("RÉSULTAT : %1 contrôle(s) en échec.").arg(log.failed));
    report.text = log.lines.join(QLatin1Char('\n'));
    return report;
}

}   // namespace OracleCheck
