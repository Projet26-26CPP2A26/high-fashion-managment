#pragma once
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

// Le QR se « coud » : des points dorés partent du bord, se posent un par un, puis une ligne le vérifie.
// Une fois posé, le QR est dessiné net (pixels entiers) pour rester lisible par un téléphone.
class QrAnimation : public QWidget {
    Q_OBJECT
public:
    explicit QrAnimation(QWidget *parent = nullptr);
    void setCode(const QImage &modules);   // image à 1 pixel par module
    void clear();
    void succeed(const QString &text);     // coche verte
    void refuse();                         // secousse rouge
    void replay();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override { replay(); }
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    enum Result { None, Ok, Refused };
    struct Stitch { QPoint cell; QPointF from; double delay; };
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
