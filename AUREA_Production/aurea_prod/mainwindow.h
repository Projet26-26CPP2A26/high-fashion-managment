#pragma once
#include "store.h"
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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QSqlDatabase database, bool demo, QWidget *parent = nullptr);
    bool ready() const { return ok; }
    QString error() const { return store.error(); }
    void setPage(int index);
    void renderScreenshots(const QString &directory);
private:
    TaskStore store;
    bool ok = false;
    QSqlDatabase db;
    QStackedWidget *stack;
    QLabel *title, *subtitle, *date;
    QLineEdit *search;
    QList<QPushButton *> navButtons;
    DashboardPage *dashboard;
    TasksPage *tasks;
    PlanningPage *planning;
    TrackingPage *tracking;
    StatsPage *stats;
    void reload();
    void updateDate();
    void showHelp();
};
