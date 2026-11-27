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
#include <QSignalBlocker>
#include <functional>

PromptPanel::PromptPanel(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
	// --- toolbar ---
	m_toolbar = new QToolBar(this);
	m_toolbar->setIconSize(QSize(16, 16));
	m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

	m_actPaste = m_toolbar->addAction("Insert text");
	m_actPaste->setToolTip("Insert clipboard text into the current node");
	connect(m_actPaste, &QAction::triggered, this, &PromptPanel::insertFromClipboard);

	m_actAdd = m_toolbar->addAction("Add node");
	connect(m_actAdd, &QAction::triggered, this, &PromptPanel::addNode);

	m_actAddCh = m_toolbar->addAction("Add child node");
	connect(m_actAddCh, &QAction::triggered, this, &PromptPanel::addChildNode);

	m_actDel = m_toolbar->addAction("Remove node");
	connect(m_actDel, &QAction::triggered, this, &PromptPanel::removeCurrentNode);

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
	m_view->setPlaceholderText("Composed prompt appears here (root selected)");
	connect(m_view, &QPlainTextEdit::textChanged,
		this, &PromptPanel::onViewTextChanged);

	// --- layout: tree left, view right ---
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

	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->addWidget(splitter);

	// --- initial content: root + one leaf ---
	m_root = new QTreeWidgetItem(m_tree, { title });
	m_root->setFlags((m_root->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
	m_root->setCheckState(0, Qt::Checked);
	m_root->setData(0, Qt::UserRole, "root");

	QTreeWidgetItem *firstLeaf = createLeaf(m_root, "text", "");
	m_root->setExpanded(true);

	m_currentNode = firstLeaf;
	m_tree->setCurrentItem(firstLeaf);
	selectNode(firstLeaf);
}

QTreeWidgetItem *PromptPanel::createLeaf(QTreeWidgetItem *parent,
                                         const QString &name,
                                         const QString &value,
										 int index /* = -1 */)
{
	auto *item = new QTreeWidgetItem();
	item->setText(0, name);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setCheckState(0, Qt::Checked);
	item->setData(0, Qt::UserRole, name);
	item->setData(0, Qt::UserRole + 1, value);

	if (index < 0 || index >= parent->childCount())
		parent->addChild(item);
	else
		parent->insertChild(index, item);

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

bool PromptPanel::isRoot(QTreeWidgetItem *item) const
{
	return item == m_root;
}

void PromptPanel::onTreeItemClicked(QTreeWidgetItem *item, int)
{
	if (!item)
		return;
	m_currentNode = item;
	selectNode(item);
}

void PromptPanel::onItemChanged(QTreeWidgetItem *item, int column)
{
	if (column != 0)
		return;
	// checkbox or label change -> if root is currently shown, refresh composition
	if (isRoot(m_currentNode)) {
		refreshComposedView();
		emit textChanged();
	}
	else if (item == m_currentNode) {
		// name of current leaf changed; nothing to do for view text
	}
}

void PromptPanel::onViewTextChanged()
{
	if (m_updatingView)
		return;
	if (!m_currentNode || isRoot(m_currentNode))
		return;
	// write edit back into the node
	m_currentNode->setData(0, Qt::UserRole + 1, m_view->toPlainText());
	emit textChanged();
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

void PromptPanel::selectNode(QTreeWidgetItem *item)
{
	if (!item)
		return;

	if (isRoot(item)) {
		// root: composed, read-only, no editing controls
		m_view->setReadOnly(true);
		m_view->setPlaceholderText("Composed prompt (read-only)");
		refreshComposedView();
	}
	else {
		// leaf: edit the node text directly
		m_view->setReadOnly(false);
		m_view->setPlaceholderText("Node text");
		refreshEditableView();
	}
	updateToolbarState();
}

void PromptPanel::refreshComposedView()
{
	m_updatingView = true;
	{
		QSignalBlocker blocker(m_view);
		m_view->setPlainText(composeText());
	}
	m_updatingView = false;
}

void PromptPanel::refreshEditableView()
{
	if (!m_currentNode)
		return;
	m_updatingView = true;
	{
		QSignalBlocker blocker(m_view);
		m_view->setPlainText(
			m_currentNode->data(0, Qt::UserRole + 1).toString());
	}
	m_updatingView = false;
}

void PromptPanel::updateToolbarState()
{
	const bool rootSelected = isRoot(m_currentNode);
	m_actPaste->setEnabled(!rootSelected);
	m_actAdd->setEnabled(true);                 // add works for both root and leaves
	m_actDel->setEnabled(!rootSelected && m_currentNode->parent() != nullptr);
}

void PromptPanel::insertFromClipboard()
{
	if (!m_currentNode || isRoot(m_currentNode))
		return;

	const QString clip = QGuiApplication::clipboard()->text().trimmed();
	if (clip.isEmpty())
		return;

	const QString old = m_currentNode->data(0, Qt::UserRole + 1).toString();
	const QString updated = old.isEmpty() ? clip : (old + ", " + clip);

	m_currentNode->setData(0, Qt::UserRole + 1, updated);
	m_tree->setCurrentItem(m_currentNode);
	refreshEditableView();
	emit textChanged();
}

void PromptPanel::addNode()
{
	if (!m_currentNode)
		return;

	QTreeWidgetItem *newItem = nullptr;

	if (isRoot(m_currentNode)) {
		newItem = createLeaf(m_root, "text", "", 0);      // beginning of root's children
	}
	else {
		QTreeWidgetItem *parent = m_currentNode->parent();
		if (!parent)
			parent = m_root;
		const int idx = parent->indexOfChild(m_currentNode) + 1;  // right after current
		newItem = createLeaf(parent, "text", "", idx);
		parent->setExpanded(true);
	}

	m_tree->setCurrentItem(newItem);
	m_currentNode = newItem;
	selectNode(newItem);
	emit textChanged();
}

void PromptPanel::addChildNode()
{
	if (!m_currentNode)
		return;

	QTreeWidgetItem *newItem = createLeaf(m_currentNode, "text", "", 0);  // first child
	m_currentNode->setExpanded(true);

	m_tree->setCurrentItem(newItem);
	m_currentNode = newItem;
	selectNode(newItem);
	emit textChanged();
}

void PromptPanel::removeCurrentNode()
{
	if (!m_currentNode || isRoot(m_currentNode))
		return;
	QTreeWidgetItem *parent = m_currentNode->parent();
	if (!parent)
		return;

	delete m_currentNode;
	m_currentNode = parent;
	m_tree->setCurrentItem(parent);
	selectNode(parent);
	emit textChanged();
}
