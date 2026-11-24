#include "ConnectionAnimation.h"

LinkLight::LinkLight(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(22, 22);
    m_clock.start();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisible() && (m_state == Waiting || m_state == On || m_change.running())) {
            update();
        }
    });
    timer->start(Fx::ambientInterval(this));
}

void LinkLight::setState(State state)
{
    if (state == m_state) {
        return;
    }
    m_before = color();
    m_state = state;
    m_change.jump(0);
    m_change.go(1, Motion::Base);
    m_clock.restart();
    update();
    QTimer::singleShot(Motion::Base + 20, this, [this] {
        update();
    });
}

void LinkLight::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(11, 11);
    const double mix = m_change.value(), t = m_clock.elapsed() / 1000.0;
    const QColor a = m_before, b = color();
    const QColor tone(int(a.red() + (b.red() - a.red()) * mix), int(a.green() + (b.green() - a.green()) * mix),
        int(a.blue() + (b.blue() - a.blue()) * mix));
    if (m_state == On) {
        const double wave = std::fmod(t, 1.8) / 1.8;
        QColor ring = tone;
        ring.setAlphaF(0.55 * (1 - wave));
        p.setPen(QPen(ring, 1.4));
        p.drawEllipse(c, 4 + 6 * Motion::ease(wave), 4 + 6 * Motion::ease(wave));
    } else if (m_state == Waiting) {
        p.setPen(QPen(tone, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(3, 3, 16, 16), int(-t * 360 * 16) % 5760, 100 * 16);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(tone);
    p.drawEllipse(c, 4, 4);
}

QColor LinkLight::color() const
{
    return QColor(m_state == On  ? "#7fc08a"
            : m_state == Waiting ? "#e2cf9f"
            : m_state == Error   ? "#e08f8f"
                                 : "#77746e");
}
