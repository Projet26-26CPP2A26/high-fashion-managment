#pragma once
#include "badgeservice.h"
#include "employeerepository.h"
#include <QSerialPort>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class VirtualBoard;

class BadgePage : public QWidget {
    Q_OBJECT
public:
    explicit BadgePage(EmployeeRepository &repository, QWidget *parent = nullptr);
    void enter();
    BadgeService &service() { return m_service; }
    QString virtualScan(const QString &uid);

signals:
    void attendanceChanged();

private:
    void refreshPorts();
    void toggleSerial();
    void readSerial();
    void refresh();
    void report(bool ok, const QString &text);
    QString handle(const QString &line);

    EmployeeRepository &m_repository;
    BadgeService m_service;
    QSerialPort m_serial;
    QByteArray m_buffer;
    bool m_ready = false;
    QLabel *m_state, *m_count, *m_hint;
    QComboBox *m_ports, *m_people;
    QLineEdit *m_uid;
    QPushButton *m_connect;
    QTableWidget *m_table;
    VirtualBoard *m_board;
};
