#pragma once
#include <QDialog>

class QLabel;
class QLineEdit;
class LoginScene;
class MorphOverlay;
class QPushButton;
class QVBoxLayout;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    enum Role { None, Admin, EmployeeRole };
    explicit LoginDialog(QWidget *parent = nullptr);
    Role role() const { return m_role; }
    QString user() const;
    MorphOverlay *takeOverlay();
    void start();
    void arriveFrom(MorphOverlay *overlay);
    void reject() override;
    static Role authenticate(const QString &user, const QString &password);

private:
    void submit();
    QLineEdit *m_user, *m_password;
    QPushButton *m_button;
    QLabel *m_error;
    LoginScene *m_scene;
    QList<QWidget *> m_lines;
    QVBoxLayout *m_form = nullptr;
    MorphOverlay *m_overlay = nullptr;
    Role m_role = None;
    int m_failures = 0;
    bool m_leaving = false;
};
