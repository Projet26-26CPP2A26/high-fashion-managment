#pragma once
#include <QObject>
#include <QSerialPort>
#include <QTcpSocket>
#include <functional>

class QTimer;

class BadgeLink : public QObject {
    Q_OBJECT
public:
    enum State { Off, Waiting, On, Error };

    static BadgeLink &instance();
    std::function<QString(const QString &)> receiver;
    State state() const { return m_state; }
    QString text() const { return m_text; }
    bool idle() const { return !m_serial.isOpen() && m_tcp.state() == QAbstractSocket::UnconnectedState; }
    bool connectSerial(const QString &port);
    void connectWokwi();
    void disconnectLink();
    void startAuto();

signals:
    void stateChanged();

private:
    explicit BadgeLink(QObject *parent);
    void readNetwork();
    void process(QIODevice &device);
    void setState(State state, const QString &text);
    QSerialPort m_serial;
    QTcpSocket m_tcp;
    QByteArray m_buffer;
    int m_telnet = 0;
    State m_state = Off;
    QString m_text = QStringLiteral("Non connectée");
    QTimer *m_auto = nullptr;
};
