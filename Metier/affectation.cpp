#include "employeerepository.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

static bool betterCandidate(const EmployeeCandidate &a, const EmployeeCandidate &b)
{
    if (a.fits() != b.fits())
        return a.fits();
    if (a.score() != b.score())
        return a.score() > b.score();
    if (a.employee().load() != b.employee().load())
        return a.employee().load() < b.employee().load();
    return QString::localeAwareCompare(a.employee().name(), b.employee().name()) < 0;
}

int EmployeeRepository::taskLoad(int hours)
{
    return static_cast<int>(std::ceil(hours * 100.0 / 40.0));
}

int EmployeeRepository::assignedLoad(int employeeId) const
{
    int total = 0;
    for (const Assignment &a : assignmentList) {
        if (a.employeeId() == employeeId && !a.completed())
            total += a.load();
    }
    return total;
}

QVector<EmployeeCandidate> EmployeeRepository::rank(const QString &specialty, int hours)
{
    QVector<EmployeeCandidate> list;
    if (hours < 1 || hours > 80)
        return list;
    const int extra = taskLoad(hours);
    const QVector<Employee> employees = loadEmployees();
    for (const Employee &e : employees) {
        if (!e.available())
            continue;
        const int remaining = std::max(0, 100 - e.load());
        EmployeeCandidate c;
        c.setEmployee(e);
        c.setFits(extra <= remaining);
        c.setSpecialtyPoints(e.specialty().compare(specialty, Qt::CaseInsensitive) == 0 ? 50 : 0);
        c.setLoadPoints(qRound(40.0 * remaining / 100.0));
        c.setCapacityPoints(c.fits() ? 10 : qRound(10.0 * remaining / extra));
        list.append(c);
    }
    std::sort(list.begin(), list.end(), betterCandidate);
    return list;
}

bool EmployeeRepository::assign(int employeeId, const QString &title, const QString &specialty, int hours)
{
    lastError.clear();
    if (title.trimmed().isEmpty() || specialty.trimmed().isEmpty() || title.size() > 200 || hours < 1 || hours > 80)
        return fail("Renseignez une tâche, une spécialité et une durée de 1 à 80 h.");
    const Employee e = findEmployee(employeeId);
    if (e.id() == 0 || !e.available())
        return fail("L'employé doit exister et être disponible.");
    const int extra = taskLoad(hours);
    if (e.load() + extra > 100)
        return fail("Capacité insuffisante : réduisez la durée ou choisissez un autre employé.");

    Assignment a;
    a.setId(nextAssignmentId++);
    a.setEmployeeId(employeeId);
    a.setName(e.name());
    a.setTitle(title.trimmed());
    a.setSpecialty(specialty.trimmed());
    a.setHours(hours);
    a.setLoad(extra);
    a.setCreated(QDateTime::currentDateTime().toString(Qt::ISODate));
    assignmentList.prepend(a);
    return true;
}

QVector<Assignment> EmployeeRepository::assignments() const
{
    return assignmentList;
}

bool EmployeeRepository::completeAssignment(int id)
{
    lastError.clear();
    for (int i = 0; i < assignmentList.size(); ++i) {
        if (assignmentList[i].id() == id && !assignmentList[i].completed()) {
            assignmentList[i].setCompleted(true);
            return true;
        }
    }
    return fail("Cette tâche est déjà terminée ou n'existe plus.");
}
