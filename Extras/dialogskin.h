#pragma once
#include "LogoAnimation.h"
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>

// Messages de l'application : carte sombre, contour doré qui se dessine, icône animée.
class DialogSkin : public QDialog {
public:
    DialogSkin(QWidget *parent, LivingIcon::Kind kind, const QString &title, const QString &text, const QString &accept, const QString &reject = QString());
    static bool ask(QWidget *parent, const QString &title, const QString &text, const QString &accept = QStringLiteral("Confirmer"));
    static void warn(QWidget *parent, const QString &title, const QString &text);
    static void info(QWidget *parent, const QString &title, const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;
    void done(int result) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QWidget *m_shade = nullptr;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
