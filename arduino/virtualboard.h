#pragma once
#include <QElapsedTimer>
#include <QVector>
#include <QWidget>
#include <functional>

class QComboBox;

class VirtualBoard : public QWidget {
public:
    explicit VirtualBoard(QWidget *parent = nullptr);
    void setBadges(const QVector<QPair<QString, QString>> &badges);
    void reply(const QString &line);
    std::function<void(const QString &)> scan;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void display(const QString &top, const QString &bottom);
    QComboBox *m_badges;
    QString m_top, m_bottom;
    int m_mode = 0;
    double m_door = 0;
    QElapsedTimer m_clock;
};
