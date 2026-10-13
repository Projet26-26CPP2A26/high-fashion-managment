#pragma once
#include <QList>
#include <QMainWindow>
#include <QPushButton>
#include <QSqlDatabase>

namespace Ui {class EmployeeWindow;}
class EmployeeModule;
// Module « Gestion des employés » (tableau de bord, employés, affectation, pointage, statistiques).
class EmployeeWindow:public QMainWindow {
    Q_OBJECT
public:
    EmployeeWindow(QSqlDatabase database,bool demo=false,QWidget *parent=nullptr);
    ~EmployeeWindow() override;
    void setPage(int index);
    bool ready() const;
    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons horizontale.
    void setSidebarVisible(bool visible);
    QList<QPushButton*> navButtons() const;
    QPushButton *helpButton() const;
    EmployeeModule *module() const {return employeeModule;}
    void renderScreenshots(const QString &directory);
private:
    Ui::EmployeeWindow *ui;
    EmployeeModule *employeeModule;
};
