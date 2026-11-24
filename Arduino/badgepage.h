#pragma once
#include "badgeservice.h"
#include "employeerepository.h"
#include <QSerialPort>
#include <QTcpSocket>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class LinkLight;
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
    void connectWokwi();
    void readSerial();
    void readNetwork();
    void process(QIODevice &link);
    void showLink(int state, const QString &text);
    void refresh();
    void report(bool ok, const QString &text);
    QString handle(const QString &line, bool mirror = false);

    EmployeeRepository &m_repository;
    BadgeService m_service;
    QSerialPort m_serial;
    QTcpSocket m_tcp;
    int m_telnet = 0;
    QTimer *m_auto = nullptr;
    LinkLight *m_light;
    QVector<QWidget *> m_cards;
    QString m_newest;
    bool m_filled = false;
    QByteArray m_buffer;
    bool m_ready = false;
    QLabel *m_state, *m_count, *m_hint;
    QComboBox *m_ports, *m_people;
    QLineEdit *m_uid;
    QPushButton *m_connect;
    QTableWidget *m_table;
    VirtualBoard *m_board;
};
