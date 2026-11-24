#pragma once
#include <QElapsedTimer>
#include <QWidget>

class QLabel;

class LivingIcon : public QWidget {
public:
    enum Kind { Question, Info, Alert, Check, Empty };
    LivingIcon(Kind kind, int size, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override
    {
        m_clock.restart();
    }

private:
    Kind m_kind;
    QElapsedTimer m_clock;
};

class LivingLogo : public QWidget {
public:
    static void attach(QLabel *logo);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *) override;

private:
    explicit LivingLogo(QLabel *logo);
    QLabel *m_logo;
    QElapsedTimer m_clock;
};
