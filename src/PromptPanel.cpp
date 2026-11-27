#include "promptpanel.h"

#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QClipboard>
#include <QGuiApplication>
#include <functional>

PromptPanel::PromptPanel(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    // --- toolbar ---
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QAction *actPaste = m_toolbar->addAction("Insert text");
    actPaste->setToolTip("Insert clipboard text into the current node");
    connect(actPaste, &QAction::triggered, this, &PromptPanel::insertFromClipboard);

    QAction *actAdd = m_toolbar->addAction("Add node");
    connect(actAdd, &QAction::triggered, this, &PromptPanel::addNode);

    QAction *actDel = m_toolbar->addAction("Remove node");
    connect(actDel, &QAction::triggered, this, &PromptPanel::removeCurrentNode);

    // --- tree ---
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabel(title);
    m_tree->setColumnCount(1);
    connect(m_tree, &QTreeWidget::itemClicked,
            this, &PromptPanel::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &PromptPanel::onItemChanged);

    // --- view ---
    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setPlaceholderText("Composed prompt appears here");

    // --- layout ---
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_toolbar);
    leftLayout->addWidget(m_tree, 1);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftWidget);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(splitter);

    // --- initial content: root with one leaf ---
    auto *rootItem = new QTreeWidgetItem(m_tree, { title });
    rootItem->setFlags(rootItem->flags() | Qt::ItemIsUserCheckable);
    rootItem->setCheckState(0, Qt::Checked);
    rootItem->setData(0, Qt::UserRole, "root");

    createLeaf(rootItem, "text", "");
    rootItem->setExpanded(true);
    m_currentNode = rootItem->child(0);
    m_tree->setCurrentItem(m_currentNode);
}

QTreeWidgetItem *PromptPanel::createLeaf(QTreeWidgetItem *parent,
                                         const QString &name,
                                         const QString &value)
{
    auto *item = new QTreeWidgetItem(parent, { name });
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole, name);
    item->setData(0, Qt::UserRole + 1, value);
    return item;
}

QTreeWidgetItem *PromptPanel::findInsertionTarget(QTreeWidgetItem *node) const
{
    if (!node)
        return nullptr;
    // if node has text -> insert directly
    if (node->data(0, Qt::UserRole + 1).isValid())
        return node;
    // else first child with text, or first child at all
    for (int i = 0; i < node->childCount(); ++i) {
        if (node->child(i)->data(0, Qt::UserRole + 1).isValid())
            return node->child(i);
    }
    return node->childCount() > 0 ? node->child(0) : nullptr;
}

void PromptPanel::onTreeItemClicked(QTreeWidgetItem *item, int)
{
    if (item) {
        m_currentNode = item;
    }
}

void PromptPanel::onItemChanged(QTreeWidgetItem *, int column)
{
    if (column == 0)
        refreshView();
}

void PromptPanel::refreshView()
{
    m_view->setPlainText(composeText());
    emit textChanged();
}

QString PromptPanel::composeText() const
{
    QStringList parts;
    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
        if (item->checkState(0) != Qt::Checked)
            return;
        const QVariant text = item->data(0, Qt::UserRole + 1);
        if (text.isValid()) {
            const QString s = text.toString().trimmed();
            if (!s.isEmpty())
                parts << s;
            return;
        }
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        walk(m_tree->topLevelItem(i));
    return parts.join(", ");
}

void PromptPanel::insertFromClipboard()
{
    QTreeWidgetItem *target = findInsertionTarget(m_currentNode);
    if (!target) {
        // no target at all -> create one under top-level root
        QTreeWidgetItem *rootItem = m_tree->topLevelItem(0);
        if (!rootItem)
            return;
        target = createLeaf(rootItem, "text", "");
    }

    const QString clip = QGuiApplication::clipboard()->text().trimmed();
    if (clip.isEmpty())
        return;

    const QString old = target->data(0, Qt::UserRole + 1).toString();
    const QString updated = old.isEmpty() ? clip : (old + ", " + clip);
    target->setData(0, Qt::UserRole + 1, updated);
    m_tree->setCurrentItem(target);
    m_currentNode = target;
    refreshView();
}

void PromptPanel::addNode()
{
    if (!m_currentNode)
        return;
    // add a sibling leaf to current node (or child, if current is root)
    QTreeWidgetItem *parent = m_currentNode->parent();
    if (!parent)
        parent = m_currentNode;
    QTreeWidgetItem *leaf = createLeaf(parent, "text", "");
    parent->setExpanded(true);
    m_tree->setCurrentItem(leaf);
    m_currentNode = leaf;
    refreshView();
}

void PromptPanel::removeCurrentNode()
{
    if (!m_currentNode)
        return;
    QTreeWidgetItem *parent = m_currentNode->parent();
    if (!parent)
        return;  // refuse to remove root
    delete m_currentNode;
    m_currentNode = parent;
    m_tree->setCurrentItem(parent);
    refreshView();
}
