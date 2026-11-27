#pragma once

#include <QMainWindow>
#include <QThread>
#include "generation.h"

class QTreeWidget;
class QPlainTextEdit;
class QListWidget;
class QPushButton;
class GenerateWorker;
class QTreeWidgetItem;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

signals:
	// этот сигнал посылаем сами себе из обработчика кнопки
	void enqueueTask(const GenerationTask &task);
private slots:
	void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onGenerateClicked();
	void onQueueEmpty();

private:
	void buildTree();
	QTreeWidgetItem* addTreeItem(QTreeWidgetItem* root, const QString &name, const QString &value);
	QString composePrompt(QTreeWidgetItem *root) const;
	GenerationParams composeParams() const;

	QTreeWidget *m_tree;
	QPlainTextEdit *m_promptView;
	QListWidget *m_queueView;
	QPushButton *m_generateBtn;

	QThread m_workerThread;
	GenerateWorker *m_worker;

	QTreeWidgetItem *m_itemPos;
	QTreeWidgetItem *m_itemNeg;
	QTreeWidgetItem *m_itemCfg;
};
