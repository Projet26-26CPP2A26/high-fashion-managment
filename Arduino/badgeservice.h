#pragma once
#include <QDateTime>
#include <QObject>
#include <QVector>

class BadgeScan {
public:
    QDateTime at;
    QString uid, name, kind;
};

class BadgeService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    static QString normalize(const QString &uid);
    static QString plain(const QString &text, int length);
    bool ready();
    QString error() const { return m_error; }
    QString receive(const QString &line);
    QString enter(const QString &uid);
    int employeeOf(const QString &uid);
    bool inside(int employeeId);
    QString adminUid;
    bool assign(const QString &uid, int employeeId);
    bool release(int employeeId);
    QString badgeOf(int employeeId);
    QVector<QPair<QString, QString>> badges();
    QVector<BadgeScan> scans(int limit = 60);
    QString lastUid() const { return m_lastUid; }

signals:
    void scanned(int employeeId, bool arrival);
    void changed();
    void refused(const QString &uid);
    void adminScanned();

private:
    QString scan(const QString &uid);
    bool insert(const QString &uid, int employeeId, const QString &kind);
    int workedMinutes(int employeeId);
    bool fail(const QString &text);
    QString m_error, m_lastUid;
};
