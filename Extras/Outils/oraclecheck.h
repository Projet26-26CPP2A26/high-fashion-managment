#pragma once
#include <QString>

namespace OracleCheck {
class Report {
public:
    bool ok = false;
    QString text;
};
Report run();
}
