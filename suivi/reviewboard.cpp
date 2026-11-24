#include "reviewboard.h"
#include "activityboard.h"
#include "employeetile.h"
#include "fx.h"
#include "living.h"
#include "submitdialog.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

ReviewBoard::ReviewBoard(ActivityBoard &board, QWidget *parent) : QWidget(parent), m_board(board)
{
    auto *page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(14);
    auto *kpis = new QHBoxLayout;
    kpis->setSpacing(14);
    const QStringList titles = {QStringLiteral("En attente de revue"), QStringLiteral("Validées aujourd'hui"), QStringLiteral("Qualité moyenne")};
    const QStringList tints = {"#e6c27a", "#7fc08a", "#ecdcae"};
    for (int i = 0; i < 3; ++i) {
        auto *frame = new QFrame;
        frame->setProperty("card", true);
        auto *column = new QVBoxLayout(frame);
        column->setContentsMargins(18, 12, 18, 12);
        auto *caption = new QLabel(titles[i]);
        caption->setProperty("muted", true);
        m_kpi[i] = new QLabel("0");
        m_kpi[i]->setProperty("kpi", true);
        m_kpi[i]->setStyleSheet("color:" + tints[i]);
        column->addWidget(caption);
        column->addWidget(m_kpi[i]);
        kpis->addWidget(frame, 1);
    }
    page->addLayout(kpis);
    auto *root = new QHBoxLayout;
    root->setSpacing(16);
    page->addLayout(root, 1);
    m_list = new QListWidget;
    m_list->setFixedWidth(360);
    m_list->setSpacing(4);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setStyleSheet("QListWidget{background:transparent;border:none;outline:none;}"
                          "QListWidget::item{background:#232326;border:1px solid #323236;border-radius:12px;padding:14px;color:#f3f0ea;}"
                          "QListWidget::item:hover{border-color:#e2cf9f;}"
                          "QListWidget::item:selected{background:#2e2e32;border:1px solid #e2cf9f;color:white;}");
    root->addWidget(m_list);

    auto *empty = new QWidget;
    auto *emptyBox = new QVBoxLayout(empty);
    auto *emptyText = new QLabel(QStringLiteral("<div style='font-size:15pt;color:#f7f4ee'><b>Tout est à jour</b></div><div style='font-size:10pt;color:#8f8c86'>Les prochains dépôts des employés apparaîtront ici.</div>"));
    emptyText->setAlignment(Qt::AlignCenter);
    emptyBox->addStretch();
    emptyBox->addWidget(new LivingIcon(LivingIcon::Check, 130), 0, Qt::AlignCenter);
    emptyBox->addWidget(emptyText);
    emptyBox->addStretch();
    auto *panel = new QFrame;
    panel->setProperty("card", true);
    auto *box = new QVBoxLayout(panel);
    box->setContentsMargins(28, 24, 28, 24);
    box->setSpacing(12);
    m_title = new QLabel;
    m_title->setProperty("kpi", true);
    m_title->setStyleSheet("font-size:19pt");
    m_info = new QLabel;
    m_info->setProperty("muted", true);
    m_steps = new QLabel;
    m_note = new QLabel;
    for (QLabel *l : {m_title, m_info, m_steps, m_note}) { l->setTextFormat(Qt::PlainText); l->setWordWrap(true); box->addWidget(l); }
    m_open = new QPushButton;
    box->addWidget(m_open, 0, Qt::AlignLeft);
    box->addStretch();
    auto *rate = new QLabel(QStringLiteral("Note de qualité"));
    rate->setProperty("muted", true);
    m_stars = new StarRating;
    m_feedback = new QPlainTextEdit;
    m_feedback->setPlaceholderText(QStringLiteral("Commentaire pour l'employé (obligatoire pour demander une correction)"));
    m_feedback->setFixedHeight(90);
    box->addWidget(rate);
    box->addWidget(m_stars);
    box->addWidget(m_feedback);
    auto *reject = new QPushButton(QStringLiteral("↺  Demander une correction"));
    reject->setProperty("danger", true);
    auto *accept = new QPushButton(QStringLiteral("✓  Valider le dépôt"));
    accept->setProperty("primary", true);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(reject);
    buttons->addWidget(accept);
    box->addLayout(buttons);
    m_pages = new QStackedWidget;
    m_pages->addWidget(empty);
    m_pages->addWidget(panel);
    root->addWidget(m_pages, 1);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) { show(item ? item->data(Qt::UserRole).toInt() : 0); });
    connect(accept, &QPushButton::clicked, this, [this] { decide(true); });
    connect(reject, &QPushButton::clicked, this, [this] { decide(false); });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (SubmitDialog::allowed(m_current.file) && QFileInfo::exists(m_current.file)) QDesktopServices::openUrl(QUrl::fromLocalFile(m_current.file));
        else Fx::toast(window(), QStringLiteral("Fichier introuvable"));
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ReviewBoard::reload);
    timer->start(1000);
    reload();
}

void ReviewBoard::reload()
{
    const QVector<WorkTask> fresh = m_board.reviews();
    bool same = fresh.size() == m_tasks.size();
    for (int i = 0; same && i < fresh.size(); ++i) same = fresh[i].id == m_tasks[i].id;
    m_tasks = fresh;
    Fx::countTo(m_kpi[0], int(fresh.size()));
    Fx::countTo(m_kpi[1], m_board.store().acceptedToday());
    const double quality = m_board.store().averageRating();
    m_kpi[2]->setText(quality > 0 ? QStringLiteral("%1 / 5 ★").arg(quality, 0, 'f', 1) : QStringLiteral("—"));
    m_list->setVisible(!fresh.isEmpty());
    if (same) return;
    const int keep = m_current.id;
    int row = -1;
    {
        const QSignalBlocker block(m_list);
        m_list->clear();
        for (const WorkTask &t : std::as_const(m_tasks)) {
            auto *item = new QListWidgetItem(t.title + "\n" + m_board.displayName(t.who) + QStringLiteral(" · ") + clockText(t.spent), m_list);
            item->setData(Qt::UserRole, t.id);
            if (t.id == keep) row = m_list->count() - 1;
        }
        if (row < 0 && m_list->count()) row = 0;
        m_list->setCurrentRow(row);
    }
    const int id = row < 0 ? 0 : m_tasks[row].id;
    if (id != keep) show(id);
}

void ReviewBoard::show(int id)
{
    m_current = WorkTask();
    for (const WorkTask &t : std::as_const(m_tasks)) if (t.id == id) m_current = t;
    Fx::switchPage(m_pages, m_current.id ? 1 : 0);
    if (!m_current.id) return;
    const WorkTask &t = m_current;
    m_title->setText(t.title);
    const int gap = t.spent - t.planned;
    m_info->setText(m_board.displayName(t.who) + QStringLiteral(" · ") + t.details + QStringLiteral("\nTemps passé %1 · prévu %2 · %3")
                        .arg(clockText(t.spent), clockText(t.planned), gap > 0 ? QStringLiteral("retard ") + clockText(gap) : QStringLiteral("dans les temps")));
    QStringList steps;
    for (int i = 0; i < t.steps.size(); ++i) steps << (t.stepsDone & (1 << i) ? QStringLiteral("✓  ") : QStringLiteral("○  ")) + t.steps[i];
    m_steps->setText(steps.join('\n'));
    m_steps->setVisible(!steps.isEmpty());
    m_note->setText(t.note.isEmpty() ? QString() : QStringLiteral("« ") + t.note + QStringLiteral(" »"));
    m_open->setVisible(!t.file.isEmpty());
    m_open->setText(QStringLiteral("Ouvrir : ") + QFileInfo(t.file).fileName().section('_', 2));
    m_stars->setValue(4);
    int delay = Motion::Step;
    for (QWidget *w : QList<QWidget *>{m_title, m_info, m_steps, m_note}) {
        Fx::reveal(w, delay);
        delay += Motion::Step;
    }
    m_feedback->clear();
}

void ReviewBoard::decide(bool accept)
{
    if (!m_current.id) return;
    const QString feedback = m_feedback->toPlainText().trimmed().left(500);
    if (!accept && feedback.isEmpty()) {
        Fx::shake(m_feedback);
        Fx::toast(window(), QStringLiteral("Expliquez la correction à faire"));
        return;
    }
    const WorkTask task = m_current;
    if (!m_board.review(task, accept, m_stars->value(), feedback)) {
        Fx::toast(window(), QStringLiteral("Ce dépôt a déjà été traité"));
        reload();
        return;
    }
    Fx::toast(window(), (accept ? QStringLiteral("Validé : ") : QStringLiteral("Renvoyé pour correction : ")) + task.title);
    reload();
}

void ReviewBoard::enter()
{
    int delay = Motion::Step;
    for (QLabel *value : m_kpi) {
        Fx::reveal(value->parentWidget(), delay);
        delay += Motion::Step;
    }
    Fx::reveal(m_list, delay);
    if (m_current.id) m_stars->setValue(m_stars->value());
}
