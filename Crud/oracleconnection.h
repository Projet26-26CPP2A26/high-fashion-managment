#pragma once
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

class OracleConnection {
public:
    static OracleConnection &instance();

    static OracleConnection &createInstance()
    {
        return instance();
    }
    bool createConnect()
    {
        return open();
    }

    OracleConnection(const OracleConnection &) = delete;
    OracleConnection &operator=(const OracleConnection &) = delete;

    class Settings {
    public:
        QString dsn, user;
        bool hasPassword = false;
    };
    static Settings settings();

    static bool driverAvailable();
    static QString connectionName();

    static int nextId(const QString &table);
    static void warmUp();

    bool open();
    bool isOpen() const;

    void close();

    void shutdown();

    QSqlDatabase database() const;

    QSqlError lastError() const
    {
        return m_error;
    }

    QString errorMessage() const
    {
        return m_message;
    }

    QString diagnostics() const;

private:
    OracleConnection() = default;

    ~OracleConnection() = default;

    static QString describe(const QSqlError &error);

    QSqlError m_error;
    QString m_message;
};
