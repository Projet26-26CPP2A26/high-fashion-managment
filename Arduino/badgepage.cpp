#include "badgepage.h"
#include "UiAnimations.h"
#include "ConnectionAnimation.h"
#include "activitystore.h"
#include "badgelink.h"
#include "passagefeed.h"
#include "virtualboard.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>
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
}

BadgePage::BadgePage(EmployeeRepository &repository, ActivityStore &store, QWidget *parent) : QWidget(parent), m_repository(repository), m_store(store)
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

    m_feed = new PassageFeed(m_store);
    root->addWidget(m_feed, 3);

    connect(reload, &QPushButton::clicked, this, &BadgePage::refreshPorts);
    connect(m_connect, &QPushButton::clicked, this, &BadgePage::toggleSerial);
    connect(&BadgeLink::instance(), &BadgeLink::stateChanged, this, &BadgePage::showLink);
    m_service.adminUid = m_store.setting(QStringLiteral("admin_badge"));
    BadgeLink::instance().receiver = [this](const QString &line) { return handle(line, true); };
    connect(&m_service, &BadgeService::scanned, this, [this](int id, bool arrival) {
        if (arrival) m_repository.clockIn(id);
        else m_repository.clockOut(id);
        emit passed(id, arrival);
        emit attendanceChanged();
    });
    connect(&m_service, &BadgeService::refused, this, [this](const QString &uid) {
        m_store.addEntry(QString(), QStringLiteral("Badge inconnu"), QStringLiteral("BADGE"), QStringLiteral("REFUSE"), uid);
        m_feed->reload();
    });
    connect(&m_service, &BadgeService::adminScanned, this, [this] {
        m_store.addEntry(QStringLiteral("ADMIN"), QStringLiteral("Responsable RH"), QStringLiteral("BADGE"), QStringLiteral("ARRIVEE"));
        m_feed->reload();
    });
    connect(&m_service, &BadgeService::changed, this, &BadgePage::refresh);
    m_board->scan = [this](const QString &uid) { QTimer::singleShot(Motion::Slow + Motion::Base, this, [this, uid] { virtualScan(uid); }); };
    connect(last, &QPushButton::clicked, this, [this] { m_uid->setText(m_service.lastUid()); });
    connect(m_people, &QComboBox::currentIndexChanged, this, [this] {
        if (m_ready) m_uid->setText(m_people->currentData().toInt() < 0 ? m_service.adminUid : m_service.badgeOf(m_people->currentData().toInt()));
    });
    connect(assignButton, &QPushButton::clicked, this, [this] {
        const int id = m_people->currentData().toInt();
        if (id < 0) {
            const QString uid = BadgeService::normalize(m_uid->text());
            const bool free = !uid.isEmpty() && !m_service.employeeOf(uid);
            if (free) {
                m_store.saveSetting(QStringLiteral("admin_badge"), uid);
                m_service.adminUid = uid;
                refresh();
            }
            report(free, free ? QStringLiteral("Badge du responsable enregistré.") : QStringLiteral("Badge invalide ou déjà associé à un employé."));
            return;
        }
        if (BadgeService::normalize(m_uid->text()) == m_service.adminUid && !m_service.adminUid.isEmpty()) {
            report(false, QStringLiteral("Ce badge appartient au responsable RH."));
            return;
        }
        const bool ok = id > 0 && m_service.assign(m_uid->text(), id);
        report(ok, ok ? QStringLiteral("Badge associé.") : m_service.error());
    });
    connect(releaseButton, &QPushButton::clicked, this, [this] {
        if (m_people->currentData().toInt() < 0) {
            m_store.saveSetting(QStringLiteral("admin_badge"), QStringLiteral("-"));
            m_service.adminUid = QStringLiteral("-");
            m_uid->clear();
            refresh();
            report(true, QStringLiteral("Badge retiré."));
            return;
        }
        const bool ok = m_service.release(m_people->currentData().toInt());
        if (ok) m_uid->clear();
        report(ok, ok ? QStringLiteral("Badge retiré.") : m_service.error());
    });
    refreshPorts();
    showLink();
}

BadgePage::~BadgePage() { BadgeLink::instance().receiver = nullptr; }

QHash<int, bool> BadgePage::holders()
{
    QHash<int, bool> out;
    for (const auto &b : m_service.badges()) {
        const int id = m_service.employeeOf(b.first);
        if (id) out.insert(id, m_service.inside(id));
    }
    return out;
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
    if (m_store.setting(QStringLiteral("scans_imported")).isEmpty()) {
        const QVector<BadgeScan> old = m_service.scans();
        for (auto it = old.rbegin(); it != old.rend(); ++it)
            m_store.addEntry(QString(), it->name.isEmpty() ? QStringLiteral("Badge inconnu") : it->name, QStringLiteral("BADGE"), it->kind, it->name.isEmpty() ? it->uid : QString(), it->at);
        m_store.saveSetting(QStringLiteral("scans_imported"), QStringLiteral("1"));
    }
    const int keep = m_people->currentData().toInt();
    m_people->clear();
    m_people->addItem(QStringLiteral("Responsable RH (administrateur)"), -1);
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
    if (!BadgeLink::instance().idle()) {
        if (m_ports->count()) return;
    }
    m_ports->clear();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        m_ports->addItem(info.portName() + QStringLiteral(" · ") + (info.description().isEmpty() ? QStringLiteral("port série") : info.description()), info.portName());
    m_ports->addItem(QStringLiteral("Wokwi · simulateur"), QStringLiteral("wokwi"));
    m_connect->setEnabled(true);
}

void BadgePage::toggleSerial()
{
    BadgeLink &link = BadgeLink::instance();
    if (!link.idle()) {
        link.disconnectLink();
        return;
    }
    const QString port = m_ports->currentData().toString();
    if (port == "wokwi") link.connectWokwi();
    else if (!link.connectSerial(port)) Fx::shake(m_cards.first());
}

void BadgePage::showLink()
{
    const BadgeLink &link = BadgeLink::instance();
    m_light->setState(LinkLight::State(link.state()));
    m_connect->setText(link.state() == BadgeLink::On ? QStringLiteral("Déconnecter") : link.state() == BadgeLink::Waiting ? QStringLiteral("Annuler") : QStringLiteral("Connecter"));
    m_state->setText(link.text());
    if (link.state() != BadgeLink::Off && link.text().contains("Wokwi") && m_ports->count()) m_ports->setCurrentIndex(std::max(0, m_ports->findData(QStringLiteral("wokwi"))));
}

QString BadgePage::handle(const QString &line, bool mirror)
{
    const QString reply = m_service.receive(line);
    if (mirror && !reply.isEmpty()) {
        m_board->present(m_service.lastUid());
        QTimer::singleShot(Motion::Slow, m_board, [this, reply] { m_board->reply(reply, false); });
    }
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
    QVector<QPair<QString, QString>> badges = m_service.badges();
    if (m_service.adminUid.size() > 3) badges.prepend(qMakePair(m_service.adminUid, QStringLiteral("Responsable RH")));
    m_board->setBadges(badges);
    m_feed->reload();
}
