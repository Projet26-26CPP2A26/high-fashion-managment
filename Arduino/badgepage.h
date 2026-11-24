#pragma once
#include "badgeservice.h"
#include "employeerepository.h"
#include <QHash>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class ActivityStore;
class PassageFeed;
class QTableWidget;
class LinkLight;
class VirtualBoard;

class BadgePage : public QWidget {
    Q_OBJECT
public:
    BadgePage(EmployeeRepository &repository, ActivityStore &store, QWidget *parent = nullptr);
    ~BadgePage() override;
    void enter();
    BadgeService &service() { return m_service; }
    QHash<int, bool> holders();
    QString virtualScan(const QString &uid);

signals:
    void attendanceChanged();
    void passed(int employeeId, bool arrival);

private:
    void refreshPorts();
    void toggleSerial();
    void showLink();
    void refresh();
    void report(bool ok, const QString &text);
    QString handle(const QString &line, bool mirror = false);

    EmployeeRepository &m_repository;
    ActivityStore &m_store;
    BadgeService m_service;
    LinkLight *m_light;
    QVector<QWidget *> m_cards;
    bool m_ready = false;
    QLabel *m_state, *m_hint;
    QComboBox *m_ports, *m_people;
    QLineEdit *m_uid;
    QPushButton *m_connect;
    PassageFeed *m_refusals;
    QTableWidget *m_table;
    QLabel *m_counts[3];
    VirtualBoard *m_board;
};
