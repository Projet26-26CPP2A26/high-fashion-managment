#pragma once
#include "store.h"
#include <QWidget>

class AreaChart;
class BarChart;
class QCalendarWidget;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QTableView;
class QTableWidget;
class QVBoxLayout;
class QComboBox;
class TaskModel;
class TaskProxy;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);
    void setTasks(const QVector<Task> &tasks);
signals:
    void lateClicked();
    void trackingClicked();
private:
    QLabel *vTotal, *vActive, *vActiveSub, *vDone, *vDonePct, *vAvg, *bnName, *bnText;
    QPushButton *callout;
    AreaChart *chart;
    QGridLayout *miniGrid;
    QTableWidget *dueTable;
};

class TasksPage : public QWidget {
    Q_OBJECT
public:
    TasksPage(TaskStore *store, QWidget *parent = nullptr);
    void setTasks(const QVector<Task> &tasks);
    void setSearch(const QString &text);
    void setStateFilter(int index);
signals:
    void changed();
private:
    TaskStore *store;
    QVector<Task> tasks;
    TaskModel *model;
    TaskProxy *proxy;
    QTableView *view;
    QComboBox *stateBox, *prioBox;
    QLabel *count;
    void updateCount();
    void addTask();
    void editById(int id);
    void deleteById(int id);
    int selectedId() const;
    void exportPdf();
    QString reportHtml() const;
};

class PlanningPage : public QWidget {
    Q_OBJECT
public:
    explicit PlanningPage(QWidget *parent = nullptr);
    void setTasks(const QVector<Task> &tasks);
private:
    QVector<Task> plan;
    QTableWidget *table;
    QCalendarWidget *calendar;
    QListWidget *dayList;
    QLabel *dayTitle;
    void showDay();
};

class TrackingPage : public QWidget {
    Q_OBJECT
public:
    explicit TrackingPage(QWidget *parent = nullptr);
    void setTasks(const QVector<Task> &tasks);
private:
    QVBoxLayout *ordersLayout;
    QLabel *stepName, *stepText;
    BarChart *delayChart;
};

class StatsPage : public QWidget {
    Q_OBJECT
public:
    explicit StatsPage(QWidget *parent = nullptr);
    void setTasks(const QVector<Task> &tasks);
private:
    QVector<Task> tasks;
    BarChart *stepChart;
    QVBoxLayout *prioLayout, *stateLayout;
    void exportPdf();
};
