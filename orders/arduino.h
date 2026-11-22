#ifndef ARDUINO_H
#define ARDUINO_H

#include <QByteArray>
#include <QString>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

// Liaison série PC <-> Arduino (reprise de l'atelier, corrigée).
class Arduino
{
public:
    Arduino();
    ~Arduino();

    int connect_arduino();               // 0 = OK, 1 = ouverture impossible, -1 = carte introuvable
    int close_arduino();                 // 0 = fermé, 1 = déjà fermé
    bool isConnected() const;
    void write_to_arduino(const QByteArray &d);
    QByteArray read_from_arduino();      // lit tout ce qui est disponible (jamais d'UB)
    QString readLine();                  // lit une ligne complète (ex. "LIVRE"), vide sinon
    QSerialPort *getserial() const { return serial; }
    QString getarduino_port_name() const { return arduino_port_name; }

private:
    QSerialPort *serial;
    QString arduino_port_name;
    bool arduino_is_available;
    QByteArray buffer;                   // données reçues pas encore terminées par '\n'

    // 0x2341 / 0x2A03 = Arduino officiel, 0x1A86 = clone CH340, 0x0403 = FTDI
    static bool isArduinoPort(const QSerialPortInfo &info);
};

#endif // ARDUINO_H
