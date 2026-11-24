#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class TaskModel {
public:
    QString title, details;
    QStringList steps;
    int minutes;
};

namespace TaskCatalog {
QVector<TaskModel> forSpecialty(const QString &s);
}
