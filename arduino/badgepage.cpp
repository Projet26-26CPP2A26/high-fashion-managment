#include "badgepage.h"
#include "fx.h"
#include "linklight.h"
#include "polish.h"
#include "virtualboard.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QFrame *card(QLayout *content)
{
    auto *frame = new QFrame;
    frame->setProperty("card", true);
    content->setContentsMargins(18, 14, 18, 14);
    frame->setLayout(content);
    return frame;
}

QLabel *label(const QString &text, const char *role)
{
    auto *l = new QLabel(text);
    l->setProperty(role, true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

QString summary(const QString &reply)
{
    const QString name = reply.section(';', 1, 1);
    if (reply.startsWith("IN;")) return name + QStringLiteral(" : arrivée enregistrée");
    if (reply.startsWith("OUT;")) return name + QStringLiteral(" : départ enregistré");
    if (reply.startsWith("WAIT;")) return name + QStringLiteral(" : badge déjà lu");
    return reply == "ERR" ? QStringLiteral("Badge : base de données indisponible") : QStringLiteral("Badge refusé");
}
}

BadgePage::BadgePage(EmployeeRepository &repository, QWidget *parent) : QWidget(parent), m_repository(repository)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(16);
    auto *left = new QVBoxLayout;
    left->setSpacing(14);

    auto *link = new QVBoxLayout;
    link->setSpacing(9);
    link->addWidget(label(QStringLiteral("Carte Arduino (USB)"), "section"));
    m_state = label(QStringLiteral("Non connectée"), "muted");
    m_light = new LinkLight;
    auto *stateRow = new QHBoxLayout;
    stateRow->setSpacing(6);
    stateRow->addWidget(m_light);
    stateRow->addWidget(m_state, 1);
    link->addLayout(stateRow);
    m_ports = new QComboBox;
    auto *reload = new QPushButton(QStringLiteral("Actualiser"));
    m_connect = new QPushButton(QStringLiteral("Connecter"));
    m_connect->setProperty("primary", true);
    auto *linkRow = new QHBoxLayout;
    linkRow->addWidget(m_ports, 1);
    linkRow->addWidget(reload);
    linkRow->addWidget(m_connect);
    link->addLayout(linkRow);
    m_cards << card(link);
    left->addWidget(m_cards.last());

    auto *virtualBox = new QVBoxLayout;
    virtualBox->addWidget(label(QStringLiteral("Carte virtuelle (sans matériel)"), "section"));
    m_board = new VirtualBoard;
    virtualBox->addWidget(m_board);
    m_cards << card(virtualBox);
    left->addWidget(m_cards.last());

    auto *assignBox = new QVBoxLayout;
    assignBox->setSpacing(9);
    assignBox->addWidget(label(QStringLiteral("Associer un badge"), "section"));
    m_people = new QComboBox;
    m_uid = new QLineEdit;
    m_uid->setMaxLength(30);
    m_uid->setPlaceholderText(QStringLiteral("Numéro du badge, ex. 04A1B2C3"));
    auto *last = new QPushButton(QStringLiteral("Dernier lu"));
    auto *assignButton = new QPushButton(QStringLiteral("Associer"));
    assignButton->setProperty("primary", true);
    auto *releaseButton = new QPushButton(QStringLiteral("Retirer"));
    releaseButton->setProperty("danger", true);
    auto *uidRow = new QHBoxLayout;
    uidRow->addWidget(m_uid, 1);
    uidRow->addWidget(last);
    auto *buttons = new QHBoxLayout;
    m_hint = label(QString(), "muted");
    m_hint->setWordWrap(true);
    buttons->addWidget(m_hint, 1);
    buttons->addWidget(releaseButton);
    buttons->addWidget(assignButton);
    assignBox->addWidget(m_people);
    assignBox->addLayout(uidRow);
    assignBox->addLayout(buttons);
    m_cards << card(assignBox);
    left->addWidget(m_cards.last());
    left->addStretch();
    root->addLayout(left, 2);

    auto *right = new QVBoxLayout;
    right->setSpacing(10);
    m_count = label(QStringLiteral("Derniers passages"), "section");
    right->addWidget(m_count);
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Date"), QStringLiteral("Employé"), QStringLiteral("Badge"), QStringLiteral("Statut")});
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(46);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setColumnWidth(0, 130);
    m_table->setColumnWidth(2, 120);
    right->addWidget(m_table, 1);
    root->addLayout(right, 3);

    connect(reload, &QPushButton::clicked, this, &BadgePage::refreshPorts);
    connect(m_connect, &QPushButton::clicked, this, [this] {
        if (m_auto) m_auto->stop();
        toggleSerial();
    });
    connect(&m_serial, &QSerialPort::readyRead, this, &BadgePage::readSerial);
    connect(&m_tcp, &QTcpSocket::readyRead, this, &BadgePage::readNetwork);
    connect(&m_tcp, &QTcpSocket::connected, this, [this] {
        showLink(LinkLight::On, QStringLiteral("Connectée à Wokwi"));
    });
    connect(&m_tcp, &QTcpSocket::errorOccurred, this, [this] {
        m_tcp.abort();
        showLink(LinkLight::Error, QStringLiteral("Wokwi ne répond pas"));
    });
    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::ResourceError) return;
        m_serial.close();
        showLink(LinkLight::Error, QStringLiteral("Carte débranchée"));
        refreshPorts();
    });
    connect(&m_service, &BadgeService::scanned, this, [this](int id, bool arrival) {
        if (arrival) m_repository.clockIn(id);
        else m_repository.clockOut(id);
        emit attendanceChanged();
    });
    connect(&m_service, &BadgeService::changed, this, &BadgePage::refresh);
    m_board->scan = [this](const QString &uid) { QTimer::singleShot(Motion::Slow + Motion::Base, this, [this, uid] { virtualScan(uid); }); };
    connect(last, &QPushButton::clicked, this, [this] { m_uid->setText(m_service.lastUid()); });
    connect(m_people, &QComboBox::currentIndexChanged, this, [this] {
        if (m_ready) m_uid->setText(m_service.badgeOf(m_people->currentData().toInt()));
    });
    connect(assignButton, &QPushButton::clicked, this, [this] {
        const int id = m_people->currentData().toInt();
        const bool ok = id > 0 && m_service.assign(m_uid->text(), id);
        report(ok, ok ? QStringLiteral("Badge associé.") : m_service.error());
    });
    connect(releaseButton, &QPushButton::clicked, this, [this] {
        const bool ok = m_service.release(m_people->currentData().toInt());
        if (ok) m_uid->clear();
        report(ok, ok ? QStringLiteral("Badge retiré.") : m_service.error());
    });
    refreshPorts();
    if (qEnvironmentVariableIsSet("AUREA_WOKWI")) {
        m_ports->setCurrentIndex(m_ports->findData(QStringLiteral("wokwi")));
        m_auto = new QTimer(this);
        connect(m_auto, &QTimer::timeout, this, [this] {
            if (!m_serial.isOpen() && m_tcp.state() == QAbstractSocket::UnconnectedState) connectWokwi();
        });
        m_auto->start(2000);
    }
}

void BadgePage::connectWokwi()
{
    m_buffer.clear();
    m_telnet = 0;
    showLink(LinkLight::Waiting, QStringLiteral("Connexion à Wokwi…"));
    m_tcp.connectToHost(QStringLiteral("127.0.0.1"), 4000);
}

void BadgePage::report(bool ok, const QString &text)
{
    m_hint->setText(text);
    m_hint->setStyleSheet(ok ? "color:#7fc08a" : "color:#f0b9bb");
    if (!isVisible()) return;
    Fx::reveal(m_hint, 0, Motion::Base);
    if (!ok) Fx::shake(m_uid);
}

void BadgePage::enter()
{
    m_ready = m_service.ready();
    if (!m_ready) {
        report(false, QStringLiteral("Tables des badges absentes : exécutez oracle/03_badges.sql."));
        return;
    }
    const int keep = m_people->currentData().toInt();
    m_people->clear();
    const QVector<Employee> people = m_repository.loadEmployees();
    for (const Employee &e : people) m_people->addItem(e.name() + QStringLiteral(" · ") + e.reference(), e.id());
    m_people->setCurrentIndex(std::max(0, m_people->findData(keep)));
    refreshPorts();
    refresh();
    int delay = Motion::Step;
    for (QWidget *w : std::as_const(m_cards)) {
        Fx::reveal(w, delay);
        delay += Motion::Step;
    }
}

void BadgePage::refreshPorts()
{
    if (m_serial.isOpen() || m_tcp.state() != QAbstractSocket::UnconnectedState) return;
    m_ports->clear();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        m_ports->addItem(info.portName() + QStringLiteral(" · ") + (info.description().isEmpty() ? QStringLiteral("port série") : info.description()), info.portName());
    m_ports->addItem(QStringLiteral("Wokwi · simulateur"), QStringLiteral("wokwi"));
    m_connect->setEnabled(true);
}

void BadgePage::toggleSerial()
{
    if (m_serial.isOpen()) {
        m_serial.close();
        showLink(LinkLight::Off, QStringLiteral("Non connectée"));
        return;
    }
    if (m_tcp.state() != QAbstractSocket::UnconnectedState) {
        m_tcp.abort();
        showLink(LinkLight::Off, QStringLiteral("Non connectée"));
        return;
    }
    if (m_ports->currentData().toString() == "wokwi") {
        connectWokwi();
        return;
    }
    m_buffer.clear();
    m_serial.setPortName(m_ports->currentData().toString());
    m_serial.setBaudRate(QSerialPort::Baud9600);
    if (!m_serial.open(QIODevice::ReadWrite)) {
        showLink(LinkLight::Error, QStringLiteral("Connexion impossible : ") + m_serial.errorString());
        Fx::shake(m_cards.first());
        return;
    }
    showLink(LinkLight::On, QStringLiteral("Connectée sur ") + m_serial.portName());
}

void BadgePage::readSerial()
{
    m_buffer += m_serial.readAll();
    process(m_serial);
}

void BadgePage::readNetwork()
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

void BadgePage::showLink(int state, const QString &text)
{
    m_light->setState(LinkLight::State(state));
    m_connect->setText(state == LinkLight::On ? QStringLiteral("Déconnecter") : state == LinkLight::Waiting ? QStringLiteral("Annuler") : QStringLiteral("Connecter"));
    m_state->setText(text);
}

void BadgePage::process(QIODevice &link)
{
    if (m_buffer.size() > 4096) m_buffer.clear();
    int end;
    while ((end = int(m_buffer.indexOf('\n'))) >= 0) {
        const QString reply = handle(QString::fromLatin1(m_buffer.left(end)), true);
        m_buffer.remove(0, end + 1);
        if (!reply.isEmpty()) link.write(reply.toLatin1() + '\n');
    }
}

QString BadgePage::handle(const QString &line, bool mirror)
{
    const QString reply = m_service.receive(line);
    if (mirror && !reply.isEmpty()) {
        m_board->present(m_service.lastUid());
        QTimer::singleShot(Motion::Slow, m_board, [this, reply] { m_board->reply(reply); });
    }
    if (!reply.isEmpty() && isVisible()) Fx::toast(window(), summary(reply));
    return reply;
}

QString BadgePage::virtualScan(const QString &uid)
{
    const QString reply = handle(QStringLiteral("BADGE:") + uid);
    m_board->reply(reply);
    return reply;
}

void BadgePage::refresh()
{
    m_board->setBadges(m_service.badges());
    const QVector<BadgeScan> scans = m_service.scans();
    m_count->setText(QStringLiteral("Derniers passages · %1").arg(scans.size()));
    m_table->setRowCount(int(scans.size()));
    for (int row = 0; row < scans.size(); ++row) {
        const BadgeScan &s = scans[row];
        const QString values[] = {s.at.toString("dd/MM HH:mm:ss"), s.name.isEmpty() ? QStringLiteral("Inconnu") : s.name, s.uid,
                                  s.kind == "ARRIVEE" ? QStringLiteral("Arrivée") : s.kind == "DEPART" ? QStringLiteral("Départ") : QStringLiteral("Refusé")};
        for (int c = 0; c < 4; ++c) {
            if (!m_table->item(row, c)) m_table->setItem(row, c, new QTableWidgetItem);
            m_table->item(row, c)->setText(values[c]);
        }
    }
    const QString newest = scans.isEmpty() ? QString() : scans.first().at.toString(Qt::ISODate) + scans.first().uid + scans.first().kind;
    if (m_filled && !newest.isEmpty() && newest != m_newest) TablePolish::flash(m_table, 0);
    m_newest = newest;
    m_filled = true;
}
