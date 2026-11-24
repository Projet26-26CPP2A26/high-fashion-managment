#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

// Guide du réapprovisionnement : 1 Choisir, 2 Commander, 3 Recevoir.
class OrderAnimation : public QWidget {
public:
    explicit OrderAnimation(QWidget *parent = nullptr);
    void setStep(int step);   // 0 = rien à commander, 1, 2, 3

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_step = 0;
    double m_position = 0;
    QElapsedTimer m_life;
    QTimer m_timer;
};
