#pragma once
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class ActivityStore;
class EmployeeRepository;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class HoursView : public QWidget {
public:
    HoursView(EmployeeRepository &repository, ActivityStore &store, QWidget *parent = nullptr);
    void refresh();
    QString html() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    class Line {
    public:
        int employeeId = 0;
        QString name, state;
        QDate day;
        QTime arrival, departure;
        int pause = 0, minutes = 0, overtime = 0, days = 1;
    };
    QVector<Line> lines() const;
    QVector<Line> totals(const QVector<Line> &list) const;
    bool inPeriod(const QDate &day) const;
    void exportPdf();
    EmployeeRepository &m_repository;
    ActivityStore &m_store;
    QLineEdit *m_search;
    QComboBox *m_period;
    QPushButton *m_modes[2];
    QLabel *m_values[4];
    QTableWidget *m_daily, *m_summary;
    QVector<QStringList> m_shown;
    bool m_total = false, m_first = true;
};
