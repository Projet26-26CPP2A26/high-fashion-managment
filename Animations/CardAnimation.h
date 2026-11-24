#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class PopIn : public QWidget
{
public:
    static void play(QWidget *target, int delay);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    PopIn(QWidget *target, int delay);
    QPixmap m_shot;
    QColor m_back = QColor("#1b1b1d");
    QRect m_home;
    int m_delay;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

class CardGlow : public QWidget
{
public:
    explicit CardGlow(QWidget *card);

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    double m_level = 0, m_target = 0;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
