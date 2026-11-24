#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <QLabel>

// Ouverture : le logo surgit en profondeur, l'application avance derrière lui,
// puis ce même logo va se poser à sa place dans le menu (un seul logo à l'écran).
class TransitionAnimation : public QWidget {
    Q_OBJECT
public:
    TransitionAnimation(QWidget *window, QLabel *logo, const QString &title, const QString &subtitle);
    ~TransitionAnimation() override;

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override { finish(); }
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void finish();
    void restoreLogo();
    QPointer<QLabel> m_logo;
    QString m_title, m_subtitle;
    QPixmap m_menuLogo, m_big, m_shot, m_back;
    QElapsedTimer m_clock;
    QTimer m_timer;
    bool m_done = false;
};
