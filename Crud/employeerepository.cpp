#include "employeerepository.h"
#include "oracleconnection.h"
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

EmployeeRepository::EmployeeRepository()
    : nextAttendanceId(1), nextAssignmentId(1)
{
}

QString EmployeeRepository::error() const
{
    return lastError;
}

bool EmployeeRepository::fail(const QString &message)
{
    lastError = message;
    return false;
}

QStringList EmployeeRepository::positions()
{
    return {"Chef d'atelier", "Modéliste", "Couturier", "Brodeur", "Coupeur", "Finisseur", "Responsable qualité"};
}

QStringList EmployeeRepository::specialties()
{
    return {"Couture haute précision", "Broderie main", "Patronage", "Drapé et moulage", "Finitions", "Contrôle qualité"};
}

QString EmployeeRepository::validate(const Employee &e)
{
    if (e.name().trimmed().isEmpty() || e.position().trimmed().isEmpty() || e.specialty().trimmed().isEmpty())
        return "Le nom, le poste et la spécialité sont obligatoires.";
    if (e.name().size() > 120 || e.email().size() > 180 || e.position().size() > 100 || e.specialty().size() > 100)
        return "Un champ dépasse sa longueur maximale.";
    QRegularExpression emailPattern("^[^@\\s]+@[^@\\s]+\\.[^@\\s]{2,}$");
    if (!emailPattern.match(e.email().trimmed()).hasMatch())
        return "L'adresse e-mail n'est pas valide.";
    if (!e.hired().isValid() || e.hired() > QDate::currentDate() || e.hired() < QDate(1970, 1, 1))
        return "La date d'embauche doit être valide et ne peut pas être future.";
    if (e.baseLoad() < 0 || e.baseLoad() > 100)
        return "La charge initiale doit être comprise entre 0 et 100 %.";
    return QString();
}

bool EmployeeRepository::emailUsed(const QString &email, int exceptId)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM employees WHERE UPPER(email) = UPPER(?) AND id <> ?");
    query.addBindValue(email);
    query.addBindValue(exceptId);
    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

QVector<Employee> EmployeeRepository::loadEmployees()
{
    lastError.clear();
    QVector<Employee> list;
    QSqlQuery query;
    if (!query.exec("SELECT id, name, email, position, specialty, hired, available, base_load "
                    "FROM employees ORDER BY UPPER(name)")) {
        fail(query.lastError().text());
        return list;
    }
    while (query.next()) {
        Employee e;
        e.setId(query.value(0).toInt());
        e.setName(query.value(1).toString());
        e.setEmail(query.value(2).toString());
        e.setPosition(query.value(3).toString());
        e.setSpecialty(query.value(4).toString());
        e.setHired(query.value(5).toDate());
        e.setAvailable(query.value(6).toInt() == 1);
        e.setBaseLoad(query.value(7).toInt());
        e.setLoad(e.baseLoad() + assignedLoad(e.id()));
        list.append(e);
    }
    return list;
}

Employee EmployeeRepository::findEmployee(int id)
{
    const QVector<Employee> list = loadEmployees();
    for (const Employee &e : list) {
        if (e.id() == id)
            return e;
    }
    return Employee();
}

bool EmployeeRepository::addEmployee(Employee &employee)
{
    lastError.clear();
    const QString problem = validate(employee);
    if (!problem.isEmpty())
        return fail(problem);
    if (emailUsed(employee.email().trimmed(), 0))
        return fail("Cette adresse e-mail est déjà utilisée.");

    employee.setId(OracleConnection::nextId("employees"));
    QSqlQuery query;
    query.prepare("INSERT INTO employees(id, name, email, position, specialty, hired, available, base_load) "
                  "VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(employee.id());
    query.addBindValue(employee.name().trimmed());
    query.addBindValue(employee.email().trimmed());
    query.addBindValue(employee.position().trimmed());
    query.addBindValue(employee.specialty().trimmed());
    query.addBindValue(employee.hired());
    query.addBindValue(employee.available() ? 1 : 0);
    query.addBindValue(employee.baseLoad());
    if (!query.exec())
        return fail(query.lastError().text());
    return true;
}

bool EmployeeRepository::updateEmployee(const Employee &employee)
{
    lastError.clear();
    const QString problem = validate(employee);
    if (!problem.isEmpty())
        return fail(problem);
    if (emailUsed(employee.email().trimmed(), employee.id()))
        return fail("Cette adresse e-mail est déjà utilisée.");
    if (employee.baseLoad() + assignedLoad(employee.id()) > 100)
        return fail("Cette charge initiale provoquerait une surcharge avec les tâches en cours.");

    QSqlQuery query;
    query.prepare("UPDATE employees SET name = ?, email = ?, position = ?, specialty = ?, hired = ?, "
                  "available = ?, base_load = ? WHERE id = ?");
    query.addBindValue(employee.name().trimmed());
    query.addBindValue(employee.email().trimmed());
    query.addBindValue(employee.position().trimmed());
    query.addBindValue(employee.specialty().trimmed());
    query.addBindValue(employee.hired());
    query.addBindValue(employee.available() ? 1 : 0);
    query.addBindValue(employee.baseLoad());
    query.addBindValue(employee.id());
    if (!query.exec())
        return fail(query.lastError().text());
    if (query.numRowsAffected() == 0)
        return fail("Cet employé n'existe plus.");
    return true;
}

bool EmployeeRepository::deleteEmployee(int id)
{
    lastError.clear();
    QSqlQuery query;
    query.prepare("DELETE FROM employees WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec())
        return fail(query.lastError().text());
    if (query.numRowsAffected() == 0)
        return fail("Cet employé n'existe plus.");
    for (int i = 0; i < attendanceList.size(); ++i) {
        if (attendanceList[i].employeeId() == id)
            attendanceList[i].setEmployeeId(0);
    }
    for (int i = 0; i < assignmentList.size(); ++i) {
        if (assignmentList[i].employeeId() == id)
            assignmentList[i].setEmployeeId(0);
    }
    return true;
}
