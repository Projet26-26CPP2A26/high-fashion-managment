#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

// Ouverture : une aiguille coud un cercle autour du logo, puis le tissu s'ouvre comme une fermeture éclair.
// Fermeture : les deux pans se referment et l'aiguille coud une dernière ligne.
class StitchAnimation : public QWidget {
    Q_OBJECT
public:
    enum Mode { Opening, Closing };
    StitchAnimation(Mode mode, QWidget *window);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override { finish(); }
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void finish();
    void curtains(QPainter &p, double open);
    void emblem(QPainter &p, double sewn, double words);

    Mode m_mode;
    bool m_done = false;
    QPixmap m_logo;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
