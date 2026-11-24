#include "badgelink.h"
#include <QCoreApplication>
#include <QTimer>

BadgeLink &BadgeLink::instance()
{
    static BadgeLink *link = new BadgeLink(qApp);
    return *link;
}

BadgeLink::BadgeLink(QObject *parent) : QObject(parent)
{
    connect(&m_serial, &QSerialPort::readyRead, this, [this] {
        m_buffer += m_serial.readAll();
        process(m_serial);
    });
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::ResourceError) return;
        m_serial.close();
        setState(Error, QStringLiteral("Carte débranchée"));
    });
    connect(&m_tcp, &QTcpSocket::readyRead, this, &BadgeLink::readNetwork);
    connect(&m_tcp, &QTcpSocket::connected, this, [this] { setState(On, QStringLiteral("Connectée à Wokwi")); });
    connect(&m_tcp, &QTcpSocket::errorOccurred, this, [this] {
        m_tcp.abort();
        setState(Error, QStringLiteral("Wokwi ne répond pas"));
    });
}

void BadgeLink::setState(State state, const QString &text)
{
    m_state = state;
    m_text = text;
    emit stateChanged();
}

bool BadgeLink::connectSerial(const QString &port)
{
    m_buffer.clear();
    m_serial.setPortName(port);
    m_serial.setBaudRate(QSerialPort::Baud9600);
    if (!m_serial.open(QIODevice::ReadWrite)) {
        setState(Error, QStringLiteral("Connexion impossible : ") + m_serial.errorString());
        return false;
    }
    setState(On, QStringLiteral("Connectée sur ") + port);
    return true;
}

void BadgeLink::connectWokwi()
{
    m_buffer.clear();
    m_telnet = 0;
    setState(Waiting, QStringLiteral("Connexion à Wokwi…"));
    m_tcp.connectToHost(QStringLiteral("127.0.0.1"), 4000);
}

void BadgeLink::disconnectLink()
{
    if (m_auto) m_auto->stop();
    m_serial.close();
    m_tcp.abort();
    setState(Off, QStringLiteral("Non connectée"));
}

void BadgeLink::startAuto()
{
    if (m_auto) return;
    m_auto = new QTimer(this);
    connect(m_auto, &QTimer::timeout, this, [this] { if (idle()) connectWokwi(); });
    m_auto->start(2000);
}

void BadgeLink::readNetwork()
{
    const QByteArray data = m_tcp.readAll();
    for (const char ch : data) {
        const uchar c = uchar(ch);
        if (m_telnet == 0) {
            if (c == 255) m_telnet = 1;
            else m_buffer += ch;
        } else if (m_telnet == 1) {
            m_telnet = c == 250 ? 3 : c >= 251 && c <= 254 ? 2 : 0;
        } else if (m_telnet == 2) {
            m_telnet = 0;
        } else if (m_telnet == 3) {
            if (c == 255) m_telnet = 4;
        } else {
            m_telnet = c == 240 ? 0 : 3;
        }
    }
    process(m_tcp);
}

void BadgeLink::process(QIODevice &device)
{
    if (m_buffer.size() > 4096) m_buffer.clear();
    int end;
    while ((end = int(m_buffer.indexOf('\n'))) >= 0) {
        const QString line = QString::fromLatin1(m_buffer.left(end)).trimmed();
        m_buffer.remove(0, end + 1);
        if (!line.startsWith("BADGE:")) continue;
        const QString reply = receiver ? receiver(line) : QStringLiteral("NO;Patientez");
        if (!reply.isEmpty()) device.write(reply.toLatin1() + '\n');
    }
}
