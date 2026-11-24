#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

// Fermeture : la fenêtre se replie dans le logo, les fils l'entourent, puis tout s'éteint.
class ClosingAnimation : public QWidget {
    Q_OBJECT
public:
    explicit ClosingAnimation(QWidget *window);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override { finish(); }

private:
    void finish();
    QPixmap m_shot, m_logo;
    QElapsedTimer m_clock;
    QPixmap m_back;
    QTimer m_timer;
    bool m_done = false;
};
