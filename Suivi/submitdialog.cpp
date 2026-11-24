#include "submitdialog.h"
#include "UiAnimations.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

bool SubmitDialog::allowed(const QString &path)
{
    static const QStringList types = {"png", "jpg", "jpeg", "pdf", "docx", "xlsx", "txt"};
    return types.contains(QFileInfo(path).suffix().toLower());
}

SubmitDialog::SubmitDialog(const WorkTask &task, QWidget *parent) : QDialog(parent), m_task(task.id)
{
    setWindowTitle(QStringLiteral("Déposer la tâche"));
    setMinimumWidth(560);
    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(26, 22, 26, 22);
    box->setSpacing(12);
    auto *title = new QLabel(task.title);
    title->setTextFormat(Qt::PlainText);
    title->setProperty("section", true);
    auto *info = new QLabel(QStringLiteral("%1 étapes validées · temps passé %2 (prévu %3)").arg(task.steps.size()).arg(clockText(task.spent), clockText(task.planned)));
    info->setProperty("muted", true);
    m_note = new QPlainTextEdit;
    m_note->setPlaceholderText(QStringLiteral("Compte rendu pour le responsable RH : ce qui a été fait, difficultés…"));
    m_note->setFixedHeight(120);
    auto *attachButton = new QPushButton(QStringLiteral("Joindre un fichier…"));
    m_info = new QLabel(QStringLiteral("Image, PDF, Office, texte (10 Mo)"));
    m_info->setTextFormat(Qt::PlainText);
    m_info->setProperty("muted", true);
    auto *row = new QHBoxLayout;
    row->addWidget(attachButton);
    row->addWidget(m_info, 1);
    auto *cancel = new QPushButton(QStringLiteral("Annuler"));
    auto *send = new QPushButton(QStringLiteral("Envoyer en revue  ↗"));
    send->setProperty("primary", true);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(send);
    box->addWidget(title);
    box->addWidget(info);
    box->addWidget(m_note);
    box->addLayout(row);
    box->addLayout(buttons);
    connect(attachButton, &QPushButton::clicked, this, &SubmitDialog::attach);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(send, &QPushButton::clicked, this, [this] {
        if (note().isEmpty()) {
            m_info->setText(QStringLiteral("Le compte rendu est obligatoire."));
            Fx::shake(this);
            return;
        }
        accept();
    });
}

QString SubmitDialog::note() const { return m_note->toPlainText().trimmed().left(1000); }

void SubmitDialog::attach()
{
    const QString source = QFileDialog::getOpenFileName(this, QStringLiteral("Pièce jointe"), QString(), QStringLiteral("Documents (*.png *.jpg *.jpeg *.pdf *.docx *.xlsx *.txt)"));
    if (source.isEmpty()) return;
    const QFileInfo info(source);
    if (!allowed(source) || info.size() > 10 * 1024 * 1024) {
        m_info->setText(QStringLiteral("Fichier refusé (type ou taille)."));
        Fx::shake(this);
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/depots";
    QDir().mkpath(folder);
    const QString target = folder + QStringLiteral("/%1_%2_%3").arg(m_task).arg(QDateTime::currentSecsSinceEpoch()).arg(info.fileName());
    if (!QFile::copy(source, target)) {
        m_info->setText(QStringLiteral("Copie impossible."));
        return;
    }
    m_file = target;
    m_info->setText(QStringLiteral("Joint : ") + info.fileName());
}
