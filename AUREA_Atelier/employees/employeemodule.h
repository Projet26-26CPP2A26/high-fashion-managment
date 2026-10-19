#pragma once
#include "employeerepository.h"
#include <QWidget>

namespace Ui {class EmployeeModule;}
class QTableWidget;
class EmployeeModule:public QWidget {
    Q_OBJECT
public:
    EmployeeModule(QSqlDatabase database,bool demonstration,QWidget *parent=nullptr);
    ~EmployeeModule() override;
    bool ready() const {return initialized;}
    EmployeeRepository &repository() {return repo;}
    void refresh();
    void setSection(int index);
    QString employeesHtml() const;
    QString statisticsHtml() const;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    Ui::EmployeeModule *ui;
    EmployeeRepository repo;
    bool initialized=false;
    QVector<Employee> employees,filtered;
    QVector<Attendance> pointages;
    QVector<Assignment> tasks;
    QVector<EmployeeCandidate> candidates;
    void refreshEmployees();
    void refreshAttendance();
    void refreshCandidates();
    void updateStatistics();
    void updateDashboard();
    void updateHoursChart();
    void updateTeamGrid();
    void editEmployee(qint64 id=0);
    void deleteEmployee();
    void assignTask();
    void completeTask();
    void saveAttendance();
    void loadAttendance(int row);
    void deleteAttendance();
    void calculation();
    void exportPdf(bool statistics);
    void report(bool success,const QString &message);
    qint64 selectedId(QTableWidget *table) const;
};
