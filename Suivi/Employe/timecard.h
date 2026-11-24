#pragma once
#include "AnimationSpeed.h"
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QWidget>
#include <functional>

class QTimer;

class TimeCard : public QWidget {
public:
    explicit TimeCard(QWidget *parent = nullptr);
    void setTimes(int work, int pause, bool paused, bool running);
    void setBack(bool back);
    std::function<void()> clicked;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    class Half {
    public:
        QString title, value, note;
        double ratio;
        bool alert;
    };
    void paintHalf(QPainter &p, const QRectF &box, const Half &half, bool pause, bool live) const;
    int m_work = 0, m_pause = 0;
    bool m_paused = false, m_running = true, m_hover = false;
    Motion::Tween m_flip;
    QTimer *m_timer;
};

class ActivityLog : public QWidget {
public:
    enum Kind { Presence, Pause, Task, Idle };
    explicit ActivityLog(QWidget *parent = nullptr);
    static Kind kindOf(const QString &text);
    static QString kindName(Kind kind);
    static QColor kindColor(Kind kind);
    void add(const QDateTime &at, const QString &text);
    void setFilter(int kind);
    int count(int kind) const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    class Row {
    public:
        QDateTime at;
        QString text;
        Kind kind;
    };
    void fit();
    static constexpr int RowHeight = 58;
    QVector<Row> m_rows;
    int m_filter = -1;
    Motion::Tween m_enter;
    QTimer *m_timer;
};
