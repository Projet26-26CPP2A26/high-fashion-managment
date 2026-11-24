#pragma once
#include "AnimationSpeed.h"
#include <QElapsedTimer>
#include <QWidget>

class BadgeAnimation : public QWidget {
public:
    enum Mode { Idle, Checking, Granted, Denied };

    explicit BadgeAnimation(QWidget *parent = nullptr);
    void present();
    void answer(bool granted);
    Mode mode() const
    {
        return m_mode;
    }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Mode m_mode = Idle;
    Motion::Tween m_card, m_tone;
    QElapsedTimer m_life, m_clock;
};
