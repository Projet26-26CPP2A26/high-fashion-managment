#include "virtualboard.h"
#include "fx.h"
#include "sound.h"
#include <QComboBox>
#include <QLinearGradient>
#include <QPainterPath>
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
        if (m_mode == Checking) return;
        const QString uid = m_badges->currentData().toString();
        present(uid);
        if (scan) scan(uid);
    });
    m_life.start();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &VirtualBoard::tick);
    timer->start(Fx::frameInterval(this));
}

void VirtualBoard::tick()
{
    const qint64 ms = m_mode ? m_clock.elapsed() : 0;
    if (m_mode && m_mode != Checking && ms > Motion::Slow * 2 && m_card.target() > 0) m_card.go(0, Motion::Slow);
    if (m_mode == Accepted && ms > 2600 && m_door.target() > 0) m_door.go(0, Motion::Slow);
    if (m_mode && ms > 3200) {
        m_mode = Idle;
        m_card.go(0, Motion::Slow);
        m_tone.go(0, Motion::Base);
        display(QStringLiteral("AUREA Atelier"), QStringLiteral("Passez le badge"));
    }
    if (isVisible()) update();
}

void VirtualBoard::display(const QString &top, const QString &bottom)
{
    m_top = top.left(16);
    m_bottom = bottom.left(16);
    m_text.jump(0);
    m_text.go(1, Motion::Slow);
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

void VirtualBoard::present(const QString &uid)
{
    m_mode = Checking;
    m_clock.start();
    m_card.go(1, Motion::Slow);
    m_tone.go(1, Motion::Base);
    display(QStringLiteral("Verification..."), uid);
}

void VirtualBoard::reply(const QString &line)
{
    const QString type = line.section(';', 0, 0), name = line.section(';', 1, 1), info = line.section(';', 2, 2);
    m_clock.start();
    Sound::play(type == "IN" ? QStringLiteral("arrivee") : type == "OUT" ? QStringLiteral("depart") : type == "WAIT" ? QStringLiteral("deja") : QStringLiteral("refus"));
    m_mode = Refused;
    m_tone.go(1, Motion::Base);
    if (type == "IN" || type == "OUT") {
        m_mode = Accepted;
        m_door.go(1, Motion::Slow);
        display((type == "IN" ? QStringLiteral("Bonjour ") : QStringLiteral("A bientot ")) + name, (type == "IN" ? QStringLiteral("Arrivee ") : QStringLiteral("Travail ")) + info);
    } else if (type == "WAIT") {
        m_mode = Known;
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
    const QColor tone(m_mode == Accepted ? "#7fc08a" : m_mode == Refused ? "#e08f8f" : "#e2cf9f");
    paintScreen(p, tone);
    paintDoor(p);
    paintReader(p, tone);
    paintCard(p);
}

void VirtualBoard::paintScreen(QPainter &p, const QColor &tone)
{
    const double t = m_clock.isValid() ? m_clock.elapsed() / double(Motion::Slow) : 9;
    p.save();
    if (m_mode == Refused && t < 1) p.translate(std::sin(t * 18) * 5 * (1 - t), 0);
    const QRectF lcd(6, 6, width() - 122, 108);
    const double level = m_tone.value();
    for (int i = 3; i > 0; --i) {
        QColor halo = tone;
        halo.setAlphaF(0.07 * level);
        p.setPen(QPen(halo, i * 3.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(lcd, 14, 14);
    }
    p.setPen(QPen(QColor("#4a4434"), 1));
    p.setBrush(QColor("#17171a"));
    p.drawRoundedRect(lcd, 14, 14);
    const QRectF glass = lcd.adjusted(12, 12, -12, -12);
    p.setPen(QPen(m_mode ? tone : QColor("#6b5f43"), 1.4));
    p.setBrush(QColor("#1d2118"));
    p.drawRoundedRect(glass, 8, 8);
    if (m_mode == Checking) {
        const double x = std::fmod(m_clock.elapsed() / 900.0, 1.0);
        QPainterPath clip;
        clip.addRoundedRect(glass.adjusted(1, 1, -1, -1), 8, 8);
        p.save();
        p.setClipPath(clip);
        const double w = glass.width() * 0.36, left = glass.left() - w + (glass.width() + w) * x;
        QLinearGradient sweep(left, 0, left + w, 0);
        sweep.setColorAt(0, QColor(226, 207, 159, 0));
        sweep.setColorAt(0.5, QColor(226, 207, 159, 46));
        sweep.setColorAt(1, QColor(226, 207, 159, 0));
        p.fillRect(QRectF(left, glass.top(), w, glass.height()), sweep);
        p.restore();
    }
    QFont f("Consolas");
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(std::clamp(int((lcd.width() - 48) / 9.6), 12, 24));
    p.setFont(f);
    p.setPen(QColor("#dfe8b8"));
    const int shown = int(std::ceil(16 * m_text.value()));
    p.drawText(lcd.adjusted(26, 20, -16, -54), Qt::AlignLeft | Qt::AlignVCenter, m_top.left(shown));
    p.drawText(lcd.adjusted(26, 54, -16, -20), Qt::AlignLeft | Qt::AlignVCenter, m_bottom.left(shown));
    p.restore();
}

void VirtualBoard::paintDoor(QPainter &p)
{
    const QRectF frame(width() - 94, 8, 84, 104);
    const double open = std::clamp(m_door.value(), 0.0, 1.0), w = 72 * (1 - 0.68 * open), skew = 12 * open;
    p.setPen(QPen(QColor("#6b5f43"), 2));
    p.setBrush(QColor("#121214"));
    p.drawRoundedRect(frame, 6, 6);
    QLinearGradient light(frame.topRight(), frame.bottomLeft());
    light.setColorAt(0, QColor(241, 225, 180, int(120 * open)));
    light.setColorAt(1, QColor(127, 192, 138, int(40 * open)));
    p.setPen(Qt::NoPen);
    p.setBrush(light);
    p.drawRoundedRect(frame.adjusted(6, 6, -6, -6), 3, 3);
    QPolygonF door;
    door << QPointF(frame.left() + 6, frame.top() + 6) << QPointF(frame.left() + 6 + w, frame.top() + 6 + skew) << QPointF(frame.left() + 6 + w, frame.bottom() - 6 - skew * 0.4) << QPointF(frame.left() + 6, frame.bottom() - 6);
    p.setPen(QPen(QColor("#c9b27c"), 1.2));
    p.setBrush(QColor("#3a3427"));
    p.drawPolygon(door);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#e9d8a8"));
    p.drawEllipse(QPointF(frame.left() + w - 4, frame.center().y()), 2.6, 2.6);
}

void VirtualBoard::paintReader(QPainter &p, const QColor &tone)
{
    const QRectF box(11, 128, 76, 48);
    const double life = m_life.elapsed() / 1000.0, ms = m_clock.isValid() ? m_clock.elapsed() : 0;
    const bool answered = m_mode && m_mode != Checking;
    if (answered) {
        const double wave = std::min(1.0, ms / (Motion::Slow * 1.5));
        QColor ring = tone;
        ring.setAlphaF(0.5 * (1 - wave));
        const double grow = 9 * Motion::ease(wave);
        p.setPen(QPen(ring, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(-grow, -grow, grow, grow), 10 + grow, 10 + grow);
    }
    p.setPen(QPen(m_mode ? tone : QColor("#6b5f43"), 1.2));
    p.setBrush(QColor("#232326"));
    p.drawRoundedRect(box, 10, 10);
    p.setBrush(Qt::NoBrush);
    if (answered && m_card.value() < 0.6) {
        const double draw = Motion::ease((ms - Motion::Slow * 2) / Motion::Slow);
        const QPointF c = box.center();
        QPainterPath mark;
        if (m_mode == Refused) {
            mark.moveTo(c + QPointF(-7, -7));
            mark.lineTo(c + QPointF(7, 7));
            mark.moveTo(c + QPointF(7, -7));
            mark.lineTo(c + QPointF(-7, 7));
        } else {
            mark.moveTo(c + QPointF(-9, 1));
            mark.lineTo(c + QPointF(-2, 8));
            mark.lineTo(c + QPointF(10, -7));
        }
        QPen pen(tone, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        const double length = mark.length() / pen.widthF();
        pen.setDashPattern({std::max(0.01, length * draw), length * 2 + 1});
        p.setPen(pen);
        if (draw > 0.01) p.drawPath(mark);
    } else if (!answered) {
        const double speed = m_mode == Checking ? 1.6 : 0.55;
        for (int i = 0; i < 3; ++i) {
            QColor c = m_mode ? tone : QColor("#c9b27c");
            c.setAlphaF(0.22 + 0.5 * std::pow(0.5 + 0.5 * std::sin((life * speed - i * 0.16) * 6.2832), 2));
            p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap));
            const double radius = 7 + i * 6;
            p.drawArc(QRectF(45 - radius, 152 - radius, radius * 2, radius * 2), -45 * 16, 90 * 16);
        }
    }
    QFont small("Inter");
    small.setPointSizeF(8.5);
    p.setFont(small);
    p.setPen(QColor("#8f8c86"));
    p.drawText(QRectF(104, 128, width() - 190, 24), Qt::AlignVCenter, QStringLiteral("Lecteur RC522"));
    p.setPen(Qt::NoPen);
    p.setBrush(m_mode ? tone : QColor("#77746e"));
    p.drawEllipse(QPointF(108, 163), 3, 3);
    p.setPen(m_mode ? tone : QColor("#8f8c86"));
    const QString text = m_mode == Accepted ? QStringLiteral("Porte ouverte") : m_mode == Refused ? QStringLiteral("Accès refusé") : m_mode == Known ? QStringLiteral("Déjà enregistré") : m_mode == Checking ? QStringLiteral("Lecture du badge") : QStringLiteral("En attente");
    p.drawText(QRectF(118, 151, width() - 200, 24), Qt::AlignVCenter, text);
}

void VirtualBoard::paintCard(QPainter &p)
{
    const double go = m_card.value();
    const QPointF from(width() - 82, 133), to(20, 133);
    const QPointF at = from + (to - from) * go - QPointF(0, std::sin(go * 3.1416) * 8);
    p.save();
    p.translate(at + QPointF(29, 19));
    p.rotate(-7 * std::sin(go * 3.1416));
    const QRectF body(-29, -19, 58, 38);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawRoundedRect(body.translated(0, 3), 6, 6);
    QLinearGradient gold(body.topLeft(), body.bottomRight());
    gold.setColorAt(0, QColor("#bfa370"));
    gold.setColorAt(0.5, QColor("#f1e1b4"));
    gold.setColorAt(1, QColor("#c4a876"));
    p.setBrush(gold);
    p.drawRoundedRect(body, 6, 6);
    p.setBrush(QColor("#3a3427"));
    p.drawEllipse(QPointF(-15, -4), 5.5, 5.5);
    p.drawRoundedRect(QRectF(-4, -9, 24, 3.4), 1.7, 1.7);
    p.drawRoundedRect(QRectF(-4, -2, 17, 3.4), 1.7, 1.7);
    p.drawRoundedRect(QRectF(-21, 9, 41, 2.6), 1.3, 1.3);
    p.restore();
}
