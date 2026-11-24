#pragma once
#include "AnimationSpeed.h"
#include <QElapsedTimer>
#include <QString>
#include <QWidget>
#include <functional>

class QTimer;

class ErrorBanner : public QWidget {
public:
    explicit ErrorBanner(QWidget *parent = nullptr);
    void showError(const QString &title, const QString &text);
    void lock(int seconds);
    void clear();
    bool locked() const
    {
        return m_locked > 0;
    }
    std::function<void()> unlocked;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void open();
    void tick();
    double remaining() const;
    void paintWarning(QPainter &p, const QPointF &center, double grow) const;
    void paintCountdown(QPainter &p, const QPointF &center) const;
    static constexpr int Closed = 20, Opened = 48;
    QString m_title, m_text;
    int m_locked = 0;
    bool m_shown = false;
    QElapsedTimer m_clock;
    Motion::Tween m_open, m_pulse, m_icon, m_words;
    QTimer *m_timer;
};
