// Données et règles métier de la production (aucune dépendance à l'interface).
#pragma once
#include <QDate>
#include <QMetaType>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct Task {
    int id = 0, order = 1, employeeId = 0, progress = 0;
    QString step, priority = QStringLiteral("Moyenne"), employeeName;
    QDate start, end;
    bool employeeAvailable = true;

    bool done() const { return progress >= 100; }
    bool late(const QDate &today = QDate::currentDate()) const { return !done() && end.isValid() && end < today; }
    QString ref() const { return QStringLiteral("TSK-%1").arg(id, 4, 10, QChar('0')); }
    QString orderRef() const { return QStringLiteral("CMD-%1").arg(order, 4, 10, QChar('0')); }
    QString employeeRef() const { return employeeId ? QStringLiteral("EMP-%1").arg(employeeId, 3, 10, QChar('0')) : QString(); }
    QString employeeLabel() const { return employeeName.isEmpty() ? employeeRef() : employeeName; }
    int priorityRank() const { return priority == QLatin1String("Haute") ? 0 : priority == QLatin1String("Moyenne") ? 1 : 2; }
    int expected(const QDate &today = QDate::currentDate()) const;   // avancement attendu à ce jour (%)
    int gap(const QDate &today = QDate::currentDate()) const;        // retard en points (attendu - réel)
};
Q_DECLARE_METATYPE(Task)

// Employé lu dans la table `employees` du module Gestion des employés (s'il existe).
struct Person {
    int id = 0;
    QString name, position, specialty;
    bool available = true;
    QString initials() const;
};

struct StepDelay { QString step; int tasks = 0, late = 0; double gap = 0; };
struct OrderSummary { int order = 0, tasks = 0, late = 0, progress = 0; };

class TaskStore {
public:
    explicit TaskStore(QSqlDatabase database) : db(database) {}
    bool init(bool demo);
    QVector<Task> all();
    bool save(Task &task);
    bool remove(int id);
    QVector<Person> people() const;
    QString error() const { return lastError; }
    static QStringList steps();
    static QStringList priorities() { return {QStringLiteral("Haute"), QStringLiteral("Moyenne"), QStringLiteral("Basse")}; }
private:
    QSqlDatabase db;
    QString lastError;
    bool fail(const QString &m) { lastError = m; return false; }
    void seed();
};

namespace Analytics {
QVector<Task> planning(QVector<Task> tasks);                       // non terminées : priorité puis date de livraison
QVector<OrderSummary> orders(const QVector<Task> &tasks);          // progression par commande
QVector<StepDelay> delays(const QVector<Task> &tasks);             // étapes en retard sur le planning, pire en premier
QVector<int> workload(const QVector<Task> &tasks, int days);       // tâches actives par jour à partir d'aujourd'hui
}
