#pragma once
#include "UiAnimations.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <cmath>

class LinkLight : public QWidget {
public:
    enum State { Off, Waiting, On, Error };

    explicit LinkLight(QWidget *parent = nullptr);
    void setState(State state);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QColor color() const;
    State m_state = Off;
    QColor m_before = QColor("#77746e");
    Motion::Tween m_change;
    QElapsedTimer m_clock;
};
