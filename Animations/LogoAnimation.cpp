#include "LogoAnimation.h"
#include "AnimationSpeed.h"
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

LivingLogo::LivingLogo(QLabel *logo) : QWidget(logo), m_logo(":/assets/logo_aurea.png")
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(logo->rect());
    resize(48, 56);
    m_logo = m_logo.scaled(QSize(44, 44) * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_logo.setDevicePixelRatio(devicePixelRatioF());
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_timer.start(33);
}

void LivingLogo::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double t = m_clock.elapsed() / 1000.0;
    const QSizeF size = m_logo.deviceIndependentSize();
    const QPointF c(width() / 2.0, height() / 2.0);
    const QRectF at(c - QPointF(size.width() / 2, size.height() / 2), size);
    p.drawPixmap(at.topLeft(), m_logo);
    QPainterPath disc;
    disc.addEllipse(c, 17, 17);
    const double x = std::fmod(t, 5.0) / 5 * 6 - 1.5;
    if (x < 2) {
        p.save();
        p.setClipPath(disc);
        QLinearGradient shine(c + QPointF((x - 0.4) * 22, -20), c + QPointF((x + 0.4) * 22, -6));
        shine.setColorAt(0, QColor(255, 246, 214, 0));
        shine.setColorAt(0.5, QColor(255, 246, 214, 110));
        shine.setColorAt(1, QColor(255, 246, 214, 0));
        p.fillRect(rect(), shine);
        p.restore();
    }
    for (int i = 0; i < 12; ++i) {            // étincelle et sa traîne
        const double a = t * 1.1 - i * 0.07;
        QColor spark(255, 240, 200);
        spark.setAlphaF(0.85 * std::pow(1 - i / 12.0, 2));
        p.setPen(Qt::NoPen);
        p.setBrush(spark);
        p.drawEllipse(c + QPointF(std::cos(a), std::sin(a)) * 21, 1.5 - i * 0.09, 1.5 - i * 0.09);
    }
}

LivingIcon::LivingIcon(Kind kind, int size, QWidget *parent) : QWidget(parent), m_kind(kind)
{
    setFixedSize(size, size);
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_timer.start(Motion::frame(this));
}

void LivingIcon::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double ms = m_clock.elapsed(), t = ms / 1000, k = width() / 76.0;
    const QColor tone(m_kind == Alert ? "#e6ce89" : m_kind == Error ? "#e08f8f" : m_kind == Success ? "#7fc08a" : "#e2cf9f");
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(k, k);
    QRadialGradient glow(0, 0, 38);
    QColor soft = tone;
    soft.setAlpha(int(40 + 18 * std::sin(t * 2.4)));
    glow.setColorAt(0, soft);
    soft.setAlpha(0);
    glow.setColorAt(1, soft);
    p.fillRect(QRectF(-38, -38, 76, 76), glow);
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {             // anneaux inclinés qui tournent
        p.save();
        p.rotate(i * 60 + t * (i % 2 ? -32 : 24));
        QColor ring = tone;
        ring.setAlphaF((0.6 - 0.13 * i) * Motion::soft(ms / 600));
        p.setPen(QPen(ring, 1.3, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(-32, -20, 64, 40), int((t * 70 + i * 90) * 16), int(230 * 16 * Motion::soft(ms / 900)));
        p.restore();
    }
    const double grow = Motion::spring(Motion::part(ms, 60, 520));
    p.scale(grow, grow);
    soft = tone;
    soft.setAlpha(34);
    p.setBrush(QColor("#232326"));
    p.setPen(QPen(tone, 1.6));
    p.drawEllipse(QPointF(0, 0), 18, 18);
    p.setBrush(soft);
    p.drawEllipse(QPointF(0, 0), 18, 18);
    QPainterPath mark;
    if (m_kind == Success) { mark.moveTo(-8, 1); mark.lineTo(-2, 7); mark.lineTo(9, -6); }
    else if (m_kind == Error) { mark.moveTo(-6, -6); mark.lineTo(6, 6); mark.moveTo(6, -6); mark.lineTo(-6, 6); }
    QPen pen(tone, 3, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
    if (!mark.isEmpty()) {
        const double length = mark.length() / 3;
        pen.setDashPattern({std::max(0.01, length * Motion::soft(Motion::part(ms, 260, 700))), length * 2 + 1});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(mark);
    } else {
        QFont font("Inter");
        font.setPixelSize(20);
        font.setBold(true);
        p.setFont(font);
        p.setPen(tone);
        p.setOpacity(Motion::soft(Motion::part(ms, 220, 600)));
        p.drawText(QRectF(-18, -18, 36, 36), Qt::AlignCenter, m_kind == Question ? "?" : m_kind == Alert ? "!" : "i");
    }
}

SpoolAnimation::SpoolAnimation(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(84);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_timer.start(33);
}

void SpoolAnimation::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double t = m_clock.elapsed() / 1000.0;
    const double boost = m_boost.isValid() ? 6 * (1 - Motion::soft(m_boost.elapsed() / 1400.0)) : 0;
    m_turn += (t - m_last) * (1.2 + boost);
    m_last = t;
    const QRectF body(16, 22, 30, 42);
    p.setPen(Qt::NoPen);
    QLinearGradient metal(body.left() - 6, 0, body.right() + 6, 0);
    metal.setColorAt(0, QColor("#8f7434"));
    metal.setColorAt(0.5, QColor("#f1e1b4"));
    metal.setColorAt(1, QColor("#8f7434"));
    QLinearGradient silk(body.left(), 0, body.right(), 0);
    silk.setColorAt(0, QColor("#3a3427"));
    silk.setColorAt(0.45, QColor("#8a774d"));
    silk.setColorAt(1, QColor("#2c281e"));
    p.setBrush(silk);
    p.drawRect(body);
    p.save();
    p.setClipRect(body);
    for (int i = -2; i < 13; ++i) {           // spires qui défilent
        const double y = body.top() + std::fmod(i * 4.2 + m_turn * 6, 13 * 4.2) - 4;
        p.setPen(QPen(QColor(241, 225, 180, 120), 1.1));
        p.drawLine(QPointF(body.left(), y), QPointF(body.right(), y + 5));
    }
    p.restore();
    p.setPen(Qt::NoPen);
    p.setBrush(metal);
    p.drawRoundedRect(QRectF(body.left() - 6, body.top() - 7, body.width() + 12, 7), 3, 3);
    p.drawRoundedRect(QRectF(body.left() - 6, body.bottom(), body.width() + 12, 7), 3, 3);
    QPainterPath thread(QPointF(body.right(), 46));
    for (int i = 1; i <= 60; ++i) {
        const double u = i / 60.0, x = body.right() + u * (width() - body.right() - 22);
        thread.lineTo(x, 46 + std::sin(u * 9 - m_turn * 1.7) * 13 * u * (1.1 - 0.5 * u));
    }
    QLinearGradient fade(body.right(), 0, width() - 20, 0);
    fade.setColorAt(0, QColor(226, 207, 159, 220));
    fade.setColorAt(1, QColor(226, 207, 159, 40));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(fade), 1.5, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(thread);
    for (int i = 0; i < 2; ++i) {             // reflets qui glissent sur le fil
        const double u = std::fmod(m_turn * 0.16 + i * 0.5, 1.0);
        QColor c(255, 246, 220);
        c.setAlphaF(std::sin(u * Motion::Pi));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(thread.pointAtPercent(u), 2, 2);
    }
}
