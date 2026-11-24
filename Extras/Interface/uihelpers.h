#pragma once
#include "dialogskin.h"
#include "employee.h"
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QLinearGradient>
#include <QGridLayout>
#include <QLocale>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace UiHelpers {
QString duration(int minutes);
QString hm(const QTime &time);
QString availability(const Employee &e);
QString archived(const QString &name, int id);
void setupTable(QTableWidget *t, const QList<int> &widths);
QColor statusColor(const QString &text);
void addRow(QTableWidget *t, const QStringList &values, int id = 0);
QString initials(const QString &name);
class BarRow {
public:
    BarRow(const QString &label, double value, const QString &text, bool high)
        : m_label(label)
        , m_value(value)
        , m_text(text)
        , m_high(high)
    {
    }
    QString label() const
    {
        return m_label;
    }
    double value() const
    {
        return m_value;
    }
    QString text() const
    {
        return m_text;
    }
    bool high() const
    {
        return m_high;
    }

private:
    QString m_label;
    double m_value;
    QString m_text;
    bool m_high;
};

QPixmap barsImage(const QVector<BarRow> &rows, double maxValue, int width, int labelWidth, qreal ratio);
QPixmap hoursChartImage(
    const QVector<double> &values, const QStringList &labels, int width, int height, qreal ratio);
bool confirm(QWidget *parent, const QString &text);
QString document(const QString &title, const QString &body);
QString headers(const QStringList &values);
QString htmlRow(const QStringList &values);
}
