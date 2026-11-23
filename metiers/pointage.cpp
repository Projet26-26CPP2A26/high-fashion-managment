#include "employeerepository.h"

QVector<Attendance> EmployeeRepository::attendance() const
{
    return attendanceList;
}

bool EmployeeRepository::clockIn(int employeeId, const QTime &time)
{
    lastError.clear();
    const Employee e = findEmployee(employeeId);
    if (e.id() == 0)
        return fail("Sélectionnez un employé existant.");
    const QDate today = QDate::currentDate();
    for (const Attendance &a : attendanceList) {
        if (a.employeeId() == employeeId && a.day() == today)
            return fail("L'arrivée est déjà enregistrée aujourd'hui.");
    }
    Attendance a;
    a.setId(nextAttendanceId++);
    a.setEmployeeId(employeeId);
    a.setName(e.name());
    a.setDay(today);
    a.setArrival(time);
    attendanceList.prepend(a);
    return true;
}

bool EmployeeRepository::clockOut(int employeeId, const QTime &time)
{
    lastError.clear();
    const QDate today = QDate::currentDate();
    for (int i = 0; i < attendanceList.size(); ++i) {
        if (attendanceList[i].employeeId() != employeeId || attendanceList[i].day() != today)
            continue;
        if (attendanceList[i].departure().isValid())
            return fail("Le départ est déjà enregistré.");
        if (attendanceList[i].arrival().secsTo(time) <= 0)
            return fail("Le départ doit suivre l'arrivée.");
        attendanceList[i].setDeparture(time);
        return true;
    }
    return fail("Aucune arrivée en cours pour cet employé aujourd'hui.");
}

bool EmployeeRepository::saveAttendance(int employeeId, const QDate &day, const QTime &arrival, const QTime &departure, int pause)
{
    lastError.clear();
    const Employee e = findEmployee(employeeId);
    if (e.id() == 0)
        return fail("Sélectionnez un employé existant.");
    if (!day.isValid() || day > QDate::currentDate() || day < e.hired())
        return fail("La date doit être comprise entre l'embauche et aujourd'hui.");
    const int presence = arrival.secsTo(departure) / 60;
    if (!arrival.isValid() || !departure.isValid() || presence <= 0)
        return fail("Le départ doit suivre l'arrivée, dans la même journée.");
    if (pause < 0 || pause > 240 || pause >= presence)
        return fail("La pause doit être inférieure à la durée de présence (0 à 240 min).");

    for (int i = 0; i < attendanceList.size(); ++i) {
        if (attendanceList[i].employeeId() == employeeId && attendanceList[i].day() == day) {
            attendanceList[i].setArrival(arrival);
            attendanceList[i].setDeparture(departure);
            attendanceList[i].setPause(pause);
            return true;
        }
    }
    Attendance a;
    a.setId(nextAttendanceId++);
    a.setEmployeeId(employeeId);
    a.setName(e.name());
    a.setDay(day);
    a.setArrival(arrival);
    a.setDeparture(departure);
    a.setPause(pause);
    attendanceList.prepend(a);
    return true;
}

bool EmployeeRepository::removeAttendance(int id)
{
    lastError.clear();
    for (int i = 0; i < attendanceList.size(); ++i) {
        if (attendanceList[i].id() == id) {
            attendanceList.removeAt(i);
            return true;
        }
    }
    return fail("Ce pointage n'existe plus.");
}
