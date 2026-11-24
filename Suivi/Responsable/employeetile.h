#pragma once
#include "activitystore.h"
#include <QColor>
#include <QElapsedTimer>
#include <QVariantAnimation>
#include <QWidget>
#include <functional>

class QTimer;

QColor statusColor(const QString &status);
QStringList sessionAlerts(const WorkSession &session);

class EmployeeTile : public QWidget {
public:
    explicit EmployeeTile(QWidget *parent = nullptr);
    void setSession(const WorkSession &session, bool selected);
    void enter(int delay);
    void setPulse(qreal phase)
    {
        if (m_status != QStringLiteral("Actif")) {
            return;
        }
        m_pulse = phase;
        update(m_avatar);
    }
    std::function<void()> clicked;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override
    {
        animateHover(1);
    }
    void leaveEvent(QEvent *) override
    {
        animateHover(0);
    }
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void animateHover(qreal to);
    WorkSession m_session;
    QString m_status;
    QStringList m_alerts;
    QRect m_avatar;
    bool m_selected = false, m_first = true;
    qreal m_enter = 1, m_hover = 0, m_pulse = 0, m_progress = 0, m_day = 0, m_flash = 0;
    QVariantAnimation m_hoverAnimation, m_barAnimation, m_flashAnimation;
    qreal m_fromProgress = 0, m_fromDay = 0, m_toProgress = 0, m_toDay = 0;
};

class StarRating : public QWidget {
public:
    explicit StarRating(QWidget *parent = nullptr);
    int value() const
    {
        return m_value;
    }
    void setValue(int v);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *) override;

private:
    void step();
    int starAt(double x) const;
    int m_value = 4, m_hover = 0, m_clicked = -1;
    double m_fill[5] = {}, m_scale[5] = {1, 1, 1, 1, 1}, m_ring = 1;
    QElapsedTimer m_clock, m_frame;
    QTimer *m_timer;
};

class DayTimeline : public QWidget {
public:
    explicit DayTimeline(QWidget *parent = nullptr);
    void add(const QString &state, int seconds);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<QPair<QString, int>> m_parts;
};

class ProgressRing : public QWidget {
public:
    explicit ProgressRing(int size, QWidget *parent = nullptr);
    void setValue(qreal ratio, const QString &text, const QString &caption);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    qreal m_value = 0, m_target = 0;
    QString m_text, m_caption;
    QVariantAnimation m_animation;
};
