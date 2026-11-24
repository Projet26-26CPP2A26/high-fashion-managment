#pragma once
#include "Motion.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <QWidget>

// Voyant de la liaison Arduino : un fil de points qui circule quand la carte est connectée.
class LinkLight : public QWidget {
public:
    explicit LinkLight(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(46, 16);
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
        const double t = m_clock.elapsed() / 1000.0;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 5; ++i) {
            QColor c(m_on ? "#7fc08a" : "#77746d");
            c.setAlphaF(m_on ? 0.25 + 0.75 * std::pow(0.5 + 0.5 * std::sin(t * 5 - i * 1.1), 3) : 0.5);
            p.setBrush(c);
            p.drawEllipse(QPointF(7 + i * 8, 8), 2.4, 2.4);
        }
    }

private:
    bool m_on = false;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
