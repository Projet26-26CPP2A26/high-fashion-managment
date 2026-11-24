#pragma once
#include "activitystore.h"
#include <QDialog>

class QLabel;
class QPlainTextEdit;

class SubmitDialog : public QDialog {
    Q_OBJECT
public:
    explicit SubmitDialog(const WorkTask &task, QWidget *parent = nullptr);
    QString note() const;
    QString file() const { return m_file; }
    static bool allowed(const QString &path);

private:
    void attach();
    int m_task;
    QString m_file;
    QPlainTextEdit *m_note;
    QLabel *m_info;
};
