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
    void addNode();
    void removeCurrentNode();

signals:
    void textChanged();

private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void refreshView();

private:
    QTreeWidgetItem *createLeaf(QTreeWidgetItem *parent,
                                const QString &name,
                                const QString &value);
    QTreeWidgetItem *findInsertionTarget(QTreeWidgetItem *node) const;

    QString m_title;
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_view = nullptr;
    QToolBar *m_toolbar = nullptr;
    QTreeWidgetItem *m_currentNode = nullptr;
};
