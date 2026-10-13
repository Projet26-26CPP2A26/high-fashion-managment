#include "store.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <map>

QString Person::initials() const {
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString s;
    for (int i = 0; i < parts.size() && i < 2; ++i) s += parts[i].left(1).toUpper();
    return s.isEmpty() ? QStringLiteral("–") : s;
}
int Task::expected(const QDate &today) const {
    if (!start.isValid() || !end.isValid() || today < start) return 0;
    const int days = start.daysTo(end) + 1, elapsed = start.daysTo(today) + 1;
    return std::min(100, elapsed * 100 / std::max(1, days));
}
int Task::gap(const QDate &today) const { return done() ? 0 : std::max(0, expected(today) - progress); }

QStringList TaskStore::steps() {
    return {"Patronage", "Couture haute précision", "Broderie main", "Drapé et moulage",
            "Finitions", "Contrôle qualité", "Essayage"};
}

bool TaskStore::init(bool demo) {
    lastError.clear();
    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS tache ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, id_commande INTEGER NOT NULL, etape TEXT NOT NULL, "
                "id_employe INTEGER, date_debut TEXT NOT NULL, date_fin_prevue TEXT NOT NULL, "
                "priorite TEXT NOT NULL CHECK(priorite IN('Haute','Moyenne','Basse')), "
                "avancement INTEGER NOT NULL CHECK(avancement BETWEEN 0 AND 100))"))
        return fail(q.lastError().text());
    if (!q.exec("CREATE TABLE IF NOT EXISTS production_meta(key TEXT PRIMARY KEY, value TEXT NOT NULL)"))
        return fail(q.lastError().text());
    if (demo) seed();
    return true;
}

QVector<Person> TaskStore::people() const {
    QVector<Person> out;
    QSqlQuery q(db);
    // Table du module employés ; si elle n'existe pas, la requête échoue et la liste reste vide.
    if (!q.exec("SELECT id, name, COALESCE(position,''), COALESCE(specialty,''), COALESCE(available,1) "
                "FROM employees ORDER BY name COLLATE NOCASE"))
        return out;
    while (q.next()) {
        Person p;
        p.id = q.value(0).toInt(); p.name = q.value(1).toString(); p.position = q.value(2).toString();
        p.specialty = q.value(3).toString(); p.available = q.value(4).toInt() != 0;
        out.append(p);
    }
    return out;
}

QVector<Task> TaskStore::all() {
    QVector<Task> out;
    QMap<int, Person> map;
    for (const Person &p : people()) map.insert(p.id, p);
    QSqlQuery q(db);
    if (!q.exec("SELECT id, id_commande, etape, id_employe, date_debut, date_fin_prevue, priorite, avancement "
                "FROM tache ORDER BY id")) {
        fail(q.lastError().text());
        return out;
    }
    while (q.next()) {
        Task t;
        t.id = q.value(0).toInt(); t.order = q.value(1).toInt(); t.step = q.value(2).toString();
        t.employeeId = q.value(3).isNull() ? 0 : q.value(3).toInt();
        t.start = QDate::fromString(q.value(4).toString(), Qt::ISODate);
        t.end = QDate::fromString(q.value(5).toString(), Qt::ISODate);
        t.priority = q.value(6).toString(); t.progress = q.value(7).toInt();
        if (map.contains(t.employeeId)) {
            t.employeeName = map[t.employeeId].name;
            t.employeeAvailable = map[t.employeeId].available;
        }
        out.append(t);
    }
    return out;
}

bool TaskStore::save(Task &t) {
    lastError.clear();
    t.step = t.step.trimmed();
    if (t.step.isEmpty()) return fail(QStringLiteral("L'étape est obligatoire."));
    if (t.order < 1) return fail(QStringLiteral("L'identifiant de commande doit être positif."));
    if (!t.start.isValid() || !t.end.isValid() || t.end < t.start)
        return fail(QStringLiteral("La date de fin prévue doit être après la date de début."));
    if (t.progress < 0 || t.progress > 100) return fail(QStringLiteral("L'avancement doit être compris entre 0 et 100 %."));
    if (!priorities().contains(t.priority)) return fail(QStringLiteral("Priorité invalide."));
    QSqlQuery q(db);
    q.prepare(t.id ? "UPDATE tache SET id_commande=?, etape=?, id_employe=?, date_debut=?, date_fin_prevue=?, priorite=?, avancement=? WHERE id=?"
                   : "INSERT INTO tache(id_commande, etape, id_employe, date_debut, date_fin_prevue, priorite, avancement) VALUES(?,?,?,?,?,?,?)");
    q.addBindValue(t.order); q.addBindValue(t.step);
    q.addBindValue(t.employeeId > 0 ? QVariant(t.employeeId) : QVariant());
    q.addBindValue(t.start.toString(Qt::ISODate)); q.addBindValue(t.end.toString(Qt::ISODate));
    q.addBindValue(t.priority); q.addBindValue(t.progress);
    if (t.id) q.addBindValue(t.id);
    if (!q.exec()) return fail(q.lastError().text());
    if (!t.id) t.id = q.lastInsertId().toInt();
    return true;
}

bool TaskStore::remove(int id) {
    lastError.clear();
    QSqlQuery q(db);
    q.prepare("DELETE FROM tache WHERE id=?");
    q.addBindValue(id);
    return q.exec() ? true : fail(q.lastError().text());
}

void TaskStore::seed() {
    QSqlQuery q(db);
    if (!q.exec("SELECT count(*) FROM production_meta WHERE key='demo_seeded'") || !q.next() || q.value(0).toInt()) return;
    q.finish();
    if (!q.exec("SELECT count(*) FROM tache") || !q.next()) return;
    const bool empty = q.value(0).toInt() == 0;
    q.finish();
    if (empty) {
        struct S { int order; const char *step; int start, end; const char *prio; int progress; };
        const S demo[] = {
            {1001, "Patronage",               -12, -5, "Haute",   100},
            {1001, "Couture haute précision",  -6,  3, "Haute",    55},
            {1001, "Broderie main",            -3,  9, "Haute",    15},
            {1002, "Drapé et moulage",         -9, -2, "Moyenne",  60},
            {1002, "Finitions",                 2, 12, "Moyenne",   0},
            {1003, "Contrôle qualité",          0,  6, "Basse",    20},
            {1003, "Couture haute précision",   5, 10, "Basse",     0},
            {1004, "Broderie main",            -1,  4, "Moyenne",  20},
        };
        const QVector<Person> staff = people();
        const int fallback[] = {12, 14, 21, 9, 17};
        const QDate today = QDate::currentDate();
        db.transaction();
        int i = 0;
        for (const S &s : demo) {
            int emp = fallback[i % 5];
            if (!staff.isEmpty()) {
                emp = staff[i % staff.size()].id;
                for (const Person &p : staff)
                    if (p.available && p.specialty == QString::fromUtf8(s.step)) { emp = p.id; break; }
            }
            ++i;
            Task t;
            t.order = s.order; t.step = QString::fromUtf8(s.step); t.employeeId = emp;
            t.start = today.addDays(s.start); t.end = today.addDays(s.end);
            t.priority = QString::fromUtf8(s.prio); t.progress = s.progress;
            save(t);
        }
        db.commit();
    }
    q.exec("INSERT OR IGNORE INTO production_meta(key, value) VALUES('demo_seeded','1')");
}

// ------------------------------------------------------------------ analyses
namespace Analytics {
QVector<Task> planning(QVector<Task> v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](const Task &t) { return t.done(); }), v.end());
    std::sort(v.begin(), v.end(), [](const Task &a, const Task &b) {
        if (a.priorityRank() != b.priorityRank()) return a.priorityRank() < b.priorityRank();
        if (a.end != b.end) return a.end < b.end;
        if (a.start != b.start) return a.start < b.start;
        return a.id < b.id;
    });
    return v;
}
QVector<OrderSummary> orders(const QVector<Task> &v) {
    std::map<int, OrderSummary> m;
    std::map<int, int> sum;
    for (const Task &t : v) {
        OrderSummary &o = m[t.order];
        o.order = t.order; ++o.tasks; sum[t.order] += t.progress;
        if (t.late()) ++o.late;
    }
    QVector<OrderSummary> out;
    for (auto &kv : m) { kv.second.progress = qRound(sum[kv.first] / double(kv.second.tasks)); out.append(kv.second); }
    return out;
}
QVector<StepDelay> delays(const QVector<Task> &v) {
    std::map<QString, StepDelay> m;
    for (const Task &t : v) {
        if (t.done() || t.gap() <= 0) continue;
        StepDelay &d = m[t.step];
        d.step = t.step; ++d.tasks; d.gap += t.gap();
        if (t.late()) ++d.late;
    }
    QVector<StepDelay> out;
    for (auto &kv : m) { kv.second.gap /= kv.second.tasks; out.append(kv.second); }
    std::sort(out.begin(), out.end(), [](const StepDelay &a, const StepDelay &b) {
        if (a.gap != b.gap) return a.gap > b.gap;
        return a.tasks > b.tasks;
    });
    return out;
}
QVector<int> workload(const QVector<Task> &v, int days) {
    QVector<int> out(days, 0);
    const QDate today = QDate::currentDate();
    for (int i = 0; i < days; ++i) {
        const QDate d = today.addDays(i);
        for (const Task &t : v) if (!t.done() && t.start <= d && d <= t.end) ++out[i];
    }
    return out;
}
}
