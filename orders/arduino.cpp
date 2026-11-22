#include "arduino.h"
#include <QDebug>

Arduino::Arduino()
    : serial(new QSerialPort), arduino_is_available(false)
{
}

Arduino::~Arduino()
{
    if (serial->isOpen()) serial->close();
    delete serial;
}

bool Arduino::isArduinoPort(const QSerialPortInfo &info)
{
    if (info.hasVendorIdentifier()) {
        const quint16 vid = info.vendorIdentifier();
        if (vid == 0x2341 || vid == 0x2A03 || vid == 0x1A86 || vid == 0x0403) return true;
    }
    const QString d = info.description() + info.manufacturer();
    return d.contains(QLatin1String("Arduino"), Qt::CaseInsensitive)
           || d.contains(QLatin1String("CH340"), Qt::CaseInsensitive);
}

int Arduino::connect_arduino()
{
    arduino_is_available = false;
    arduino_port_name.clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        if (isArduinoPort(info)) {
            arduino_is_available = true;
            arduino_port_name = info.portName();
            break;
        }
    }
    qDebug() << "arduino_port_name is :" << arduino_port_name;

    if (!arduino_is_available) return -1;

    serial->setPortName(arduino_port_name);
    if (!serial->open(QSerialPort::ReadWrite)) return 1;

    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    return 0;
}

int Arduino::close_arduino()
{
    if (serial->isOpen()) {
        serial->close();
        return 0;
    }
    return 1;
}

bool Arduino::isConnected() const
{
    return serial->isOpen();
}

QByteArray Arduino::read_from_arduino()
{
    if (serial->isReadable()) return serial->readAll();
    return QByteArray();
}

QString Arduino::readLine()
{
    buffer += read_from_arduino();
    const int i = buffer.indexOf('\n');
    if (i < 0) return QString();
    const QString line = QString::fromUtf8(buffer.left(i)).trimmed();
    buffer.remove(0, i + 1);
    return line;
}

void Arduino::write_to_arduino(const QByteArray &d)
{
    if (serial->isWritable())
        serial->write(d);
    else
        qDebug() << "Couldn't write to serial!";
}
