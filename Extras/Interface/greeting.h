#pragma once

#include "activitystore.h"
#include <QTime>

namespace Greeting {
QString hello(int ever, int today, const QString &name, const QTime &now = QTime::currentTime());
QString plural(int n, const QString &one, const QString &many);
QString tasks(const QVector<WorkTask> &list);
QString team(int connected, int reviews);
}
