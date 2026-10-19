#include "productionpages.h"
#include "pdfexport.h"
#include "productionkit.h"
#include <QtWidgets>
#include <functional>

namespace {
enum { RoleTask = Qt::UserRole + 1 };
const QStringList kHeaders = {"Réf", "Commande", "Étape", "Employé", "Début", "Fin prévue", "Priorité", "Avancement", "Actions"};

QLabel *mk(const QString &text, const char *prop = nullptr) {
    auto *l = new QLabel(text);
    if (prop) l->setProperty(prop, true);
    return l;
}
QFrame *newCard(QLayout *layout, int margin = 20) {
    auto *f = new QFrame;
    f->setProperty("card", true);
    layout->setContentsMargins(margin, margin, margin, margin);
    f->setLayout(layout);
    return f;
}
QString fmt(const QDate &d) { return d.toString("dd/MM/yyyy"); }
QColor prioColor(const QString &p) { return p == "Haute" ? Kit::crit : p == "Moyenne" ? Kit::warn : Kit::ok; }
QString initialsOf(const QString &name) { Person p; p.name = name; return p.initials(); }
QProgressBar *bar(int value) {
    auto *b = new QProgressBar;
    b->setRange(0, 100); b->setValue(value); b->setTextVisible(false);
    b->setProperty("done", value >= 100);
    return b;
}
void clearLayout(QLayout *l) {
    while (QLayoutItem *it = l->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        if (QLayout *c = it->layout()) clearLayout(c);
        delete it;
    }
}
QWidget *iconBox(Kit::Icon icon) {
    auto *box = new QFrame;
    box->setObjectName("iconBox");
    box->setFixedSize(36, 36);
    auto *l = new QVBoxLayout(box);
    l->setContentsMargins(0, 0, 0, 0);
    auto *i = new QLabel;
    i->setPixmap(Kit::pixmap(icon, 18, Kit::gold));
    i->setAlignment(Qt::AlignCenter);
    l->addWidget(i);
    return box;
}
QHBoxLayout *cardHeader(const QString &title, const QString &chip = {}) {
    auto *h = new QHBoxLayout;
    h->addWidget(mk(title, "section"));
    h->addStretch();
    if (!chip.isEmpty()) { auto *c = new QLabel(chip); c->setObjectName("chip"); h->addWidget(c); }
    return h;
}
void setupTable(QTableWidget *t, int rowHeight = 46) {
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(rowHeight);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->horizontalHeader()->setStretchLastSection(true);
}
QTableWidgetItem *cell(const QString &text, const QColor &color = Kit::text) {
    auto *i = new QTableWidgetItem(text);
    i->setForeground(color);
    return i;
}

QString document(const QString &title, const QString &body) {
    return QStringLiteral("<html><body style='font-family:Segoe UI;font-size:9pt;color:#222'><h1>AUREA — %1</h1>"
                          "<p>Atelier de couture · %2</p>%3</body></html>")
        .arg(title.toHtmlEscaped(), QDate::currentDate().toString("dd/MM/yyyy"), body);
}
QString headerRow(const QStringList &v) {
    QString out = "<tr bgcolor='#c9a95f'>";
    for (const QString &s : v) out += "<th align='left'>" + s.toHtmlEscaped() + "</th>";
    return out + "</tr>";
}
QString htmlRow(const QStringList &v) {
    QString out = "<tr>";
    for (const QString &s : v) out += "<td>" + s.toHtmlEscaped() + "</td>";
    return out + "</tr>";
}
void savePdf(QWidget *parent, const QString &name, const QString &html, bool landscape) {
    const QString path = QFileDialog::getSaveFileName(parent, "Exporter en PDF", name, "PDF (*.pdf)");
    if (path.isEmpty()) return;
    QString error;
    if (!Reports::writePdf(path, html, &error, landscape)) QMessageBox::warning(parent, "AUREA — Production", error);
    else QMessageBox::information(parent, "AUREA — Production", "PDF créé :\n" + path);
}
}  // namespace

// ============================================================================ modèle / proxy / delegate
class TaskModel : public QAbstractTableModel {
public:
    QVector<Task> rows;
    void setRows(const QVector<Task> &r) { beginResetModel(); rows = r; endResetModel(); }
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : rows.size(); }
    int columnCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : 9; }
    QVariant data(const QModelIndex &i, int role) const override {
        if (!i.isValid()) return {};
        const Task &t = rows[i.row()];
        if (role == RoleTask) return QVariant::fromValue(t);
        if (role == Qt::UserRole) {  // clés de tri
            switch (i.column()) {
            case 0: return t.id;
            case 1: return t.order;
            case 2: return t.step;
            case 3: return t.employeeLabel().isEmpty() ? QStringLiteral("\uFFFF") : t.employeeLabel();
            case 4: return qlonglong(t.start.toJulianDay());
            case 5: return qlonglong(t.end.toJulianDay());
            case 6: return t.priorityRank();
            case 7: return t.progress;
            }
        }
        return {};
    }
};

class TaskProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setSearch(const QString &s) { search = s.trimmed(); invalidateFilter(); }
    void setPriority(const QString &p) { prio = p; invalidateFilter(); }
    void setState(int s) { state = s; invalidateFilter(); }
    QVariant headerData(int s, Qt::Orientation o, int role = Qt::DisplayRole) const override {
        if (o == Qt::Horizontal && role == Qt::DisplayRole && s < kHeaders.size()) {
            QString t = kHeaders[s];
            if (s == sc && s != 8) t += (so == Qt::AscendingOrder) ? "  ▴" : "  ▾";
            return t;
        }
        return QSortFilterProxyModel::headerData(s, o, role);
    }
    void sort(int c, Qt::SortOrder o = Qt::AscendingOrder) override {
        if (c == 8) return;
        sc = c; so = o;
        QSortFilterProxyModel::sort(c, o);
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }
protected:
    bool filterAcceptsRow(int r, const QModelIndex &p) const override {
        const Task t = sourceModel()->index(r, 0, p).data(RoleTask).value<Task>();
        if (!prio.isEmpty() && t.priority != prio) return false;
        if (state == 1 && !t.late()) return false;
        if (state == 2 && t.done()) return false;
        if (state == 3 && !t.done()) return false;
        if (search.isEmpty()) return true;
        const QString hay = QStringList{t.ref(), t.orderRef(), t.step, t.employeeLabel(), t.employeeRef(), fmt(t.start), fmt(t.end),
                                        t.priority, QString::number(t.progress) + " %"}.join(' ');
        return hay.contains(search, Qt::CaseInsensitive);
    }
private:
    QString prio, search;
    int state = 0, sc = 0;
    Qt::SortOrder so = Qt::AscendingOrder;
};

class TaskDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<void(const QModelIndex &)> onEdit, onDelete;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {100, 60}; }

    static QRect editRect(const QRect &r) { return QRect(r.left() + 4, r.center().y() - 15, 70, 30); }
    static QRect deleteRect(const QRect &r) { return QRect(editRect(r).right() + 6, r.center().y() - 15, 84, 30); }

    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &ix) const override {
        const Task t = ix.data(RoleTask).value<Task>();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (o.state & QStyle::State_Selected) p->fillRect(o.rect, QColor(201, 178, 124, 36));
        else if (o.state & QStyle::State_MouseOver) p->fillRect(o.rect, QColor(255, 255, 255, 8));
        p->setPen(QColor("#2f2f33"));
        p->drawLine(o.rect.bottomLeft(), o.rect.bottomRight());

        QRect r = o.rect.adjusted(10, 0, -6, 0);
        QFont f = o.font;
        QColor col = Kit::text;
        QString txt;
        Qt::Alignment al = Qt::AlignVCenter | Qt::AlignLeft;

        switch (ix.column()) {
        case 0: txt = t.ref(); col = Kit::muted; f.setPointSizeF(9); break;
        case 1: txt = t.orderRef(); break;
        case 2: txt = t.step; f.setWeight(QFont::DemiBold); f.setPointSizeF(10.5); break;
        case 3: {
            if (t.employeeLabel().isEmpty()) { txt = "Non assigné"; col = Kit::muted; f.setItalic(true); break; }
            const QRectF av(r.left(), r.center().y() - 13, 26, 26);
            p->setPen(Qt::NoPen); p->setBrush(QColor("#3a3427"));
            p->drawEllipse(av);
            QFont af = f; af.setPointSizeF(8); af.setWeight(QFont::DemiBold);
            p->setFont(af); p->setPen(Kit::gold);
            p->drawText(av, Qt::AlignCenter, t.employeeName.isEmpty() ? QStringLiteral("–") : initialsOf(t.employeeName));
            r.setLeft(r.left() + 36);
            txt = t.employeeLabel();
            if (!t.employeeAvailable) { col = Kit::warn; txt += "  ·  indisponible"; }
            break;
        }
        case 4: txt = fmt(t.start); break;
        case 5: txt = fmt(t.end); if (t.late()) col = Kit::crit; break;
        case 6:
            p->setPen(Qt::NoPen); p->setBrush(prioColor(t.priority));
            p->drawEllipse(QPointF(r.left() + 4, r.center().y()), 4, 4);
            r.setLeft(r.left() + 18);
            txt = t.priority;
            break;
        case 7: {
            const QRect track(r.left(), r.center().y() - 3, r.width() - 48, 6);
            p->setPen(Qt::NoPen); p->setBrush(Kit::track);
            p->drawRoundedRect(track, 3, 3);
            QRect fill = track; fill.setWidth(track.width() * t.progress / 100);
            QLinearGradient g(fill.topLeft(), fill.topRight());
            if (t.done()) { g.setColorAt(0, Kit::ok); g.setColorAt(1, Kit::ok); }
            else { g.setColorAt(0, QColor("#c9b27c")); g.setColorAt(1, QColor("#e9d8a8")); }
            p->setBrush(g);
            if (t.progress > 0) p->drawRoundedRect(fill, 3, 3);
            txt = QString::number(t.progress) + " %";
            r = QRect(r.right() - 42, r.top(), 42, r.height());
            al = Qt::AlignVCenter | Qt::AlignRight;
            break;
        }
        case 8: {
            QFont bf = f; bf.setPointSizeF(9); bf.setWeight(QFont::DemiBold);
            p->setFont(bf);
            const QRect e = editRect(o.rect), d = deleteRect(o.rect);
            p->setPen(QColor("#3a3a3f")); p->setBrush(QColor("#2a2a2d")); p->drawRoundedRect(e, 8, 8);
            p->setPen(Kit::text); p->drawText(e, Qt::AlignCenter, QStringLiteral("Modifier"));
            p->setPen(QColor("#5e3d44")); p->setBrush(QColor("#3a2a2e")); p->drawRoundedRect(d, 8, 8);
            p->setPen(Kit::crit); p->drawText(d, Qt::AlignCenter, QStringLiteral("Supprimer"));
            p->restore();
            return;
        }
        }
        p->setFont(f);
        p->setPen(col);
        p->drawText(r, al, p->fontMetrics().elidedText(txt, Qt::ElideRight, r.width()));
        p->restore();
    }

    bool editorEvent(QEvent *e, QAbstractItemModel *, const QStyleOptionViewItem &o, const QModelIndex &ix) override {
        if (ix.column() == 8 && e->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton) {
                const QPoint pt = me->position().toPoint();
                if (editRect(o.rect).contains(pt)) { if (onEdit) onEdit(ix); return true; }
                if (deleteRect(o.rect).contains(pt)) { if (onDelete) onDelete(ix); return true; }
            }
        }
        return false;
    }
};

// ============================================================================ formulaire d'une tâche
class TaskDialog : public QDialog {
public:
    TaskDialog(QWidget *parent, const QString &title, const Task &t, const QVector<Person> &staff, const QVector<Task> &all,
               const QVector<OrderInfo> &orderList = {})
        : QDialog(parent), original(t), people(staff), tasks(all), orderInfos(orderList) {
        setWindowTitle(title);
        setMinimumWidth(480);
        order = new QSpinBox; order->setRange(1, 999999); order->setValue(t.order);
        step = new QComboBox; step->setEditable(true); step->addItems(TaskStore::steps());
        step->setCurrentText(t.step);
        step->lineEdit()->setPlaceholderText("ex. Broderie main");
        start = new QDateEdit(t.start.isValid() ? t.start : QDate::currentDate());
        end = new QDateEdit(t.end.isValid() ? t.end : QDate::currentDate().addDays(7));
        for (auto *d : {start, end}) { d->setCalendarPopup(true); d->setDisplayFormat("dd/MM/yyyy"); }
        prio = new QComboBox; prio->addItems(TaskStore::priorities()); prio->setCurrentText(t.priority);
        progress = new QSpinBox; progress->setRange(0, 100); progress->setSuffix(" %"); progress->setValue(t.progress);
        for (auto *s : {order, progress}) s->setButtonSymbols(QAbstractSpinBox::NoButtons);

        auto *form = new QFormLayout;
        form->setVerticalSpacing(12);
        if (!orderInfos.isEmpty()) {   // commandes réelles du module Commandes
            orderBox = new QComboBox;
            for (const OrderInfo &o : orderInfos)
                orderBox->addItem(o.ref() + "  —  " + o.model + (o.delivery.isValid() ? "  ·  livraison " + fmt(o.delivery) : QString()), o.id);
            if (orderBox->findData(t.order) < 0) orderBox->insertItem(0, QStringLiteral("CMD-%1").arg(t.order, 4, 10, QChar('0')), t.order);
            orderBox->setCurrentIndex(std::max(0, orderBox->findData(t.order)));
            form->addRow("Commande", orderBox);
        } else {
            form->addRow("ID commande", order);
        }
        form->addRow("Étape", step);
        if (!people.isEmpty()) {
            emp = new QComboBox;
            emp->addItem("Non assigné", 0);
            for (const Person &p : people)
                emp->addItem(p.name + "  —  " + p.position + (p.available ? "" : "  (indisponible)"), p.id);
            emp->setCurrentIndex(std::max(0, emp->findData(t.employeeId)));
            auto *suggest = new QPushButton("Suggérer");
            suggest->setCursor(Qt::PointingHandCursor);
            auto *row = new QHBoxLayout;
            row->addWidget(emp, 1); row->addWidget(suggest);
            form->addRow("Employé", row);
            hint = mk("", "muted");
            hint->setWordWrap(true);
            hint->setMinimumHeight(40);
            hint->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            form->addRow("", hint);
            connect(suggest, &QPushButton::clicked, this, [this] { suggestEmployee(); });
        } else {
            empId = new QSpinBox; empId->setRange(0, 999999); empId->setSpecialValueText("Non assigné");
            empId->setValue(t.employeeId); empId->setButtonSymbols(QAbstractSpinBox::NoButtons);
            form->addRow("ID employé", empId);
        }
        form->addRow("Date début", start);
        form->addRow("Date fin prévue", end);
        form->addRow("Priorité", prio);
        form->addRow("Avancement", progress);

        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText("Enregistrer");
        bb->button(QDialogButtonBox::Ok)->setProperty("primary", true);
        bb->button(QDialogButtonBox::Cancel)->setText("Annuler");
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(28, 24, 28, 24);
        lay->setSpacing(18);
        auto *head = mk(title, "section");
        lay->addWidget(head);
        lay->addLayout(form);
        lay->addWidget(bb);
    }

    Task result() const {
        Task t = original;
        t.order = orderBox ? orderBox->currentData().toInt() : order->value(); t.step = step->currentText().trimmed();
        t.employeeId = emp ? emp->currentData().toInt() : empId->value();
        t.start = start->date(); t.end = end->date();
        t.priority = prio->currentText(); t.progress = progress->value();
        return t;
    }
    void accept() override {
        if (step->currentText().trimmed().isEmpty()) { QMessageBox::warning(this, "Erreur", "L'étape est obligatoire."); return; }
        if (end->date() < start->date()) { QMessageBox::warning(this, "Erreur", "La date de fin prévue doit être après la date de début."); return; }
        if (emp) {
            const int id = emp->currentData().toInt();
            for (const Person &p : people)
                if (p.id == id && !p.available && id != original.employeeId) {
                    QMessageBox::warning(this, "Employé indisponible", p.name + " est indisponible. Choisissez un autre employé.");
                    return;
                }
        }
        if (orderBox) {   // la livraison promise au client ne doit pas être dépassée
            const int id = orderBox->currentData().toInt();
            for (const OrderInfo &o : orderInfos)
                if (o.id == id && o.delivery.isValid() && end->date() > o.delivery) {
                    const auto answer = QMessageBox::question(this, "Livraison dépassée",
                        QString("La fin prévue (%1) est après la livraison de %2 (%3).\nEnregistrer quand même ?").arg(fmt(end->date()), o.ref(), fmt(o.delivery)));
                    if (answer != QMessageBox::Yes) return;
                    break;
                }
        }
        QDialog::accept();
    }

private:
    Task original;
    QVector<Person> people;
    QVector<Task> tasks;
    QVector<OrderInfo> orderInfos;
    QSpinBox *order, *progress, *empId = nullptr;
    QComboBox *step, *prio, *emp = nullptr, *orderBox = nullptr;
    QDateEdit *start, *end;
    QLabel *hint = nullptr;

    void suggestEmployee() {
        const QString wanted = step->currentText().trimmed();
        QMap<int, int> open;
        for (const Task &t : tasks) if (!t.done() && t.id != original.id) ++open[t.employeeId];
        const Person *best = nullptr;
        int bestScore = -100000;
        for (const Person &p : people) {
            if (!p.available) continue;
            const bool same = QString::compare(p.specialty, wanted, Qt::CaseInsensitive) == 0;
            const int score = (same ? 100 : 0) - open[p.id] * 10;
            if (score > bestScore) { bestScore = score; best = &p; }
        }
        if (!best) { hint->setText("Aucun employé disponible."); return; }
        emp->setCurrentIndex(emp->findData(best->id));
        const bool same = QString::compare(best->specialty, wanted, Qt::CaseInsensitive) == 0;
        hint->setText(QString("Suggestion : %1 (%2, %3 tâche(s) en cours)")
                          .arg(best->name, same ? "spécialité identique" : "autre spécialité").arg(open[best->id]));
        adjustSize();
    }
};

// ============================================================================ page : tâches
TasksPage::TasksPage(TaskStore *s, QWidget *parent) : QWidget(parent), store(s) {
    model = new TaskModel;
    proxy = new TaskProxy(this);
    proxy->setSourceModel(model);
    proxy->setSortRole(Qt::UserRole);

    stateBox = new QComboBox; stateBox->addItems({"Toutes les tâches", "En retard", "En cours", "Terminées"});
    prioBox = new QComboBox; prioBox->addItems({"Toutes les priorités", "Haute", "Moyenne", "Basse"});
    stateBox->setMinimumWidth(170); prioBox->setMinimumWidth(180);
    count = mk("", "muted");

    auto *bEdit = new QPushButton("Modifier");
    auto *bDel = new QPushButton("Supprimer"); bDel->setProperty("danger", true);
    auto *bPdf = new QPushButton("Exporter PDF");
    auto *bAdd = new QPushButton("+  Nouvelle tâche"); bAdd->setProperty("primary", true);
    for (auto *b : {bEdit, bDel, bPdf, bAdd}) b->setCursor(Qt::PointingHandCursor);

    auto *tool = new QHBoxLayout;
    tool->setSpacing(10);
    tool->addWidget(stateBox); tool->addWidget(prioBox);
    tool->addStretch();
    tool->addWidget(count); tool->addSpacing(8);
    tool->addWidget(bEdit); tool->addWidget(bDel); tool->addWidget(bPdf); tool->addWidget(bAdd);

    view = new QTableView;
    view->setModel(proxy);
    auto *del = new TaskDelegate(view);
    del->onEdit = [this](const QModelIndex &ix) {
        const int id = ix.data(RoleTask).value<Task>().id;
        QTimer::singleShot(0, this, [this, id] { editById(id); });
    };
    del->onDelete = [this](const QModelIndex &ix) {
        const int id = ix.data(RoleTask).value<Task>().id;
        QTimer::singleShot(0, this, [this, id] { deleteById(id); });
    };
    view->setItemDelegate(del);
    view->setMouseTracking(true);
    view->setShowGrid(false);
    view->setFrameShape(QFrame::NoFrame);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setFocusPolicy(Qt::NoFocus);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->verticalHeader()->hide();
    view->verticalHeader()->setDefaultSectionSize(60);
    auto *h = view->horizontalHeader();
    h->setSortIndicatorShown(false);
    h->setHighlightSections(false);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setSectionResizeMode(QHeaderView::Fixed);
    const int widths[] = {80, 92, 0, 160, 96, 100, 100, 124, 176};
    for (int i = 0; i < 9; ++i) if (widths[i]) h->resizeSection(i, widths[i]);
    h->setSectionResizeMode(2, QHeaderView::Stretch);
    h->setMinimumSectionSize(60);
    view->setSortingEnabled(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    h->setSortIndicatorShown(false);

    auto *tableLayout = new QVBoxLayout;
    tableLayout->addWidget(view);
    auto *card = newCard(tableLayout, 10);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 6, 28, 18);
    root->setSpacing(16);
    root->addLayout(tool);
    root->addWidget(card, 1);

    connect(stateBox, &QComboBox::currentIndexChanged, this, [this](int i) { proxy->setState(i); updateCount(); });
    connect(prioBox, &QComboBox::currentIndexChanged, this, [this](int i) { proxy->setPriority(i == 0 ? QString() : prioBox->currentText()); updateCount(); });
    connect(view, &QTableView::doubleClicked, this, [this](const QModelIndex &ix) {
        if (ix.column() != 8) editById(ix.data(RoleTask).value<Task>().id);
    });
    connect(bAdd, &QPushButton::clicked, this, [this] { addTask(); });
    connect(bEdit, &QPushButton::clicked, this, [this] {
        const int id = selectedId();
        if (id) editById(id); else QMessageBox::information(this, "AUREA — Production", "Sélectionnez d'abord une tâche dans le tableau.");
    });
    connect(bDel, &QPushButton::clicked, this, [this] {
        const int id = selectedId();
        if (id) deleteById(id); else QMessageBox::information(this, "AUREA — Production", "Sélectionnez d'abord une tâche dans le tableau.");
    });
    connect(bPdf, &QPushButton::clicked, this, [this] { exportPdf(); });
}

void TasksPage::setTasks(const QVector<Task> &t) { tasks = t; model->setRows(t); updateCount(); }
void TasksPage::setSearch(const QString &text) { proxy->setSearch(text); updateCount(); }
void TasksPage::setStateFilter(int i) { stateBox->setCurrentIndex(i); }
void TasksPage::updateCount() {
    count->setText(QString("%1 sur %2 tâche%3").arg(proxy->rowCount()).arg(tasks.size()).arg(tasks.size() > 1 ? "s" : ""));
}
int TasksPage::selectedId() const {
    const QModelIndex i = view->currentIndex();
    if (!i.isValid() || !view->selectionModel()->hasSelection()) return 0;
    return i.data(RoleTask).value<Task>().id;
}
void TasksPage::addTask() {
    Task t;
    for (const Task &x : tasks) t.order = std::max(t.order, x.order);
    if (tasks.isEmpty()) t.order = 1001;
    const QVector<OrderInfo> real = store->orders();
    if (!real.isEmpty()) t.order = real.last().id;
    t.start = QDate::currentDate(); t.end = QDate::currentDate().addDays(7);
    TaskDialog d(this, "Nouvelle tâche", t, store->people(), tasks, store->orders());
    while (d.exec() == QDialog::Accepted) {
        Task n = d.result();
        if (store->save(n)) { emit changed(); return; }
        QMessageBox::warning(this, "Erreur", store->error());
    }
}
void TasksPage::editById(int id) {
    for (const Task &t : tasks) {
        if (t.id != id) continue;
        TaskDialog d(this, "Modifier la tâche " + t.ref(), t, store->people(), tasks, store->orders());
        while (d.exec() == QDialog::Accepted) {
            Task n = d.result();
            if (store->save(n)) { emit changed(); return; }
            QMessageBox::warning(this, "Erreur", store->error());
        }
        return;
    }
}
void TasksPage::deleteById(int id) {
    for (const Task &t : tasks) {
        if (t.id != id) continue;
        QMessageBox box(QMessageBox::Question, "Confirmer", QString("Supprimer la tâche %1 (%2) ?").arg(t.ref(), t.step), QMessageBox::NoButton, this);
        auto *yes = box.addButton("Supprimer", QMessageBox::AcceptRole);
        yes->setProperty("danger", true);
        auto *no = box.addButton("Annuler", QMessageBox::RejectRole);
        box.setDefaultButton(no);
        box.exec();
        if (box.clickedButton() != yes) return;
        if (!store->remove(id)) { QMessageBox::warning(this, "Erreur", store->error()); return; }
        emit changed();
        return;
    }
}
QString TasksPage::reportHtml() const {
    QString body = "<table width='100%' cellspacing='0' cellpadding='6'>" +
                   headerRow({"Réf", "Commande", "Étape", "Employé", "Début", "Fin prévue", "Priorité", "Avancement"});
    for (int r = 0; r < proxy->rowCount(); ++r) {
        const Task t = proxy->index(r, 0).data(RoleTask).value<Task>();
        body += htmlRow({t.ref(), t.orderRef(), t.step, t.employeeLabel().isEmpty() ? QStringLiteral("—") : t.employeeLabel(),
                         fmt(t.start), fmt(t.end), t.priority, QString::number(t.progress) + " %"});
    }
    return document("Production — Liste des tâches", body + "</table>");
}
void TasksPage::exportPdf() { savePdf(this, "production_taches.pdf", reportHtml(), true); }

// ============================================================================ page : tableau de bord
DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto kpi = [](const QString &title, Kit::Icon icon, QLabel *&value, QLabel *&suffix) {
        auto *v = new QVBoxLayout;
        v->setSpacing(6);
        auto *top = new QHBoxLayout;
        top->addWidget(mk(title, "muted")); top->addStretch(); top->addWidget(iconBox(icon));
        value = mk("0", "value");
        suffix = mk("", "muted");
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(value, 0, Qt::AlignBottom); row->addWidget(suffix, 0, Qt::AlignBottom); row->addStretch();
        v->addLayout(top); v->addLayout(row);
        return newCard(v, 18);
    };
    QLabel *dummy = nullptr;
    auto *k1 = kpi("Tâches", Kit::Icon::List, vTotal, dummy); dummy->setText("au total");
    auto *k2 = kpi("En cours", Kit::Icon::Clock, vActive, vActiveSub);
    auto *k3 = kpi("Terminées", Kit::Icon::Check, vDone, vDonePct); vDonePct->setProperty("ok", true);
    QLabel *dummy2 = nullptr;
    auto *k4 = kpi("Avancement moyen", Kit::Icon::Trend, vAvg, dummy2); dummy2->setText("toutes tâches");
    callout = new QPushButton;
    callout->setObjectName("callout");
    callout->setCursor(Qt::PointingHandCursor);
    callout->setMinimumHeight(92);
    callout->setIconSize(QSize(22, 22));
    connect(callout, &QPushButton::clicked, this, &DashboardPage::lateClicked);

    auto *kpis = new QHBoxLayout;
    kpis->setSpacing(16);
    for (auto *k : {k1, k2, k3, k4}) kpis->addWidget(k, 3);
    kpis->addWidget(callout, 4);

    // colonne gauche : charge + échéances
    chart = new AreaChart;
    auto *cl = new QVBoxLayout;
    cl->setSpacing(10);
    cl->addLayout(cardHeader("Charge de production", "7 prochains jours"));
    cl->addWidget(chart);
    auto *chartCard = newCard(cl, 22);

    dueTable = new QTableWidget(0, 5);
    setupTable(dueTable);
    dueTable->setHorizontalHeaderLabels({"Réf", "Étape", "Employé", "Fin prévue", "Avancement"});
    dueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    dueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    dueTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    dueTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    dueTable->setColumnWidth(0, 96); dueTable->setColumnWidth(3, 108); dueTable->setColumnWidth(4, 100);
    auto *dl = new QVBoxLayout;
    dl->setSpacing(8);
    dl->addLayout(cardHeader("Échéances proches", "Date de fin croissante"));
    dl->addWidget(dueTable);
    auto *dueCard = newCard(dl, 22);

    auto *left = new QVBoxLayout;
    left->setSpacing(18);
    left->addWidget(chartCard); left->addWidget(dueCard); left->addStretch();

    // colonne droite : tâches prioritaires + étape à surveiller
    miniGrid = new QGridLayout;
    miniGrid->setSpacing(12);
    auto *pl = new QVBoxLayout;
    pl->setSpacing(14);
    pl->addLayout(cardHeader("Tâches prioritaires", "Haute priorité en premier"));
    pl->addLayout(miniGrid);
    auto *prioCard = newCard(pl, 22);

    bnName = mk("—", "bigserif");
    bnText = mk("", "muted");
    bnText->setWordWrap(true);
    auto *more = new QPushButton("Voir le suivi de l'avancement  ›");
    more->setObjectName("link");
    more->setCursor(Qt::PointingHandCursor);
    connect(more, &QPushButton::clicked, this, &DashboardPage::trackingClicked);
    auto *bl = new QVBoxLayout;
    bl->setSpacing(8);
    bl->addLayout(cardHeader("Étape qui ralentit l'atelier"));
    bl->addWidget(bnName); bl->addWidget(bnText); bl->addWidget(more);
    auto *bnCard = newCard(bl, 22);

    auto *right = new QVBoxLayout;
    right->setSpacing(18);
    right->addWidget(prioCard); right->addWidget(bnCard); right->addStretch();

    auto *cols = new QHBoxLayout;
    cols->setSpacing(18);
    cols->addLayout(left, 6); cols->addLayout(right, 5);

    auto *content = new QWidget;
    auto *cv = new QVBoxLayout(content);
    cv->setContentsMargins(28, 6, 22, 24);
    cv->setSpacing(18);
    cv->addLayout(kpis); cv->addLayout(cols); cv->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void DashboardPage::setTasks(const QVector<Task> &tasks) {
    int active = 0, done = 0, late = 0, sum = 0;
    for (const Task &t : tasks) { (t.done() ? done : active)++; late += t.late(); sum += t.progress; }
    vTotal->setText(QString::number(tasks.size()));
    vActive->setText(QString::number(active)); vActiveSub->setText(QString("sur %1").arg(tasks.size()));
    vDone->setText(QString::number(done));
    vDonePct->setText(tasks.isEmpty() ? QString() : QString("%1 %").arg(done * 100 / tasks.size()));
    vAvg->setText(tasks.isEmpty() ? "0 %" : QString("%1 %").arg(qRound(sum / double(tasks.size()))));
    if (late > 0) {
        callout->setText(QString("  %1 tâche%2 en retard    ›").arg(late).arg(late > 1 ? "s" : ""));
        callout->setIcon(QIcon(Kit::pixmap(Kit::Icon::Alert, 22, QColor("#2a2418"))));
    } else {
        callout->setText("  Aucun retard    ›");
        callout->setIcon(QIcon(Kit::pixmap(Kit::Icon::Check, 22, QColor("#2a2418"))));
    }

    QStringList labels;
    for (int i = 0; i < 7; ++i) labels << QLocale().toString(QDate::currentDate().addDays(i), "ddd");
    QVector<double> w;
    for (int v : Analytics::workload(tasks, 7)) w << v;
    chart->setData(w, labels);

    // tâches prioritaires (non terminées, priorité puis date de fin)
    clearLayout(miniGrid);
    const QVector<Task> plan = Analytics::planning(tasks);
    for (int i = 0; i < std::min<int>(6, plan.size()); ++i) {
        const Task &t = plan[i];
        auto *f = new QFrame;
        f->setProperty("mini", true);
        auto *v = new QVBoxLayout(f);
        v->setContentsMargins(14, 14, 14, 14);
        v->setSpacing(8);
        auto *top = new QHBoxLayout;
        auto *av = new QLabel(t.employeeName.isEmpty() ? QStringLiteral("–") : initialsOf(t.employeeName));
        av->setObjectName("avatar"); av->setFixedSize(34, 34); av->setAlignment(Qt::AlignCenter);
        auto *names = new QVBoxLayout;
        names->setSpacing(1);
        auto *n = new QLabel(t.step);
        n->setWordWrap(true);
        QFont nf = n->font(); nf.setWeight(QFont::DemiBold); n->setFont(nf);
        names->addWidget(n); names->addWidget(mk(t.orderRef() + "  ·  " + t.priority, "muted"));
        top->addWidget(av, 0, Qt::AlignTop); top->addLayout(names, 1);
        auto *pr = new QHBoxLayout;
        pr->addWidget(mk("Avancement", "muted")); pr->addStretch();
        pr->addWidget(mk(QString::number(t.progress) + " %", "muted"));
        v->addLayout(top); v->addLayout(pr); v->addWidget(bar(t.progress));
        miniGrid->addWidget(f, i / 2, i % 2);
    }
    if (plan.isEmpty()) miniGrid->addWidget(mk("Aucune tâche en cours.", "muted"), 0, 0);

    // échéances proches
    QVector<Task> due;
    for (const Task &t : tasks) if (!t.done()) due.append(t);
    std::sort(due.begin(), due.end(), [](const Task &a, const Task &b) { return a.end != b.end ? a.end < b.end : a.id < b.id; });
    const int rows = std::min<int>(5, due.size());
    dueTable->setRowCount(rows);
    for (int r = 0; r < rows; ++r) {
        const Task &t = due[r];
        dueTable->setItem(r, 0, cell(t.ref(), Kit::muted));
        dueTable->setItem(r, 1, cell(t.step));
        dueTable->setItem(r, 2, cell(t.employeeLabel().isEmpty() ? QStringLiteral("—") : t.employeeLabel()));
        dueTable->setItem(r, 3, cell(fmt(t.end), t.late() ? Kit::crit : Kit::text));
        dueTable->setItem(r, 4, cell(QString::number(t.progress) + " %"));
    }
    dueTable->setFixedHeight(46 + std::max(2, rows) * 46 + 4);

    const QVector<StepDelay> delays = Analytics::delays(tasks);
    if (delays.isEmpty()) {
        bnName->setText("Aucune");
        bnText->setText("Toutes les étapes avancent comme prévu.");
    } else {
        const StepDelay &d = delays.first();
        bnName->setText(d.step);
        bnText->setText(QString("Retard moyen de %1 points sur %2 tâche%3, dont %4 en retard de livraison.")
                            .arg(qRound(d.gap)).arg(d.tasks).arg(d.tasks > 1 ? "s" : "").arg(d.late));
    }
}

// ============================================================================ page : planning automatique
PlanningPage::PlanningPage(QWidget *parent) : QWidget(parent) {
    table = new QTableWidget(0, 6);
    setupTable(table, 52);
    table->setHorizontalHeaderLabels({"#", "Étape", "Commande", "Priorité", "Fin prévue", "Avancement"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int c : {0, 2, 3, 4, 5}) table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    table->setColumnWidth(0, 50); table->setColumnWidth(2, 110); table->setColumnWidth(3, 100);
    table->setColumnWidth(4, 110); table->setColumnWidth(5, 110);
    auto *tl = new QVBoxLayout;
    tl->setSpacing(10);
    tl->addLayout(cardHeader("Ordre de travail", "Priorité puis date de livraison"));
    auto *info = mk("Le planning se calcule seul : les tâches non terminées sont classées par priorité, puis par date de fin prévue.", "muted");
    info->setWordWrap(true);
    tl->addWidget(info);
    tl->addWidget(table, 1);

    calendar = new QCalendarWidget;
    calendar->setGridVisible(false);
    calendar->setFirstDayOfWeek(Qt::Monday);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    QTextCharFormat plain;
    plain.setForeground(Kit::text);
    calendar->setWeekdayTextFormat(Qt::Saturday, plain);
    calendar->setWeekdayTextFormat(Qt::Sunday, plain);
    QTextCharFormat head;
    head.setForeground(Kit::muted);
    calendar->setHeaderTextFormat(head);
    if (auto *prev = calendar->findChild<QToolButton *>("qt_calendar_prevmonth")) {
        QPixmap pm = Kit::pixmap(Kit::Icon::Chevron, 16, Kit::gold);
        prev->setIcon(QIcon(pm.transformed(QTransform().scale(-1, 1))));
    }
    if (auto *next = calendar->findChild<QToolButton *>("qt_calendar_nextmonth")) next->setIcon(QIcon(Kit::pixmap(Kit::Icon::Chevron, 16, Kit::gold)));
    dayTitle = mk("", "section");
    dayList = new QListWidget;
    dayList->setFocusPolicy(Qt::NoFocus);
    auto *cl = new QVBoxLayout;
    cl->setSpacing(12);
    cl->addLayout(cardHeader("Calendrier"));
    cl->addWidget(calendar);
    cl->addWidget(dayTitle);
    cl->addWidget(dayList, 1);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(28, 6, 28, 18);
    root->setSpacing(18);
    root->addWidget(newCard(tl, 22), 6);
    root->addWidget(newCard(cl, 22), 4);
    connect(calendar, &QCalendarWidget::selectionChanged, this, [this] { showDay(); });
}

void PlanningPage::setTasks(const QVector<Task> &tasks) {
    plan = Analytics::planning(tasks);
    table->setRowCount(plan.size());
    for (int r = 0; r < plan.size(); ++r) {
        const Task &t = plan[r];
        table->setItem(r, 0, cell(QString::number(r + 1), Kit::gold));
        table->setItem(r, 1, cell(t.step));
        table->setItem(r, 2, cell(t.orderRef()));
        table->setItem(r, 3, cell(t.priority, prioColor(t.priority)));
        table->setItem(r, 4, cell(fmt(t.end), t.late() ? Kit::crit : Kit::text));
        table->setItem(r, 5, cell(QString::number(t.progress) + " %"));
    }
    // jours occupés : rouge si une tâche Haute y est active, ambre sinon
    calendar->setDateTextFormat(QDate(), QTextCharFormat());
    QMap<QDate, int> rank;
    for (const Task &t : plan)
        for (QDate d = t.start; d <= t.end; d = d.addDays(1)) rank[d] = std::min(rank.value(d, 9), t.priorityRank());
    for (auto it = rank.cbegin(); it != rank.cend(); ++it) {
        QTextCharFormat f;
        f.setBackground(it.value() == 0 ? QColor(226, 104, 95, 70) : it.value() == 1 ? QColor(226, 169, 79, 60) : QColor(127, 192, 139, 45));
        f.setForeground(QColor("#fbf7ee"));
        calendar->setDateTextFormat(it.key(), f);
    }
    showDay();
}

void PlanningPage::showDay() {
    const QDate d = calendar->selectedDate();
    dayTitle->setText("Tâches du " + QLocale().toString(d, "d MMMM yyyy"));
    dayList->clear();
    for (int i = 0; i < plan.size(); ++i) {
        const Task &t = plan[i];
        if (t.start <= d && d <= t.end) {
            auto *it = new QListWidgetItem(QString("%1   %2 · %3 · %4%5").arg(i + 1, 2, 10, QChar('0')).arg(t.step, t.orderRef(), t.priority)
                                               .arg(t.end == d ? "   (livraison)" : ""));
            it->setForeground(prioColor(t.priority));
            dayList->addItem(it);
        }
    }
    if (dayList->count() == 0) {
        auto *it = new QListWidgetItem("Aucune tâche prévue ce jour.");
        it->setForeground(Kit::muted);
        it->setFlags(Qt::NoItemFlags);
        dayList->addItem(it);
    }
}

// ============================================================================ page : suivi de l'avancement
TrackingPage::TrackingPage(QWidget *parent) : QWidget(parent) {
    ordersLayout = new QVBoxLayout;
    ordersLayout->setSpacing(10);
    auto *ol = new QVBoxLayout;
    ol->setSpacing(14);
    ol->addLayout(cardHeader("Avancement par commande", "Moyenne des tâches"));
    ol->addLayout(ordersLayout);
    ol->addStretch();

    stepName = mk("—", "bigserif");
    stepText = mk("", "muted");
    stepText->setWordWrap(true);
    delayChart = new BarChart;
    auto *method = mk("Retard = avancement attendu d'après les dates − avancement réel.", "muted");
    method->setWordWrap(true);
    auto *sl = new QVBoxLayout;
    sl->setSpacing(10);
    sl->addLayout(cardHeader("Étape qui ralentit l'atelier"));
    sl->addWidget(stepName); sl->addWidget(stepText);
    sl->addSpacing(8);
    sl->addWidget(mk("Retard moyen par étape", "muted"));
    sl->addWidget(delayChart);
    sl->addStretch();
    sl->addWidget(method);

    auto *scrollContent = new QWidget;
    auto *root = new QHBoxLayout(scrollContent);
    root->setContentsMargins(28, 6, 22, 18);
    root->setSpacing(18);
    auto *left = newCard(ol, 22), *right = newCard(sl, 22);
    root->addWidget(left, 5, Qt::AlignTop);
    root->addWidget(right, 5, Qt::AlignTop);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(scrollContent);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

void TrackingPage::setTasks(const QVector<Task> &tasks) {
    clearLayout(ordersLayout);
    const QVector<OrderSummary> orders = Analytics::orders(tasks);
    for (const OrderSummary &o : orders) {
        auto *f = new QFrame;
        f->setProperty("row", true);
        auto *g = new QGridLayout(f);
        g->setContentsMargins(16, 12, 16, 12);
        g->setHorizontalSpacing(14);
        g->setVerticalSpacing(8);
        auto *title = new QLabel(QStringLiteral("CMD-%1").arg(o.order, 4, 10, QChar('0')));
        QFont tf = title->font(); tf.setWeight(QFont::DemiBold); title->setFont(tf);
        auto *info = mk(QString("%1 tâche%2").arg(o.tasks).arg(o.tasks > 1 ? "s" : "") + (o.late ? QString("  ·  %1 en retard").arg(o.late) : QString()), "muted");
        if (o.late) info->setProperty("crit", true);
        auto *pct = new QLabel(QString::number(o.progress) + " %");
        g->addWidget(title, 0, 0); g->addWidget(info, 0, 1); g->addWidget(pct, 0, 2, Qt::AlignRight);
        g->setColumnStretch(1, 1);
        g->addWidget(bar(o.progress), 1, 0, 1, 3);
        ordersLayout->addWidget(f);
    }
    if (orders.isEmpty()) ordersLayout->addWidget(mk("Aucune commande en production.", "muted"));

    const QVector<StepDelay> delays = Analytics::delays(tasks);
    QVector<QPair<QString, double>> rows;
    for (int i = 0; i < std::min<int>(6, delays.size()); ++i) rows.append({delays[i].step, delays[i].gap});
    delayChart->setData(rows, " pts");
    if (delays.isEmpty()) {
        stepName->setText("Aucune");
        stepText->setText("Toutes les étapes avancent comme prévu : aucun retard détecté.");
    } else {
        const StepDelay &d = delays.first();
        stepName->setText(d.step);
        stepText->setText(QString("Cette étape accuse en moyenne %1 points de retard sur %2 tâche%3, dont %4 dépassent leur date de livraison.")
                              .arg(qRound(d.gap)).arg(d.tasks).arg(d.tasks > 1 ? "s" : "").arg(d.late));
    }
}

// ============================================================================ page : statistiques
StatsPage::StatsPage(QWidget *parent) : QWidget(parent) {
    stepChart = new BarChart;
    auto *sl = new QVBoxLayout;
    sl->setSpacing(14);
    sl->addLayout(cardHeader("Tâches par étape"));
    sl->addWidget(stepChart);
    sl->addStretch();

    prioLayout = new QVBoxLayout; prioLayout->setSpacing(10);
    stateLayout = new QVBoxLayout; stateLayout->setSpacing(10);
    auto *rl = new QVBoxLayout;
    rl->setSpacing(14);
    rl->addLayout(cardHeader("Répartition par priorité"));
    rl->addLayout(prioLayout);
    rl->addSpacing(10);
    rl->addLayout(cardHeader("Répartition par état"));
    rl->addLayout(stateLayout);
    rl->addStretch();

    auto *bPdf = new QPushButton("Exporter PDF");
    bPdf->setCursor(Qt::PointingHandCursor);
    connect(bPdf, &QPushButton::clicked, this, [this] { exportPdf(); });
    auto *tool = new QHBoxLayout;
    tool->addStretch(); tool->addWidget(bPdf);

    auto *cols = new QHBoxLayout;
    cols->setSpacing(18);
    cols->addWidget(newCard(sl, 22), 6, Qt::AlignTop);
    cols->addWidget(newCard(rl, 22), 4, Qt::AlignTop);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 6, 28, 18);
    root->setSpacing(16);
    root->addLayout(tool); root->addLayout(cols); root->addStretch();
}

void StatsPage::setTasks(const QVector<Task> &t) {
    tasks = t;
    QMap<QString, int> steps;
    for (const Task &x : t) ++steps[x.step];
    QVector<QPair<QString, double>> rows;
    for (auto it = steps.cbegin(); it != steps.cend(); ++it) rows.append({it.key(), double(it.value())});
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    stepChart->setData(rows);

    auto fill = [&t](QVBoxLayout *l, const QVector<QPair<QString, int>> &items) {
        clearLayout(l);
        for (const auto &it : items) {
            auto *g = new QGridLayout;
            g->setHorizontalSpacing(12);
            g->addWidget(new QLabel(it.first), 0, 0);
            g->addWidget(mk(QString::number(it.second), "muted"), 0, 1, Qt::AlignRight);
            g->addWidget(bar(t.isEmpty() ? 0 : it.second * 100 / t.size()), 1, 0, 1, 2);
            g->setColumnStretch(0, 1);
            l->addLayout(g);
        }
    };
    int hi = 0, mid = 0, lo = 0, done = 0, late = 0, running = 0;
    for (const Task &x : t) {
        (x.priority == "Haute" ? hi : x.priority == "Moyenne" ? mid : lo)++;
        if (x.done()) ++done; else if (x.late()) ++late; else ++running;
    }
    fill(prioLayout, {{"Haute", hi}, {"Moyenne", mid}, {"Basse", lo}});
    fill(stateLayout, {{"Terminées", done}, {"En cours", running}, {"En retard", late}});
}

void StatsPage::exportPdf() {
    QMap<QString, int> steps, prios;
    int done = 0, late = 0, running = 0;
    for (const Task &t : tasks) {
        ++steps[t.step]; ++prios[t.priority];
        if (t.done()) ++done; else if (t.late()) ++late; else ++running;
    }
    auto table = [](const QString &title, const QMap<QString, int> &m) {
        QString s = "<h3>" + title + "</h3><table width='60%' cellspacing='0' cellpadding='6'>" + headerRow({title, "Tâches"});
        for (auto it = m.cbegin(); it != m.cend(); ++it) s += htmlRow({it.key(), QString::number(it.value())});
        return s + "</table>";
    };
    QString body = QString("<p>%1 tâche(s) · %2 terminée(s) · %3 en cours · %4 en retard</p>").arg(tasks.size()).arg(done).arg(running).arg(late);
    body += table("Étape", steps) + table("Priorité", prios);
    const QVector<StepDelay> delays = Analytics::delays(tasks);
    if (!delays.isEmpty())
        body += QString("<h3>Étape qui ralentit l'atelier</h3><p><b>%1</b> — retard moyen de %2 points sur %3 tâche(s).</p>")
                    .arg(delays.first().step.toHtmlEscaped()).arg(qRound(delays.first().gap)).arg(delays.first().tasks);
    savePdf(this, "production_statistiques.pdf", document("Production — Statistiques", body), false);
}
