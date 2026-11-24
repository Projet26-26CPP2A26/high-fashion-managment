#pragma once
#include "AnimationSpeed.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QWidget>

class LinkLight : public QWidget
{
public:
    explicit LinkLight(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(24, 22);
        m_clock.start();
        m_timer.setInterval(33);
        connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    }
    void setOn(bool on)
    {
        m_on = on;
        on ? m_timer.start() : m_timer.stop();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(12, 11);
        QColor tone(m_on ? "#7fc08a" : "#77746d");
        for (int i = 0; m_on && i < 2; ++i) {
            const double wave = std::fmod(m_clock.elapsed() / 2000.0 + i * 0.5, 1.0);
            QColor ring = tone;
            ring.setAlphaF(0.6 * (1 - wave));
            p.setPen(QPen(ring, 1.3));
            p.drawEllipse(c, 4 + 6 * Motion::soft(wave), 4 + 6 * Motion::soft(wave));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(tone);
        p.drawEllipse(c, 3.6, 3.6);
    }

private:
    bool m_on = false;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

class DataFlow : public QWidget
{
public:
    explicit DataFlow(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(112);
        m_life.start();
        connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
        m_timer.start(Motion::frame(this));
    }
    void send(int state)
    {
        m_state = state;
        m_clock.start();
    }
    void setOn(bool on) { m_on = on; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double t = m_life.elapsed() / 1000.0, go = m_clock.isValid() ? m_clock.elapsed() / 1500.0 : 9;
        const QColor gold("#e2cf9f"), answer(m_state == 1 ? "#e6ce89" : m_state == 2 ? "#e08f8f" : "#7fc08a");
        const QPointF a(58, 44), b(width() / 2.0, 44), c(width() - 58.0, 44);
        QPainterPath wire(a);
        for (int i = 1; i <= 100; ++i) {
            const double u = i / 100.0;
            wire.lineTo(Motion::mix(a.x(), c.x(), u), 44 + 5 * std::sin(u * 14 - t * 2) * std::sin(u * Motion::Pi * 2));
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#5d533a"), 1.6));
        p.drawPath(wire);
        p.setPen(Qt::NoPen);
        for (int i = 0; m_on && i < 4; ++i) {
            const double u = std::fmod(t * 0.14 + i / 4.0, 1.0);
            QColor dot = gold;
            dot.setAlphaF(0.55 * std::sin(u * Motion::Pi));
            p.setBrush(dot);
            p.drawEllipse(wire.pointAtPercent(u), 1.8, 1.8);
        }
        if (go < 1) {
            const bool back = go > 0.5;
            const double u = back ? 1 - Motion::both((go - 0.5) * 2) : Motion::both(go * 2);
            for (int i = 0; i < 14; ++i) {
                const double v = std::clamp(u + (back ? 1 : -1) * i * 0.012, 0.0, 1.0);
                QColor dot = back ? answer : gold;
                dot.setAlphaF(std::pow(1 - i / 14.0, 2));
                p.setBrush(dot);
                p.drawEllipse(wire.pointAtPercent(v), 5 - i * 0.28, 5 - i * 0.28);
            }
        }
        node(p, a, "Borne", go < 1.2 ? std::max(0.0, 1 - std::abs(go - 1) * 5) : 0, answer);
        node(p, b, "Qt",
             go < 1 ? std::max(std::max(0.0, 1 - std::abs(go - 0.25) * 7), std::max(0.0, 1 - std::abs(go - 0.75) * 7))
                    : 0,
             gold);
        node(p, c, "Oracle", go < 1 ? std::max(0.0, 1 - std::abs(go - 0.5) * 5) : 0, answer);
    }

private:
    void node(QPainter &p, const QPointF &c, const QString &name, double light, const QColor &tone)
    {
        const double r = 23 + 3 * light;
        for (int i = 3; i > 0 && light > 0; --i) {
            QColor halo = tone;
            halo.setAlphaF(0.12 * light);
            p.setPen(QPen(halo, i * 5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
        }
        p.setPen(QPen(light > 0.05 ? tone : QColor("#c9b27c"), 1.5));
        p.setBrush(QColor("#2a2a2d"));
        p.drawEllipse(c, r, r);
        p.setPen(QPen(QColor("#e9d8a8"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if (name == "Borne") {
            p.drawRoundedRect(QRectF(c.x() - 9, c.y() - 11, 18, 22), 3, 3);
            p.drawLine(c + QPointF(-5, -6), c + QPointF(5, -6));
            for (int i = 0; i < 6; ++i)
                p.drawPoint(c + QPointF(-5 + 5 * (i % 3), 1 + 5 * (i / 3)));
        } else if (name == "Qt") {
            p.drawRoundedRect(QRectF(c.x() - 11, c.y() - 9, 22, 16), 2, 2);
            p.drawLine(c + QPointF(-5, 11), c + QPointF(5, 11));
            p.drawLine(c + QPointF(0, 7), c + QPointF(0, 11));
        } else {
            p.drawEllipse(QRectF(c.x() - 10, c.y() - 12, 20, 7));
            p.drawArc(QRectF(c.x() - 10, c.y() - 4, 20, 7), 180 * 16, 180 * 16);
            p.drawArc(QRectF(c.x() - 10, c.y() + 4, 20, 7), 180 * 16, 180 * 16);
            p.drawLine(c + QPointF(-10, -8), c + QPointF(-10, 8));
            p.drawLine(c + QPointF(10, -8), c + QPointF(10, 8));
        }
        p.setPen(QColor("#b9b4a8"));
        p.drawText(QRectF(c.x() - 50, c.y() + 31, 100, 20), Qt::AlignCenter, name);
    }
    int m_state = 0;
    bool m_on = false;
    QElapsedTimer m_life, m_clock;
    QTimer m_timer;
};
