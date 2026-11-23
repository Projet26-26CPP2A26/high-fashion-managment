#pragma once

#include "activitystore.h"
#include <QTime>

namespace Greeting {
inline QString hello(int ever, int today, const QString &name, const QTime &now = QTime::currentTime())
{
    const bool evening = now.hour() >= 18 || now.hour() < 5;
    const QString word = !ever ? QStringLiteral("Bienvenue") : !today ? (evening ? QStringLiteral("Bonsoir") : QStringLiteral("Bonjour")) : (evening ? QStringLiteral("Rebonsoir") : QStringLiteral("Rebonjour"));
    return word + QStringLiteral(", ") + name;
}

inline QString plural(int n, const QString &one, const QString &many) { return QString::number(n) + ' ' + (n > 1 ? many : one); }

inline QString tasks(const QVector<WorkTask> &list)
{
    int open = 0, rework = 0;
    for (const WorkTask &t : list) {
        open += t.state == WorkTask::Todo || t.state == WorkTask::Current || t.state == WorkTask::Rework;
        rework += t.state == WorkTask::Rework;
    }
    if (!open) return QStringLiteral("Tout est à jour : aucune tâche en attente.");
    return QStringLiteral("C'est parti ! ") + plural(open, QStringLiteral("tâche vous attend"), QStringLiteral("tâches vous attendent"))
         + (rework ? QStringLiteral(" · ") + plural(rework, QStringLiteral("correction"), QStringLiteral("corrections")) : QString()) + '.';
}

inline QString team(int connected, int reviews)
{
    return plural(connected, QStringLiteral("employé connecté"), QStringLiteral("employés connectés")) + QStringLiteral(" · ")
         + (reviews ? plural(reviews, QStringLiteral("dépôt à valider"), QStringLiteral("dépôts à valider")) : QStringLiteral("aucun dépôt à valider"));
}
}
