#pragma once
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QrAnimation : public QWidget
{
    Q_OBJECT
public:
    explicit QrAnimation(QWidget *parent = nullptr);
    void setCode(const QImage &modules);
    void clear();
    void succeed(const QString &text);
    void refuse();
    void replay();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override { replay(); }
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    enum Result { None, Ok, Refused };
    class Stitch
    {
    public:
        Stitch(const QPoint &cell, const QPointF &from, double delay) : m_cell(cell), m_from(from), m_delay(delay) {}
        QPoint cell() const { return m_cell; }
        QPointF from() const { return m_from; }
        double delay() const { return m_delay; }

    private:
        QPoint m_cell;
        QPointF m_from;
        double m_delay;
    };
    void render();
    QRectF codeRect() const;

    QImage m_modules;
    QPixmap m_final;
    QVector<Stitch> m_stitches;
    QElapsedTimer m_clock, m_resultClock;
    QTimer m_timer;
    Result m_result = None;
    QString m_text;
};
