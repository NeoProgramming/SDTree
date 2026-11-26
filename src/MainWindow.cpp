#include "mainwindow.h"
#include "generateworker.h"

#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QUuid>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	// регистрация типа для передачи
	qRegisterMetaType<GenerationTask>("GenerationTask");

	// ---- компоновка ----
	auto *central = new QWidget(this);
	auto *mainLayout = new QHBoxLayout(central);

	auto *leftSplitter = new QSplitter(Qt::Vertical, this);
	m_tree = new QTreeWidget(this);
	m_tree->setHeaderLabel("Project");
	m_promptView = new QPlainTextEdit(this);
	m_promptView->setReadOnly(true);
	m_promptView->setPlaceholderText("The final prompt will appear here...");

	leftSplitter->addWidget(m_tree);
	leftSplitter->addWidget(m_promptView);
	leftSplitter->setStretchFactor(0, 1);
	leftSplitter->setStretchFactor(1, 1);

	auto *rightWidget = new QWidget(this);
	auto *rightLayout = new QVBoxLayout(rightWidget);
	rightLayout->addWidget(new QLabel("Task queue / log"));
	m_queueView = new QListWidget(this);
	m_generateBtn = new QPushButton("Add to Generate Queue", this);
	rightLayout->addWidget(m_queueView);
	rightLayout->addWidget(m_generateBtn);

	mainLayout->addWidget(leftSplitter, 3);
	mainLayout->addWidget(rightWidget, 1);

	setCentralWidget(central);
	setWindowTitle("SD GUI - MVP");
	resize(1100, 700);

	// ---- дерево ----
	buildTree();

	// ---- воркер в отдельном потоке ----
	m_worker = new GenerateWorker; // без parent, переедет в поток
	m_worker->moveToThread(&m_workerThread);

	
	
	connect(&m_workerThread, &QThread::finished,
		m_worker, &QObject::deleteLater);

	connect(this, &MainWindow::enqueueTask,
		m_worker, &GenerateWorker::enqueueTask);
		
	connect(m_worker, &GenerateWorker::taskStarted, this,
		[this](const QString &id, int cur, int total) {
		// обновить строку статуса задачи в m_queueView
	});

	// Приём сигналов Worker
	connect(m_worker, &GenerateWorker::imageSaved, this,
		[this](const QString &id, const QString &path, qint64 seed) {
		m_queueView->addItem(QString(" ok %1  (seed: %2)").arg(path).arg(seed));
		// здесь же можно записать .sdp: path без .N.png + ".sdp"
	});

	connect(m_worker, &GenerateWorker::taskFinished, this,
		[this](const QString &id, bool ok, const QString &err) {
		// пометить задачу как завершённую/ошибочную
		m_generateBtn->setEnabled(true);
	});

	connect(m_worker, &GenerateWorker::queueEmpty, 
		this, &MainWindow::onQueueEmpty);
	
		
	m_workerThread.start();


	// ---- сигналы GUI ----
	connect(m_tree, &QTreeWidget::itemClicked,
		this, &MainWindow::onTreeItemClicked);
	connect(m_generateBtn, &QPushButton::clicked,
		this, &MainWindow::onGenerateClicked);
}

MainWindow::~MainWindow()
{
	m_workerThread.quit();
	m_workerThread.wait();
}

QTreeWidgetItem* MainWindow::addTreeItem(QTreeWidgetItem* root, const QString &name, const QString &value)
{
	auto *fragment = new QTreeWidgetItem(root, { name });
	fragment->setFlags(fragment->flags() | Qt::ItemIsUserCheckable);
	fragment->setCheckState(0, Qt::Checked);

	fragment->setData(0, Qt::UserRole, name);
	// значение самого фрагмента храним в отдельной роли, чтобы не путать с подписью
	fragment->setData(0, Qt::UserRole + 1, value);

	return fragment;
}

void MainWindow::buildTree()
{
	m_tree->clear();

	auto *root = new QTreeWidgetItem(m_tree, { "Prompt" });
	root->setFlags(root->flags() | Qt::ItemIsUserCheckable);
	root->setCheckState(0, Qt::Checked);
	root->setData(0, Qt::UserRole, "prompt_root");

	auto *settings = addTreeItem(root, "Settings" , "cfg");
	addTreeItem(settings, "repeatCount", "10");
	addTreeItem(settings, "baseSeed", "-1");
	addTreeItem(settings, "seedDelta", "1");

	auto *fragment = addTreeItem(root, "prompt_text", "a beautiful landscape, mountains, lake, sunset");
		

	root->setExpanded(true);
	m_tree->addTopLevelItem(root);
}

GenerationParams MainWindow::composeParams() const
{
	GenerationParams params;

	// Обход дерева:
	std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
		const QVariant name = item->data(0, Qt::UserRole);
		const QVariant value = item->data(0, Qt::UserRole+1);
		if (name == "repeatCount")
			params.repeatCount = value.toInt();
		else if (name == "baseSeed")
			params.baseSeed = value.toInt();
		else if(name == "seedDelta")
			params.seedDelta = value.toInt();
		// дочерние элементы
		for (int i = 0; i < item->childCount(); ++i)
			walk(item->child(i));
	};

	// неверно, нужно просто начинать от фиксированного элемента Settings
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
		walk(m_tree->topLevelItem(i));
	return params;
}

QString MainWindow::composePrompt() const
{
	QStringList parts;
	// Обход дерева: собираем только включённые листовые узлы-фрагменты
	std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *item) {
		if (item->checkState(0) != Qt::Checked)
			return;
		const QVariant name = item->data(0, Qt::UserRole);
		const QVariant text = item->data(0, Qt::UserRole + 1);
		if (name == "prompt_text" && text.isValid() && !text.toString().isEmpty()) {
			parts << text.toString();
		}
		// дочерние элементы
		for (int i = 0; i < item->childCount(); ++i)
			walk(item->child(i));
	};

	// неверно, нужно просто начинать от фиксированного элемента Prompts
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
		walk(m_tree->topLevelItem(i));
	return parts.join(", ");
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int)
{
	if (!item) return;

	// Показываем актуальный собранный промпт при любом клике
	m_promptView->setPlainText(composePrompt());

	// Редактирование только для фрагментов с текстом
	const QVariant text = item->data(0, Qt::UserRole + 1);
	if (!text.isValid())
		return;

	bool ok = false;
	const QString newText = QInputDialog::getMultiLineText(
		this,
		"Edit fragment",
		item->text(0),
		text.toString(),
		&ok);

	if (ok) {
		item->setData(0, Qt::UserRole + 1, newText);
		m_promptView->setPlainText(composePrompt());
	}
}

void MainWindow::onGenerateClicked()
{
	const QString prompt = composePrompt();
	if (prompt.trimmed().isEmpty()) {
		QMessageBox::warning(this, "Empty prompt",
			"Include at least one segment containing text.");
		return;
	}

	GenerationTask task;
	task.taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
	task.prompt = composePrompt();
	task.params = composeParams();
	
	m_queueView->addItem(QString("Time [%1] %2  (repeat=%3, seed=%4, delta=%5)")
		.arg(task.taskId.left(8))
		.arg(task.prompt.left(60))
		.arg(task.params.repeatCount)
		.arg(task.params.baseSeed)
		.arg(task.params.seedDelta));

	emit enqueueTask(task);
}

void MainWindow::onQueueEmpty()
{
	m_generateBtn->setEnabled(true);


	m_queueView->addItem("generation finished");
//	if (success) {
//		m_queueView->addItem("Saved: " + filePath);
//	}
//	else {
//		m_queueView->addItem("Error: " + error);
//		QMessageBox::warning(this, "Generation error", error);
//	}
}
