#pragma once
#include <QString>

namespace OracleCheck {
struct Report {
    bool ok = false;
    QString text;
};
Report run();
}
