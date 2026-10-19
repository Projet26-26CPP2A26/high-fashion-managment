#pragma once
#include "productionstore.h"
#include <QList>
#include <QMainWindow>
#include <QSqlDatabase>

class DashboardPage;
class TasksPage;
class PlanningPage;
class TrackingPage;
class StatsPage;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QPushButton;

// Module « Gestion de la production » (tableau de bord, tâches, planning, suivi, statistiques).
// Intégré à la fenêtre principale AUREA : même base que les employés, thème propre au module.
class ProductionWindow : public QMainWindow {
    Q_OBJECT
public:
    ProductionWindow(QSqlDatabase database, bool demo, QWidget *parent = nullptr);
    bool ready() const { return ok; }
    QString error() const { return store.error(); }
    void setPage(int index);
    // Intégration : la fenêtre principale remplace la barre latérale par une rangée de boutons horizontale.
    void setSidebarVisible(bool visible);
    QList<QPushButton *> navButtons() const { return navs; }
    QPushButton *helpButton() const { return helpBtn; }
    void renderScreenshots(const QString &directory);
private:
    TaskStore store;
    bool ok = false;
    QSqlDatabase db;
    QStackedWidget *stack;
    QLabel *title, *subtitle, *date;
    QLineEdit *search;
    QList<QPushButton *> navs;
    QPushButton *helpBtn = nullptr;
    QWidget *sidebar = nullptr;
    DashboardPage *dashboard;
    TasksPage *tasks;
    PlanningPage *planning;
    TrackingPage *tracking;
    StatsPage *stats;
    void reload();
    void updateDate();
    void showHelp();
};
