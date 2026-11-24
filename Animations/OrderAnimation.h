#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class OrderAnimation : public QWidget
{
public:
    explicit OrderAnimation(QWidget *parent = nullptr);
    void setStep(int step);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_step = 0;
    double m_position = 0;
    QElapsedTimer m_life;
    QTimer m_timer;
};
