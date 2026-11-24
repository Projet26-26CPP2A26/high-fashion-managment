#include "materialwindow.h"
#include "bornepage.h"
#include "pdfexport.h"
#include "ui_materialwindow.h"
#include "uihelpers.h"
#include <QRegularExpression>
#include <cmath>

void MaterialWindow::connectArduino()
{
    if (A.is_connected())
        return;
    arduinoData.clear();
    const int ret = A.connect_arduino();
    if (ret == 0)
        setArduinoState(true, QStringLiteral("Arduino connecté : ") + A.getarduino_port_name());
    else if (ret == 1)
        setArduinoState(false, QStringLiteral("Arduino : port occupé (") + A.getarduino_port_name() + ")");
    else {
        setArduinoState(false, QStringLiteral("Arduino non connecté"));
        A.connect_simulator();
    }
}
void MaterialWindow::readArduino()
{
    arduinoData += A.read_from_arduino();
    int end;
    while ((end = arduinoData.indexOf('\n')) >= 0) {
        const QString message = QString::fromLatin1(arduinoData.left(end)).trimmed();
        arduinoData.remove(0, end + 1);
        if (message.isEmpty())
            continue;
        const QByteArray reply = arduinoAnswer(message);
        A.write_to_arduino(reply);
        borne->exchange(A.getserial()->isOpen() ? QStringLiteral("USB") : QStringLiteral("Wokwi"), message, reply);
    }
    if (arduinoData.size() > 100)
        arduinoData.clear();
}
QByteArray MaterialWindow::arduinoAnswer(const QString &message)
{
    const auto line = [](QString text) {
        text = text.normalized(QString::NormalizationForm_D);
        QString ascii;
        for (const QChar c : text)
            if (c.unicode() >= 32 && c.unicode() < 127 && c != '|')
                ascii += c;
        return ascii.left(16).toLatin1();
    };
    static const QRegularExpression format("^(C(\\d{1,6})|([ES])(\\d{1,6}):(\\d{1,5}))$");
    const auto match = format.match(message);
    if (!match.hasMatch())
        return "Message invalide||2\n";
    const bool consult = match.captured(3).isEmpty();
    const int id = match.captured(consult ? 2 : 4).toInt();
    Material m = repo.findMaterial(id);
    if (!repo.error().isEmpty())
        return "Erreur Oracle||2\n";
    if (!m.id())
        return "ID introuvable|" + line("ID " + QString::number(id)) + "|2\n";
    if (!consult) {
        const double amount = match.captured(5).toDouble();
        if (amount <= 0)
            return line(m.name()) + "|Quantite nulle|2\n";
        const bool out = match.captured(3) == "S";
        if (out && amount > m.quantity())
            return line(m.name()) + "|" + line("Maximum: " + QString::number(m.quantity())) + "|2\n";
        if (!repo.changeQuantity(id, out ? -amount : amount))
            return line(m.name()) + "|Operation refusee|2\n";
        m = repo.findMaterial(id);
        refresh();
        notify(QStringLiteral("Borne Arduino : %1 de %2 %3 · %4")
                   .arg(out ? QStringLiteral("sortie") : QStringLiteral("entrée"), StockReports::quantity(amount),
                        m.unit(), m.name()));
    }
    return line(m.name()) + "|" +
           line("Stock: " + QString::number(m.quantity(), 'f', m.quantity() == std::floor(m.quantity()) ? 0 : 2) + " " +
                m.unit()) +
           "|" + (m.low() ? "1" : "0") + "\n";
}
