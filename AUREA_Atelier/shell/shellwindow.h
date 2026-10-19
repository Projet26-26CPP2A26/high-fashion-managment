#pragma once
#include <QMainWindow>
#include <QSqlDatabase>

class QButtonGroup;
class QPushButton;
class QStackedWidget;
class EmployeeWindow;
class InventoryRepository;
class MaterialWindow;
class ProductionWindow;

// Fenêtre principale AUREA :
//  - à gauche, en colonne verticale : « Gestion des employés », « Gestion des matières » et « Gestion de la production » ;
//  - à droite, en haut : les boutons horizontaux du module ouvert (Tableau de bord, Employés…) ;
//  - dessous : la page du module.
class ShellWindow : public QMainWindow {
    Q_OBJECT
public:
    enum Module { Employees = 0, Materials = 1, Production = 2 };

    ShellWindow(QSqlDatabase employeeDatabase, InventoryRepository &stock, bool demo, QWidget *parent = nullptr);
    ~ShellWindow() override;

    bool ready() const;
    void setModule(int module);
    int module() const;
    EmployeeWindow *employees() const { return employeeWindow; }
    MaterialWindow *materials() const { return materialWindow; }
    ProductionWindow *production() const { return productionWindow; }
    void renderScreenshots(const QString &directory);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *makeTabStrip(const QList<QPushButton *> &inner, QPushButton *help, QWidget *parent);

    QStackedWidget *stack = nullptr;
    QStackedWidget *tabStack = nullptr;
    QButtonGroup *group = nullptr;
    QPushButton *employeeButton = nullptr;
    QPushButton *materialButton = nullptr;
    QPushButton *productionButton = nullptr;
    EmployeeWindow *employeeWindow = nullptr;
    MaterialWindow *materialWindow = nullptr;
    ProductionWindow *productionWindow = nullptr;
};
