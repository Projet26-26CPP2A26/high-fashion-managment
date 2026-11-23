#pragma once
#include "spectacle.h"
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QVector>
#include <QWidget>
#include <functional>

class QPainter;

class MorphOverlay : public QWidget {
    Q_OBJECT
public:

    static constexpr double Expand = 0.60, Message = 0.85, Land = 0.55, Close = 1.96;

    using LogoSwitch = std::function<void(bool)>;
    MorphOverlay(QWidget *from, const QRect &logo, const QRect &target, Spectacle::Mode mode, const LogoSwitch &logoVisible);
    void setMessage(const QString &title, const QString &subtitle, double duration = Message);
    void land(std::function<QRect()> logo, LogoSwitch logoVisible);
    void waitExpanded();
    void waitMessage();
    void waitClosed();
    static void breathe();
    static void warmUp();
    static QRect areaFor(QWidget *window);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override { m_skip = true; }
    void keyPressEvent(QKeyEvent *) override { m_skip = true; }

private:
    void wait(const std::function<bool()> &done);
    double seconds() const { return m_time; }
    void advance();
    static const QVector<QPixmap> &logos();
    bool messageOver() const;
    void paintClosing(QPainter &p);
    void drawLogo(class QPainter &p, const QRectF &rect) const;
    Spectacle m_show;
    Spectacle::Mode m_mode;
    QPixmap m_shot, m_dim;
    QImage m_soft, m_feather, m_buffer;
    QRectF m_start, m_target, m_logoStart, m_logoEnd;
    QString m_title, m_subtitle;
    QElapsedTimer m_clock;
    double m_time = 0, m_duration = Message;
    qint64 m_lastFrame = -1;
    double m_messageAt = -1, m_landAt = -1;
    bool m_skip = false;
};
