#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

class QDialog;
class QShortcut;
class QWidget;

class DialogSkin : public QObject {
    Q_OBJECT
public:
    static int run(QDialog &dialog);
    static bool ask(QWidget *parent, const QString &title, const QString &text,
        const QString &yes = QStringLiteral("Oui"), const QString &no = QStringLiteral("Non"));
    static void info(QWidget *parent, const QString &title, const QString &text);
    static void warn(QWidget *parent, const QString &title, const QString &text);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    DialogSkin(QDialog *dialog, QWidget *owner);
    ~DialogSkin() override;
    void place();
    void appear();
    void disappear();
    QDialog *m_dialog;
    QPointer<QWidget> m_owner;
    QPointer<QWidget> m_backdrop;
    QVector<QPointer<QShortcut>> m_shortcuts;
};
