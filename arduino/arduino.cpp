#include "arduino.h"
#include <QHostAddress>

Arduino::Arduino()
{
    data = "";
    arduino_port_name = "";
    arduino_is_available = false;
    serial = new QSerialPort;
    simulator = new QTcpSocket;
}

Arduino::~Arduino()
{
    close_arduino();
    delete serial;
    delete simulator;
}

QString Arduino::getarduino_port_name()
{
    return arduino_port_name;
}

QSerialPort *Arduino::getserial()
{
    return serial;
}

QTcpSocket *Arduino::getsimulator()
{
    return simulator;
}

void Arduino::connect_simulator()
{
    if (simulator->state() != QAbstractSocket::UnconnectedState)
        return;
    telnet.clear();
    simulator->connectToHost(QHostAddress(QHostAddress::LocalHost), simulator_port);
}

bool Arduino::is_connected()
{
    return serial->isOpen() || simulator->state() == QAbstractSocket::ConnectedState;
}

int Arduino::connect_arduino()
{
    arduino_is_available = false;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        const quint16 vendor = port.vendorIdentifier(), product = port.productIdentifier();
        if ((vendor == arduino_uno_vendor_id && product == arduino_uno_producy_id) ||
            (vendor == ch340_vendor_id && product == ch340_product_id)) {
            arduino_is_available = true;
            arduino_port_name = port.portName();
            break;
        }
    }
    if (!arduino_is_available)
        return -1;
    serial->setPortName(arduino_port_name);
    if (!serial->open(QSerialPort::ReadWrite))
        return 1;
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    return 0;
}

int Arduino::close_arduino()
{
    simulator->abort();
    if (serial->isOpen()) {
        serial->close();
        return 0;
    }
    return 1;
}

QByteArray Arduino::read_from_arduino()
{
    data.clear();
    if (serial->isOpen())
        return data = serial->readAll();
    telnet += simulator->readAll();
    int i = 0;
    while (i < telnet.size()) {
        if (uchar(telnet[i]) != 0xFF) {
            data += telnet[i++];
            continue;
        }
        if (i + 1 >= telnet.size())
            break;
        const uchar command = uchar(telnet[i + 1]);
        if (command == 0xFA) {
            const int end = telnet.indexOf(QByteArray("\xFF\xF0", 2), i + 2);
            if (end < 0)
                break;
            i = end + 2;
        } else if (command >= 0xFB && command <= 0xFE) {
            if (i + 2 >= telnet.size())
                break;
            i += 3;
        } else {
            i += 2;
        }
    }
    telnet.remove(0, i);
    if (telnet.size() > 200)
        telnet.clear();
    return data;
}

void Arduino::write_to_arduino(const QByteArray &d)
{
    if (serial->isOpen())
        serial->write(d);
    else if (simulator->state() == QAbstractSocket::ConnectedState)
        simulator->write(d);
    else
        qDebug() << "Couldn't write to serial!";
}
