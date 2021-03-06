#include "RegisterRefsWidget.h"
#include "ui_RegisterRefsWidget.h"
#include "MainWindow.h"
#include "utils/Helpers.h"
#include <QMenu>
#include <QClipboard>

RegisterRefModel::RegisterRefModel(QList<RegisterRefDescription> *registerRefs, QObject *parent)
    : QAbstractListModel(parent),
      registerRefs(registerRefs)
{
}

int RegisterRefModel::rowCount(const QModelIndex &) const
{
    return registerRefs->count();
}

int RegisterRefModel::columnCount(const QModelIndex &) const
{
    return RegisterRefModel::ColumnCount;
}

QVariant RegisterRefModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= registerRefs->count())
        return QVariant();

    const RegisterRefDescription &registerRef = registerRefs->at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case RegColumn:
            return registerRef.reg;
        case ValueColumn:
            return registerRef.value;
        case RefColumn:
            return registerRef.ref;
        default:
            return QVariant();
        }
    case RegisterRefDescriptionRole:
        return QVariant::fromValue(registerRef);
    default:
        return QVariant();
    }
}

QVariant RegisterRefModel::headerData(int section, Qt::Orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case RegColumn:
            return tr("Register");
        case ValueColumn:
            return tr("Value");
        case RefColumn:
            return tr("Reference");
        default:
            return QVariant();
        }
    default:
        return QVariant();
    }
}

void RegisterRefModel::beginReloadRegisterRef()
{
    beginResetModel();
}

void RegisterRefModel::endReloadRegisterRef()
{
    endResetModel();
}

RegisterRefProxyModel::RegisterRefProxyModel(RegisterRefModel *sourceModel, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(sourceModel);
}

bool RegisterRefProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    QModelIndex index = sourceModel()->index(row, 0, parent);
    RegisterRefDescription item = index.data(
                                      RegisterRefModel::RegisterRefDescriptionRole).value<RegisterRefDescription>();
    return item.reg.contains(filterRegExp());
}

bool RegisterRefProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    RegisterRefDescription leftRegRef = left.data(
                                            RegisterRefModel::RegisterRefDescriptionRole).value<RegisterRefDescription>();
    RegisterRefDescription rightRegRef = right.data(
                                             RegisterRefModel::RegisterRefDescriptionRole).value<RegisterRefDescription>();

    switch (left.column()) {
    case RegisterRefModel::RegColumn:
        return leftRegRef.reg < rightRegRef.reg;
    case RegisterRefModel::RefColumn:
        return leftRegRef.ref < rightRegRef.ref;
    case RegisterRefModel::ValueColumn:
        return leftRegRef.value < rightRegRef.value;
    default:
        break;
    }

    return leftRegRef.reg < rightRegRef.reg;
}

RegisterRefsWidget::RegisterRefsWidget(MainWindow *main, QAction *action) :
    CutterDockWidget(main, action),
    ui(new Ui::RegisterRefsWidget)
{
    ui->setupUi(this);

    registerRefModel = new RegisterRefModel(&registerRefs, this);
    registerRefProxyModel = new RegisterRefProxyModel(registerRefModel, this);
    ui->registerRefTreeView->setModel(registerRefProxyModel);
    ui->registerRefTreeView->sortByColumn(RegisterRefModel::RegColumn, Qt::AscendingOrder);

    actionCopyValue = new QAction(tr("Copy register value"));
    actionCopyRef = new QAction(tr("Copy register reference"));

    // Ctrl-F to show/hide the filter entry
    QShortcut *search_shortcut = new QShortcut(QKeySequence::Find, this);
    connect(search_shortcut, &QShortcut::activated, ui->quickFilterView, &QuickFilterView::showFilter);
    search_shortcut->setContext(Qt::WidgetWithChildrenShortcut);

    connect(ui->quickFilterView, SIGNAL(filterTextChanged(const QString &)), registerRefProxyModel,
            SLOT(setFilterWildcard(const QString &)));
    connect(ui->quickFilterView, SIGNAL(filterClosed()), ui->registerRefTreeView, SLOT(setFocus()));
    setScrollMode();
    connect(Core(), &CutterCore::refreshAll, this, &RegisterRefsWidget::refreshRegisterRef);
    connect(Core(), &CutterCore::registersChanged, this, &RegisterRefsWidget::refreshRegisterRef);
    connect(actionCopyValue, &QAction::triggered, [ = ] () {
        copyClip(RegisterRefModel::ValueColumn);
    });
    connect(actionCopyRef, &QAction::triggered, [ = ] () {
        copyClip(RegisterRefModel::RefColumn);
    });
    ui->registerRefTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->registerRefTreeView, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(showRegRefContextMenu(const QPoint &)));
}

RegisterRefsWidget::~RegisterRefsWidget() {}

void RegisterRefsWidget::refreshRegisterRef()
{
    registerRefModel->beginReloadRegisterRef();
    registerRefs = Core()->getRegisterRefs();
    registerRefModel->endReloadRegisterRef();

    ui->registerRefTreeView->resizeColumnToContents(0);
    ui->registerRefTreeView->resizeColumnToContents(1);
    ui->registerRefTreeView->resizeColumnToContents(2);
}

void RegisterRefsWidget::setScrollMode()
{
    qhelpers::setVerticalScrollMode(ui->registerRefTreeView);
}

void RegisterRefsWidget::on_registerRefTreeView_doubleClicked(const QModelIndex &index)
{
    RegisterRefDescription item = index.data(
                                      RegisterRefModel::RegisterRefDescriptionRole).value<RegisterRefDescription>();
    Core()->seek(item.value);
}

void RegisterRefsWidget::showRegRefContextMenu(const QPoint &pt)
{
    QMenu *menu = new QMenu(ui->registerRefTreeView);
    menu->clear();
    menu->addAction(actionCopyValue);
    menu->addAction(actionCopyRef);

    menu->exec(ui->registerRefTreeView->viewport()->mapToGlobal(pt));
    delete menu;
}

void RegisterRefsWidget::copyClip(int column)
{
    int row = ui->registerRefTreeView->selectionModel()->currentIndex().row();
    QString value = ui->registerRefTreeView->selectionModel()->currentIndex().sibling(row,
                                                                                      column).data().toString();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(value);
}