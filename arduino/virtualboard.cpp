#include "virtualboard.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

VirtualBoard::VirtualBoard(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(236);
    display(QStringLiteral("AUREA Atelier"), QStringLiteral("Passez le badge"));
    m_badges = new QComboBox;
    auto *button = new QPushButton(QStringLiteral("Passer le badge"));
    button->setObjectName("virtualScanButton");
    button->setProperty("primary", true);
    auto *row = new QHBoxLayout;
    row->addWidget(m_badges, 1);
    row->addWidget(button);
    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->addStretch();
    box->addLayout(row);
    connect(button, &QPushButton::clicked, this, [this] {
        display(QStringLiteral("Verification..."), m_badges->currentData().toString());
        if (scan) scan(m_badges->currentData().toString());
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        const bool open = m_mode == 1 && m_clock.elapsed() < 2800;
        m_door += ((open ? 1.0 : 0.0) - m_door) * 0.16;
        if (m_mode && m_clock.elapsed() > 3000) {
            m_mode = 0;
            display(QStringLiteral("AUREA Atelier"), QStringLiteral("Passez le badge"));
        }
        if (isVisible() && (m_mode || m_door > 0.004)) update();
    });
    timer->start(16);
}

void VirtualBoard::display(const QString &top, const QString &bottom)
{
    m_top = top.left(16);
    m_bottom = bottom.left(16);
    update();
}

void VirtualBoard::setBadges(const QVector<QPair<QString, QString>> &badges)
{
    const QSignalBlocker block(m_badges);
    const QString keep = m_badges->currentData().toString();
    m_badges->clear();
    for (const auto &b : badges) m_badges->addItem(b.second + QStringLiteral(" · ") + b.first, b.first);
    m_badges->addItem(QStringLiteral("Badge inconnu · 0BADBAD0"), QStringLiteral("0BADBAD0"));
    m_badges->setCurrentIndex(std::max(0, m_badges->findData(keep)));
}

void VirtualBoard::reply(const QString &line)
{
    const QString type = line.section(';', 0, 0), name = line.section(';', 1, 1), info = line.section(';', 2, 2);
    m_clock.start();
    m_mode = 2;
    if (type == "IN" || type == "OUT") {
        m_mode = 1;
        display((type == "IN" ? QStringLiteral("Bonjour ") : QStringLiteral("A bientot ")) + name, (type == "IN" ? QStringLiteral("Arrivee ") : QStringLiteral("Travail ")) + info);
    } else if (type == "WAIT") {
        m_mode = 3;
        display(name, QStringLiteral("Deja enregistre"));
    } else if (type == "NO") {
        display(QStringLiteral("Acces refuse"), name);
    } else {
        display(QStringLiteral("Base de donnees"), QStringLiteral("indisponible"));
    }
}

void VirtualBoard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double t = m_mode ? m_clock.elapsed() / 1000.0 : 9;
    const QColor tone(m_mode == 1 ? "#7fc08a" : m_mode == 2 ? "#e08f8f" : "#e2cf9f");
    const QRectF lcd(0, 6, width() - 116, 108);
    p.setPen(QPen(QColor("#4a4434"), 1));
    p.setBrush(QColor("#17171a"));
    p.drawRoundedRect(lcd, 14, 14);
    p.setPen(QPen(m_mode ? tone : QColor("#6b5f43"), 1.4));
    p.setBrush(QColor("#1d2118"));
    p.drawRoundedRect(lcd.adjusted(12, 12, -12, -12), 8, 8);
    QFont f("Consolas");
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(std::clamp(int((lcd.width() - 48) / 9.6), 12, 24));
    p.setFont(f);
    p.setPen(QColor("#dfe8b8"));
    p.drawText(lcd.adjusted(26, 20, -16, -54), Qt::AlignLeft | Qt::AlignVCenter, m_top);
    p.drawText(lcd.adjusted(26, 54, -16, -20), Qt::AlignLeft | Qt::AlignVCenter, m_bottom);

    const QRectF frame(width() - 94, 8, 84, 104);
    p.setPen(QPen(QColor("#6b5f43"), 2));
    p.setBrush(QColor("#121214"));
    p.drawRoundedRect(frame, 6, 6);
    const double open = std::clamp(m_door, 0.0, 1.0), w = 72 * (1 - 0.68 * open), skew = 12 * open;
    QPolygonF door;
    door << QPointF(frame.left() + 6, frame.top() + 6) << QPointF(frame.left() + 6 + w, frame.top() + 6 + skew) << QPointF(frame.left() + 6 + w, frame.bottom() - 6 - skew * 0.4) << QPointF(frame.left() + 6, frame.bottom() - 6);
    p.setPen(QPen(QColor("#c9b27c"), 1.2));
    p.setBrush(QColor("#3a3427"));
    p.drawPolygon(door);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#e9d8a8"));
    p.drawEllipse(QPointF(frame.left() + w - 4, frame.center().y()), 2.6, 2.6);

    const QPointF reader(34, 152);
    p.setPen(QPen(QColor("#6b5f43"), 1.2));
    p.setBrush(QColor("#232326"));
    p.drawRoundedRect(QRectF(0, 128, 76, 48), 10, 10);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        QColor c = tone;
        c.setAlphaF(m_mode && t < 1.2 ? std::clamp(0.9 - std::abs(t * 2.6 - i * 0.5) * 0.5, 0.15, 0.9) : 0.35);
        p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap));
        const double r = 7 + i * 6;
        p.drawArc(QRectF(reader.x() - r, reader.y() - r, r * 2, r * 2), -45 * 16, 90 * 16);
    }
    QFont small("Inter");
    small.setPointSizeF(8.5);
    p.setFont(small);
    p.setPen(QColor("#8f8c86"));
    p.drawText(QRectF(90, 128, width() - 100, 24), Qt::AlignVCenter, QStringLiteral("Lecteur RC522"));
    p.setPen(m_mode ? tone : QColor("#8f8c86"));
    p.drawText(QRectF(90, 150, width() - 100, 24), Qt::AlignVCenter, m_mode == 1 ? QStringLiteral("● Porte ouverte") : m_mode == 2 ? QStringLiteral("● Alarme") : QStringLiteral("○ En attente"));
}
