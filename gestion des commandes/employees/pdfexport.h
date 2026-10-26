#pragma once
#include <QString>

namespace Reports {
bool writePdf(const QString &path,const QString &html,QString *error=nullptr,bool landscape=false);
}
