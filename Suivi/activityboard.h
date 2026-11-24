#pragma once
#include "activitystore.h"
#include "employee.h"
#include <QHash>
#include <QWidget>
#include <functional>

class EmployeePopup;
class EmployeeTile;
class QComboBox;
class QDateEdit;
class QGridLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;

class ActivityBoard : public QWidget {
    Q_OBJECT
public:
    explicit ActivityBoard(ActivityStore &store, QWidget *parent = nullptr);
    void setEmployees(const QVector<Employee> &employees);
    void setSearch(const QString &text);
    bool exportCsv(const QString &path) const;

    QStringList order() const { return m_order; }
    Employee employee(const QString &reference) const { return m_people.value(reference); }
    QString reference(int employeeId) const;
    QString displayName(const QString &reference) const;
    WorkSession session(const QString &reference) const;
    int connected();
    QVector<WorkSession> sessionsToday();
    void openEmployee(const QString &reference);
    EmployeePopup *popup() const { return m_popup; }

    ActivityStore &store() { return m_store; }
    QVector<WorkTask> reviews();
    bool review(const WorkTask &task, bool accept, int rating, const QString &feedback);
    std::function<QString(int, const QString &, int)> assignHandler;
    QString assignTask(const QString &reference, const QString &title, int minutes);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void refresh();
    void refreshTiles(const QVector<WorkSession> &rows);
    void layoutTiles();
    void notify();
    QVector<WorkSession> collect(const QDate &day);

    ActivityStore &m_store;
    QHash<QString, Employee> m_people;
    QVector<WorkSession> m_rows;
    QString m_search, m_lastEvent;
    QStringList m_order;
    QHash<QString, EmployeeTile *> m_tiles;
    int m_columns = 0;
    qreal m_phase = 0;
    QLabel *m_online, *m_paused, *m_work, *m_overtime, *m_status, *m_empty;
    QGridLayout *m_grid;
    QStackedWidget *m_views;
    QPushButton *m_viewButton, *m_alert;
    QComboBox *m_filter;
    QDateEdit *m_day;
    QTableWidget *m_table;
    EmployeePopup *m_popup = nullptr;
};
