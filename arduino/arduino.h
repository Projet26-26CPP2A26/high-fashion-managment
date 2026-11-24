#ifndef ARDUINO_H
#define ARDUINO_H
#include <QByteArray>
#include <QString>
#include <QTcpSocket>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class Arduino
{
public:
    Arduino();
    ~Arduino();
    int connect_arduino();              // 0 : connecté, 1 : port occupé, -1 : carte absente
    int close_arduino();
    void write_to_arduino(const QByteArray &d);
    QByteArray read_from_arduino();
    QSerialPort *getserial();
    QString getarduino_port_name();
    QTcpSocket *getsimulator();         // Arduino simulé par Wokwi (VS Code), sur ce PC
    void connect_simulator();
    bool is_connected();

private:
    Arduino(const Arduino &) = delete;
    Arduino &operator=(const Arduino &) = delete;

    QSerialPort *serial;
    QTcpSocket *simulator;
    QByteArray telnet;                  // commande RFC2217 incomplète
    static const quint16 simulator_port = 47110;   // même port que wokwi.toml
    static const quint16 arduino_uno_vendor_id = 9025;
    static const quint16 arduino_uno_producy_id = 67;
    static const quint16 ch340_vendor_id = 6790;    // cartes Uno compatibles (puce CH340)
    static const quint16 ch340_product_id = 29987;
    QString arduino_port_name;
    bool arduino_is_available;
    QByteArray data;
};

#endif // ARDUINO_H
