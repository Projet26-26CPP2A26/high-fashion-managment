#pragma once
#include "material.h"
#include <QByteArray>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <functional>

class DataFlow;
class LinkLight;
class QComboBox;
class QLabel;
class QTableWidget;

class LcdScreen : public QWidget
{
public:
    explicit LcdScreen(QWidget *parent = nullptr);
    void display(const QString &top, const QString &bottom, int state = 0);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_top, m_bottom;
    int m_state = 0;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

class BornePage : public QWidget
{
public:
    explicit BornePage(QWidget *parent = nullptr);
    std::function<QByteArray(const QString &)> ask;
    void setLink(bool on, const QString &text);
    void setMaterials(const QVector<Material> &materials);
    void exchange(const QString &source, const QString &request, const QByteArray &reply);

private:
    void key(QChar k);
    void send(const QString &message);
    void home();
    void step(const QString &text);

    LcdScreen *m_lcd;
    LinkLight *m_light;
    DataFlow *m_flow;
    QLabel *m_link;
    QTableWidget *m_feed;
    QString m_id, m_quantity, m_last;
    QLabel *m_step;
    QComboBox *m_pick;
    QChar m_operation;
};
