#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QToolBar;

class PromptPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PromptPanel(const QString &title, QWidget *parent = nullptr);

    QString composeText() const;
    QString title() const { return m_title; }

public slots:
    void insertFromClipboard();
    void addNode();	// default: sibling after current / child of root if root selected
	void addChildNode();       // explicit: child at the beginning of current node's children
    void removeCurrentNode();
	void onViewTextChanged();
signals:
    void textChanged();

private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void refreshView();

private:
    QTreeWidgetItem *createLeaf(QTreeWidgetItem *parent,
                                const QString &name,
                                const QString &value,
								int index = -1);
	bool isRoot(QTreeWidgetItem *item) const;
	void selectNode(QTreeWidgetItem *item);
	void refreshComposedView();
	void refreshEditableView();
	void updateToolbarState();

    QTreeWidgetItem *findInsertionTarget(QTreeWidgetItem *node) const;

    QString m_title;
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_view = nullptr;
    QToolBar *m_toolbar = nullptr;
	QTreeWidgetItem *m_root = nullptr;
    QTreeWidgetItem *m_currentNode = nullptr;
	QAction *m_actPaste = nullptr;
	QAction *m_actAdd = nullptr;
	QAction *m_actAddCh = nullptr;
	QAction *m_actDel = nullptr;
	bool m_updatingView = false;                 // guard against feedback loops
};
