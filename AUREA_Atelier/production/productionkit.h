// Petits composants visuels : icônes vectorielles et graphiques dorés (style AUREA).
#pragma once
#include <QColor>
#include <QIcon>
#include <QPair>
#include <QPixmap>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace Kit {
inline const QColor gold("#e2cf9f"), gold2("#c9b27c"), text("#f3f0ea"), muted("#8f8c86"),
    ok("#7fc08a"), warn("#e2b25c"), crit("#e0807a"), track("#3b3b40"), line("#323236");
enum class Icon { Grid, Tasks, Calendar, Trend, Chart, Help, Search, Alert, Clock, Check, List, Chevron };
QPixmap pixmap(Icon icon, int px, const QColor &color);
QIcon navIcon(Icon icon, const QColor &normal, const QColor &active);  // état « coché » = couleur active
}

class AreaChart : public QWidget {
public:
    explicit AreaChart(QWidget *parent = nullptr) : QWidget(parent) { setMinimumHeight(230); }
    void setData(const QVector<double> &values, const QStringList &labels, const QString &unit = {});
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<double> vals;
    QStringList labs;
    QString suffix;
};

class BarChart : public QWidget {
public:
    explicit BarChart(QWidget *parent = nullptr) : QWidget(parent) {}
    void setData(const QVector<QPair<QString, double>> &rows, const QString &unit = {}, int decimals = 0);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<QPair<QString, double>> data;
    QString suffix;
    int dec = 0;
};
