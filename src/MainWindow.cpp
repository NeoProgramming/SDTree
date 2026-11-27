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
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <climits>

#include "PromptPanel.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	// регистрация типа для передачи
	qRegisterMetaType<GenerationTask>("GenerationTask");

	m_settings.loadSettings();

	setupCentralLayout();
	
	setWindowTitle("SD GUI");
	resize(1100, 700);

	initWidgetsFromDefaults();
	

	// ---- воркер в отдельном потоке ----
	m_worker = new GenerateWorker({ m_settings.serverUrl, m_settings.outPath }); // без parent, переедет в поток
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

	connect(m_generateBtn, &QPushButton::clicked,
		this, &MainWindow::onGenerateClicked);
}

MainWindow::~MainWindow()
{
	m_workerThread.quit();
	m_workerThread.wait();
}

void MainWindow::readParamsInto(GenerationParams &p) const
{
	p.repeatCount = m_repeatSpin->value();
	p.baseSeed = m_baseSeedSpin->value();
	p.seedDelta = m_deltaSpin->value();
	p.width = m_widthSpin->value();
	p.height = m_heightSpin->value();
	p.steps = m_stepsSpin->value();
	p.cfgScale = m_cfgSpin->value();
}

QGroupBox *MainWindow::buildParamsGroup()
{
	auto *box = new QGroupBox("Generation parameters", this);
	auto *grid = new QGridLayout(box);

	// --- repeatCount ---
	grid->addWidget(new QLabel("Repeats:"), 0, 0);
	m_repeatSpin = new QSpinBox(box);
	m_repeatSpin->setRange(1, 999);
	m_repeatSpin->setToolTip("How many images should be generated with this prompt?");
	grid->addWidget(m_repeatSpin, 0, 1);

	// --- baseSeed ---
	grid->addWidget(new QLabel("Seed (-1 = random):"), 1, 0);
	m_baseSeedSpin = new QSpinBox(box);
	m_baseSeedSpin->setRange(-1, INT_MAX);
	m_baseSeedSpin->setToolTip("-1: random seed for each image");
	grid->addWidget(m_baseSeedSpin, 1, 1);

	// --- seedDelta ---
	grid->addWidget(new QLabel("Seed step:"), 2, 0);
	m_deltaSpin = new QSpinBox(box);
	m_deltaSpin->setRange(-1000000, 1000000);
	m_deltaSpin->setToolTip("0: same seed; 1: +1 for each image");
	grid->addWidget(m_deltaSpin, 2, 1);

	// --- width / height ---
	grid->addWidget(new QLabel("Width:"), 3, 0);
	m_widthSpin = new QSpinBox(box);
	m_widthSpin->setRange(64, 4096);
	m_widthSpin->setSingleStep(8);
	grid->addWidget(m_widthSpin, 3, 1);

	grid->addWidget(new QLabel("Height:"), 4, 0);
	m_heightSpin = new QSpinBox(box);
	m_heightSpin->setRange(64, 4096);
	m_heightSpin->setSingleStep(8);
	grid->addWidget(m_heightSpin, 4, 1);

	// --- steps ---
	grid->addWidget(new QLabel("Steps:"), 5, 0);
	m_stepsSpin = new QSpinBox(box);
	m_stepsSpin->setRange(1, 150);
	grid->addWidget(m_stepsSpin, 5, 1);

	// --- cfgScale ---
	grid->addWidget(new QLabel("CFG scale:"), 6, 0);
	m_cfgSpin = new QDoubleSpinBox(box);
	m_cfgSpin->setRange(1.0, 30.0);
	m_cfgSpin->setDecimals(1);
	m_cfgSpin->setSingleStep(0.5);
	grid->addWidget(m_cfgSpin, 6, 1);

	grid->setColumnStretch(1, 1);
	return box;
}

void MainWindow::onGenerateClicked()
{
	GenerationTask task;
	
	task.posPrompt = m_posPanel->composeText().trimmed();
	if (task.posPrompt.trimmed().isEmpty()) {
		QMessageBox::warning(this, "Empty prompt",
			"Include at least one segment containing text.");
		return;
	}
	task.negPrompt = m_negPanel->composeText().trimmed();
	
	readParamsInto(task.params);   // снимок с виджетов — в задачу
	if (task.params.width % 8 != 0 || task.params.height % 8 != 0) {
		QMessageBox::warning(this, "Params",
			"width and height must be multiples of 8");
		return;
	}
	if (task.params.baseSeed < -1) {
		// -1 = random, меньше — бессмысленно
		QMessageBox::warning(this, "Params",
			"baseSeed must be greater than -1");
		return;
	}

	task.taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);	
	
	m_queueView->addItem(QString("Time [%1] %2  (repeat=%3, seed=%4, delta=%5)")
		.arg(task.taskId.left(8))
		.arg(task.posPrompt.left(60))
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

void MainWindow::initWidgetsFromDefaults()
{
	m_repeatSpin->setValue(m_settings.repeatCount);
	m_baseSeedSpin->setValue(static_cast<int>(m_settings.baseSeed));
	m_deltaSpin->setValue(static_cast<int>(m_settings.seedDelta));
	m_widthSpin->setValue(m_settings.width);
	m_heightSpin->setValue(m_settings.height);
	m_stepsSpin->setValue(m_settings.steps);
	m_cfgSpin->setValue(m_settings.cfgScale);
}

void MainWindow::collectDefaultsFromWidgets()
{
	m_settings.repeatCount = m_repeatSpin->value();
	m_settings.baseSeed = m_baseSeedSpin->value();
	m_settings.seedDelta = m_deltaSpin->value();
	m_settings.width = m_widthSpin->value();
	m_settings.height = m_heightSpin->value();
	m_settings.steps = m_stepsSpin->value();
	m_settings.cfgScale = m_cfgSpin->value();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	m_workerThread.quit();
	m_workerThread.wait();

	// копируем значения виджетов обратно в cfg (воркер уже не работает
	// по задачам, но чтобы не было гонки — делаем после остановки потока)

	collectDefaultsFromWidgets();
	m_settings.saveSettings();

	QMainWindow::closeEvent(e);
}

void MainWindow::setupCentralLayout()
{
	// --- left column: two PromptPanels stacked (positive on top, negative below) ---
	m_posPanel = new PromptPanel("Positive prompt", this);
	m_negPanel = new PromptPanel("Negative prompt", this);

	auto *leftSplitter = new QSplitter(Qt::Vertical, this);
	leftSplitter->addWidget(m_posPanel);
	leftSplitter->addWidget(m_negPanel);
	leftSplitter->setStretchFactor(0, 2);   // positive: 2/3 height
	leftSplitter->setStretchFactor(1, 1);   // negative: 1/3 height
	leftSplitter->setSizes({ 400, 200 });

	// --- right column: settings + buttons + queue/log ---
	auto *rightWidget = new QWidget(this);
	auto *rightLayout = new QVBoxLayout(rightWidget);

	rightLayout->addWidget(buildParamsGroup());          // settings group

	m_generateBtn = new QPushButton("Generate", rightWidget);
	m_stopBtn = new QPushButton("Stop", rightWidget);
	m_abortBtn = new QPushButton("Abort", rightWidget);
	auto *btnRow = new QHBoxLayout();
	btnRow->addWidget(m_generateBtn);
	btnRow->addWidget(m_stopBtn);
	btnRow->addWidget(m_abortBtn);
	rightLayout->addLayout(btnRow);

	rightLayout->addWidget(new QLabel("Queue / Log", rightWidget));
	m_queueView = new QListWidget(rightWidget);
	rightLayout->addWidget(m_queueView, 1);              // take remaining space

	// --- main horizontal splitter: left ~2/3, right ~1/3 ---
	auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
	mainSplitter->addWidget(leftSplitter);
	mainSplitter->addWidget(rightWidget);
	mainSplitter->setStretchFactor(0, 2);   // left 2/3 width
	mainSplitter->setStretchFactor(1, 1);   // right 1/3 width
	mainSplitter->setSizes({ 700, 350 });

	setCentralWidget(mainSplitter);
}
