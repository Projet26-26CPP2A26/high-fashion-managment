#pragma once
#include <QElapsedTimer>
#include <QPair>
#include <QTimer>
#include <QVector>
#include <QWidget>

// Graphique animé : les barres poussent l'une après l'autre, un reflet les traverse.
class ChartAnimation : public QWidget {
public:
    explicit ChartAnimation(QWidget *parent);
    void setData(const QVector<QPair<QString, double>> &data);
    void replay();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QVector<QPair<QString, double>> m_data;
    QElapsedTimer m_clock;
    QTimer m_timer;
    int m_hover = -1;
};
