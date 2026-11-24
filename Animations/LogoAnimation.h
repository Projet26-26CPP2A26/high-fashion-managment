#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QLabel;

// Logo vivant : un reflet passe et une étincelle tourne autour.
class LivingLogo : public QWidget {
public:
    explicit LivingLogo(QLabel *logo);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap m_logo;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

// Icône animée des messages.
class LivingIcon : public QWidget {
public:
    enum Kind { Question, Info, Alert, Error, Success };
    explicit LivingIcon(Kind kind, int size = 76, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Kind m_kind;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

// Bobine de fil qui tourne doucement (bas du menu).
class SpoolAnimation : public QWidget {
public:
    explicit SpoolAnimation(QWidget *parent = nullptr);
    void hurry() { m_boost.start(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QElapsedTimer m_clock, m_boost;
    QTimer m_timer;
    double m_turn = 0, m_last = 0;
};
