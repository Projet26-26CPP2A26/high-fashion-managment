#include "BadgeAnimation.h"
#include "UiAnimations.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>

BadgeAnimation::BadgeAnimation(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(210);
    m_life.start();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (m_mode == Denied && m_clock.elapsed() > 2200) {
            m_mode = Idle;
            m_tone.go(0, Motion::Base);
            m_card.go(0, Motion::Slow);
        }
        if (m_mode == Denied && m_clock.elapsed() > 900 && m_card.target() > 0) m_card.go(0, Motion::Slow);
        if (isVisible()) update();
    });
    timer->start(Fx::frameInterval(this));
}

void BadgeAnimation::present()
{
    m_mode = Checking;
    m_clock.start();
    m_card.go(1, Motion::Slow);
    m_tone.go(1, Motion::Base);
}

void BadgeAnimation::answer(bool granted)
{
    m_mode = granted ? Granted : Denied;
    m_clock.start();
}

void BadgeAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double life = m_life.elapsed() / 1000.0, ms = m_clock.isValid() ? m_clock.elapsed() : 0, level = m_tone.value(), card = m_card.value();
    const QColor gold("#e2cf9f"), tone(m_mode == Granted ? "#7fc08a" : m_mode == Denied ? "#e08f8f" : "#e2cf9f");
    const bool answered = m_mode == Granted || m_mode == Denied;
    QPointF c(width() / 2.0, 92);
    if (m_mode == Denied && ms < Motion::Slow) c.rx() += std::sin(ms / Motion::Slow * 18) * 6 * (1 - ms / Motion::Slow);

    QRadialGradient glow(c, 120);
    QColor light = tone;
    light.setAlphaF(0.07 + 0.10 * level);
    glow.setColorAt(0, light);
    light.setAlphaF(0);
    glow.setColorAt(1, light);
    p.fillRect(rect(), glow);

    if (answered) {
        for (int i = 0; i < 2; ++i) {
            const double t = (ms - i * 130) / (Motion::Slow * 1.7);
            if (t <= 0 || t >= 1) continue;
            QColor ring = tone;
            ring.setAlphaF(0.5 * (1 - t));
            const double grow = 26 * Motion::ease(t);
            p.setPen(QPen(ring, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(c.x() - 58 - grow, c.y() - 58 - grow, 116 + 2 * grow, 116 + 2 * grow), 26 + grow, 26 + grow);
        }
    }
    p.setPen(QPen(m_mode == Idle ? QColor("#6b5f43") : tone, 1.6));
    p.setBrush(QColor("#232326"));
    p.drawRoundedRect(QRectF(c.x() - 58, c.y() - 58, 116, 116), 26, 26);

    const double mark = answered ? Motion::ease((ms - 60) / Motion::Slow) : 0;
    const double waves = answered ? std::max(0.0, 1 - ms / Motion::Base) : 1;
    if (waves > 0.01) {
        const double speed = m_mode == Checking ? 1.7 : 0.5;
        for (int i = 0; i < 3; ++i) {
            QColor w = m_mode == Idle ? QColor("#c9b27c") : gold;
            w.setAlphaF(waves * (0.22 + 0.55 * std::pow(0.5 + 0.5 * std::sin((life * speed - i * 0.17) * 6.2832), 2)));
            p.setPen(QPen(w, 2.6, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            const double r = 12 + i * 11;
            p.drawArc(QRectF(c.x() - 12 - r, c.y() - 12 - r, r * 2, r * 2), -45 * 16, 90 * 16);
        }
    }
    if (mark > 0.01) {
        QPainterPath path;
        if (m_mode == Denied) {
            path.moveTo(c + QPointF(-17, -17));
            path.lineTo(c + QPointF(17, 17));
            path.moveTo(c + QPointF(17, -17));
            path.lineTo(c + QPointF(-17, 17));
        } else {
            path.moveTo(c + QPointF(-23, 2));
            path.lineTo(c + QPointF(-6, 19));
            path.lineTo(c + QPointF(25, -17));
        }
        QPen pen(tone, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        const double length = path.length() / pen.widthF();
        pen.setDashPattern({std::max(0.01, length * mark), length * 2 + 1});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    const double hide = m_mode == Granted ? std::max(0.0, 1 - ms / Motion::Slow) : 1;
    if (hide <= 0.01) return;
    const QPointF from(c.x() + 96, 168 + 3 * std::sin(life * 1.7)), to(c.x() + 6, c.y() + 30);
    p.setOpacity(hide);
    p.translate(from + (to - from) * card - QPointF(0, std::sin(card * 3.1416) * 14));
    p.rotate(-9 + 9 * card);
    const QRectF body(-38, -24, 76, 48);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 90));
    p.drawRoundedRect(body.translated(0, 4), 8, 8);
    QLinearGradient metal(body.topLeft(), body.bottomRight());
    metal.setColorAt(0, QColor("#bfa370"));
    metal.setColorAt(0.5, QColor("#f1e1b4"));
    metal.setColorAt(1, QColor("#c4a876"));
    p.setBrush(metal);
    p.drawRoundedRect(body, 8, 8);
    p.setBrush(QColor("#3a3427"));
    p.drawEllipse(QPointF(-19, -5), 7, 7);
    p.drawRoundedRect(QRectF(-5, -12, 32, 4.4), 2.2, 2.2);
    p.drawRoundedRect(QRectF(-5, -3, 22, 4.4), 2.2, 2.2);
    p.drawRoundedRect(QRectF(-27, 12, 54, 3.4), 1.7, 1.7);
}
