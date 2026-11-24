#pragma once
#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;
class LoginScene;
class ErrorBanner;
class MorphOverlay;
class QPushButton;
class QVBoxLayout;
class BadgeAnimation;
class LinkLight;
class QComboBox;
class QStackedWidget;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    enum Role { None, Admin, EmployeeRole };
    explicit LoginDialog(QWidget *parent = nullptr);
    Role role() const
    {
        return m_role;
    }
    QString user() const;
    MorphOverlay *takeOverlay();
    void start();
    void arriveFrom(MorphOverlay *overlay);
    void reject() override;
    class BadgeResult {
    public:
        Role role = None;
        int employeeId = 0;
        QString name, reply;
    };
    std::function<BadgeResult(const QString &)> badgeHandler;
    std::function<QVector<QPair<QString, QString>>()> badgeList;
    std::function<void(const QString &)> refusedHandler;
    int employeeId() const
    {
        return m_employeeId;
    }
    bool usedBadge() const
    {
        return m_usedBadge;
    }
    QString scanBadge(const QString &line, bool board = false);
    void answerBadge(const BadgeResult &result);
    void showBadge(bool on);
    ~LoginDialog() override;
    static Role authenticate(const QString &user, const QString &password);

private:
    void submit();
    void leave(int delay);
    QWidget *createBadgePanel();
    void showLink();
    void listen();
    QLineEdit *m_user, *m_password;
    QPushButton *m_button;
    void markFields(bool wrong);
    ErrorBanner *m_error;
    LoginScene *m_scene;
    QList<QWidget *> m_lines;
    QVBoxLayout *m_form = nullptr;
    MorphOverlay *m_overlay = nullptr;
    Role m_role = None;
    int m_failures = 0;
    bool m_leaving = false, m_usedBadge = false, m_listed = false;
    int m_employeeId = 0;
    QStackedWidget *m_pages;
    BadgeAnimation *m_badge;
    LinkLight *m_light;
    QLabel *m_badgeTitle, *m_badgeText, *m_linkText;
    QComboBox *m_badges;
    QPushButton *m_scanButton;
};
