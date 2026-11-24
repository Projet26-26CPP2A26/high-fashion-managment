#pragma once
#include "material.h"
#include <QElapsedTimer>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <functional>

class FieldGlow : public QWidget
{
public:
    FieldGlow(QWidget *field, std::function<bool()> filled);
    bool filled() const { return m_filled(); }
    void refresh();

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void place();
    QWidget *m_field;
    std::function<bool()> m_filled;
    double m_focus = 0, m_focusTo = 0, m_check = 0, m_checkTo = 0;
    QTimer m_timer;
};

class FormAnimation : public QWidget
{
public:
    explicit FormAnimation(QWidget *parent = nullptr);
    void setFields(const QVector<FieldGlow *> &fields) { m_fields = fields; }
    void show(const Material &material);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Material m_data;
    QVector<FieldGlow *> m_fields;
    QPixmap m_icon;
    double m_progress = 0, m_progressTo = 0, m_level = 0, m_levelTo = 0, m_value = 0, m_valueTo = 0;
    QElapsedTimer m_pop, m_life;
    QTimer m_timer;
};
