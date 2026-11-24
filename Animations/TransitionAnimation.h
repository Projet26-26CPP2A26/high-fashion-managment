#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QLabel;

// Ouverture : les fils de soie s'enroulent autour du logo, puis le logo rejoint le menu
// pendant que l'application apparaît dans un cercle de lumière.
class TransitionAnimation : public QWidget {
    Q_OBJECT
public:
    TransitionAnimation(QWidget *window, QLabel *logo, const QString &title, const QString &subtitle);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override { finish(); }
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void finish();
    QLabel *m_logo;
    QString m_title, m_subtitle;
    QPixmap m_big, m_shot;
    QElapsedTimer m_clock;
    QPixmap m_back;
    QTimer m_timer;
    bool m_done = false;
};
