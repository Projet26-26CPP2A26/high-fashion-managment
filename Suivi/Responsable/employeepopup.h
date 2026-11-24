#pragma once
#include "activitystore.h"
#include "employee.h"
#include <QVariantAnimation>
#include <QWidget>
#include <functional>

class ActivityBoard;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;
class WeekChart;

class EmployeePopup : public QWidget {
    Q_OBJECT
public:
    EmployeePopup(ActivityBoard &board, ActivityStore &store, QWidget *window);
    void openFor(const QString &reference);
    void closePopup();
    QString reference() const
    {
        return m_reference;
    }
    void refresh();
    void showTab(int index);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void place();
    void step(int delta);
    void assign();
    void showTasks();

    ActivityBoard &m_board;
    ActivityStore &m_store;
    QString m_reference, m_specialty, m_tasksShown;
    qreal m_open = 0;
    QVariantAnimation m_animation;
    QFrame *m_panel;
    QLabel *m_name, *m_job, *m_status, *m_values[8], *m_notice;
    QStackedWidget *m_pages;
    QVector<QPushButton *> m_tabs;
    QVBoxLayout *m_tasks;
    QTableWidget *m_log;
    WeekChart *m_week;
    QComboBox *m_title;
    QSpinBox *m_minutes;
};
