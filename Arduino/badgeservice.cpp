#include "badgeservice.h"
#include "oracleconnection.h"
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

QString BadgeService::normalize(const QString &uid)
{
    QString out = uid.trimmed().toUpper();
    out.remove(QRegularExpression("[\\s:-]"));
    static const QRegularExpression valid("^[0-9A-F]{4,20}$");
    return valid.match(out).hasMatch() ? out : QString();
}

QString BadgeService::plain(const QString &text, int length)
{
    QString out;
    for (const QChar c : text.normalized(QString::NormalizationForm_D)) {
        if (c.unicode() >= 32 && c.unicode() < 127 && c != QChar(';')) {
            out += c;
        }
    }
    return out.left(length);
}

bool BadgeService::fail(const QString &text)
{
    m_error = text;
    return false;
}

bool BadgeService::ready()
{
    QSqlQuery q;
    if (!q.exec("SELECT COUNT(*) FROM employee_badges") || !q.exec("SELECT COUNT(*) FROM badge_scans")) {
        return fail(q.lastError().text());
    }
    m_error.clear();
    return true;
}

QString BadgeService::receive(const QString &line)
{
    const QString text = line.trimmed();
    if (!text.startsWith("BADGE:")) {
        return QString();
    }
    const QString uid = normalize(text.mid(6));
    if (uid.isEmpty()) {
        emit refused(QString());
        return QStringLiteral("NO;Badge illisible");
    }
    m_lastUid = uid;
    const QString reply = scan(uid);
    emit changed();
    return reply;
}

int BadgeService::employeeOf(const QString &uid)
{
    QSqlQuery q;
    q.prepare("SELECT employee_id FROM employee_badges WHERE uid_badge = ?");
    q.addBindValue(uid);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

bool BadgeService::inside(int employeeId)
{
    QSqlQuery q;
    q.prepare(
        "SELECT sens FROM (SELECT sens FROM badge_scans WHERE employee_id = ? AND date_scan >= TRUNC(SYSDATE) AND sens <> 'REFUSE' ORDER BY date_scan DESC, id DESC) WHERE ROWNUM = 1");
    q.addBindValue(employeeId);
    return q.exec() && q.next() && q.value(0).toString() == "ARRIVEE";
}

QString BadgeService::enter(const QString &text)
{
    const QString uid = normalize(text);
    m_lastUid = uid;
    const int id = uid.isEmpty() ? 0 : employeeOf(uid);
    if (!id) {
        if (!uid.isEmpty()) {
            insert(uid, 0, "REFUSE");
        }
        return uid.isEmpty() ? QStringLiteral("NO;Badge illisible") : QStringLiteral("NO;Badge inconnu");
    }
    if (!inside(id) && !insert(uid, id, "ARRIVEE")) {
        return QStringLiteral("ERR");
    }
    QSqlQuery q;
    q.prepare("SELECT name FROM employees WHERE id = ?");
    q.addBindValue(id);
    const QString name = q.exec() && q.next() ? plain(q.value(0).toString().section(' ', 0, 0), 10) : QString();
    return QStringLiteral("IN;%1;%2").arg(name, QTime::currentTime().toString("HH:mm"));
}

QString BadgeService::scan(const QString &uid)
{
    if (!adminUid.isEmpty() && uid == adminUid) {
        emit adminScanned();
        return QStringLiteral("IN;Admin;") + QTime::currentTime().toString("HH:mm");
    }
    QSqlQuery q;
    q.prepare(
        "SELECT e.id, e.name FROM employee_badges b JOIN employees e ON e.id = b.employee_id WHERE b.uid_badge = ?");
    q.addBindValue(uid);
    if (!q.exec()) {
        fail(q.lastError().text());
        return QStringLiteral("ERR");
    }
    if (!q.next()) {
        const bool saved = insert(uid, 0, "REFUSE");
        emit refused(uid);
        return saved ? QStringLiteral("NO;Badge inconnu") : QStringLiteral("ERR");
    }
    const int id = q.value(0).toInt();
    const QString name = plain(q.value(1).toString().section(' ', 0, 0), 10);

    QSqlQuery last;
    last.prepare(
        "SELECT sens, ROUND((SYSDATE - date_scan) * 86400) FROM (SELECT sens, date_scan FROM badge_scans "
        "WHERE employee_id = ? AND date_scan >= TRUNC(SYSDATE) AND sens <> 'REFUSE' ORDER BY date_scan DESC, id DESC) WHERE ROWNUM = 1");
    last.addBindValue(id);
    if (!last.exec()) {
        fail(last.lastError().text());
        return QStringLiteral("ERR");
    }
    bool arrival = true;
    if (last.next()) {
        if (last.value(1).toInt() < 5) {
            return QStringLiteral("WAIT;") + name;
        }
        arrival = last.value(0).toString() != "ARRIVEE";
    }
    if (!insert(uid, id, arrival ? "ARRIVEE" : "DEPART")) {
        return QStringLiteral("ERR");
    }
    emit scanned(id, arrival);
    if (arrival) {
        return QStringLiteral("IN;%1;%2").arg(name, QTime::currentTime().toString("HH:mm"));
    }
    const int minutes = workedMinutes(id);
    return QStringLiteral("OUT;%1;%2h%3").arg(name).arg(minutes / 60).arg(minutes % 60, 2, 10, QChar('0'));
}

bool BadgeService::insert(const QString &uid, int employeeId, const QString &kind)
{
    const int id = OracleConnection::nextId("badge_scans");
    if (id == 0) {
        return fail(QStringLiteral("Identifiant de pointage indisponible."));
    }
    QSqlQuery q;
    q.prepare("INSERT INTO badge_scans(id, uid_badge, employee_id, sens, date_scan) VALUES(?, ?, ?, ?, SYSDATE)");
    q.addBindValue(id);
    q.addBindValue(uid);
    q.addBindValue(employeeId ? QVariant(employeeId) : QVariant(QMetaType::fromType<int>()));
    q.addBindValue(kind);
    return q.exec() ? true : fail(q.lastError().text());
}

int BadgeService::workedMinutes(int employeeId)
{
    QSqlQuery q;
    q.prepare(
        "SELECT sens, date_scan FROM badge_scans WHERE employee_id = ? AND date_scan >= TRUNC(SYSDATE) AND sens <> 'REFUSE' ORDER BY date_scan, id");
    q.addBindValue(employeeId);
    qint64 seconds = 0;
    QDateTime start;
    if (q.exec()) {
        while (q.next()) {
            const QDateTime at = q.value(1).toDateTime();
            if (q.value(0).toString() == "ARRIVEE") {
                start = at;
            } else if (start.isValid()) {
                seconds += start.secsTo(at);
                start = QDateTime();
            }
        }
    }
    return int(seconds / 60);
}

bool BadgeService::assign(const QString &text, int employeeId)
{
    const QString uid = normalize(text);
    if (uid.isEmpty()) {
        return fail(QStringLiteral("Numéro de badge invalide (4 à 20 caractères 0-9, A-F)."));
    }
    QSqlQuery owner;
    owner.prepare(
        "SELECT e.name FROM employee_badges b JOIN employees e ON e.id = b.employee_id WHERE b.uid_badge = ? AND b.employee_id <> ?");
    owner.addBindValue(uid);
    owner.addBindValue(employeeId);
    if (!owner.exec()) {
        return fail(owner.lastError().text());
    }
    if (owner.next()) {
        return fail(QStringLiteral("Ce badge appartient déjà à ") + owner.value(0).toString() + QChar('.'));
    }
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery q;
    q.prepare("DELETE FROM employee_badges WHERE employee_id = ?");
    q.addBindValue(employeeId);
    bool ok = q.exec();
    if (ok) {
        q.prepare("INSERT INTO employee_badges(uid_badge, employee_id) VALUES(?, ?)");
        q.addBindValue(uid);
        q.addBindValue(employeeId);
        ok = q.exec();
    }
    if (!ok || !db.commit()) {
        fail(q.lastError().text());
        db.rollback();
        return false;
    }
    emit changed();
    return true;
}

bool BadgeService::release(int employeeId)
{
    QSqlQuery q;
    q.prepare("DELETE FROM employee_badges WHERE employee_id = ?");
    q.addBindValue(employeeId);
    if (!q.exec()) {
        return fail(q.lastError().text());
    }
    if (q.numRowsAffected() == 0) {
        return fail(QStringLiteral("Cet employé n'a pas de badge."));
    }
    emit changed();
    return true;
}

QString BadgeService::badgeOf(int employeeId)
{
    QSqlQuery q;
    q.prepare("SELECT uid_badge FROM employee_badges WHERE employee_id = ?");
    q.addBindValue(employeeId);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QVector<QPair<QString, QString>> BadgeService::badges()
{
    QVector<QPair<QString, QString>> list;
    QSqlQuery q;
    if (q.exec(
            "SELECT b.uid_badge, e.name FROM employee_badges b JOIN employees e ON e.id = b.employee_id ORDER BY UPPER(e.name)")) {
        while (q.next()) {
            list.append(qMakePair(q.value(0).toString(), q.value(1).toString()));
        }
    }
    return list;
}

QVector<BadgeScan> BadgeService::scans(int limit)
{
    QVector<BadgeScan> list;
    QSqlQuery q;
    if (q.exec(
            "SELECT s.date_scan, s.uid_badge, e.name, s.sens FROM badge_scans s LEFT JOIN employees e ON e.id = s.employee_id ORDER BY s.id DESC")) {
        while (list.size() < limit && q.next()) {
            list.append({q.value(0).toDateTime(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString()});
        }
    }
    return list;
}
