#pragma once
#include <QString>

class OracleConnection {
public:
    static OracleConnection &instance();
    bool open();
    void close();
    QString errorMessage() const;
    static int nextId(const QString &table);

private:
    OracleConnection();
    QString message;
};
