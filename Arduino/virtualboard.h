#pragma once
#include "AnimationSpeed.h"
#include <QElapsedTimer>
#include <QVector>
#include <QWidget>
#include <functional>

class QComboBox;
class QTimer;
class QPainter;

class VirtualBoard : public QWidget {
public:
    explicit VirtualBoard(QWidget *parent = nullptr);
    void setBadges(const QVector<QPair<QString, QString>> &badges);
    void present(const QString &uid);
    void reply(const QString &line, bool sound = true);
    std::function<void(const QString &)> scan;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void display(const QString &top, const QString &bottom);
    QComboBox *m_badges;
    QString m_top, m_bottom;
    enum Mode { Idle, Accepted, Refused, Known, Checking };
    void tick();
    void pace();
    QTimer *m_timer;
    void paintScreen(QPainter &p, const QColor &tone);
    void paintDoor(QPainter &p);
    void paintReader(QPainter &p, const QColor &tone);
    void paintCard(QPainter &p);
    Mode m_mode = Idle;
    Motion::Tween m_door, m_card, m_text, m_tone;
    QElapsedTimer m_clock, m_life;
};
