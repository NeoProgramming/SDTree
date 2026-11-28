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
#include <QLineEdit>
#include <QFileDialog>
#include <climits>

#include "PromptPanel.h"
#include "maskededitordialog.h"

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
	m_worker = new GenerateWorker({ m_settings.serverBase, m_settings.outPath }); // без parent, переедет в поток
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

	p.initImagePath = m_initEdit->text().trimmed();
	p.maskImagePath = m_maskEdit->text().trimmed();
	p.strength = m_strengthSpin->value();
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

QGroupBox *MainWindow::buildImg2ImgGroup()
{
	auto *box = new QGroupBox("Image to image (optional)", this);
	auto *grid = new QGridLayout(box);

	// --- init image ---
	grid->addWidget(new QLabel("Init image:"), 0, 0);
	m_initEdit = new QLineEdit(box);
	m_initEdit->setPlaceholderText("(empty = txt2img)");
	m_initEdit->setToolTip("Reference image for img2img. Leave empty to use txt2img.");
	grid->addWidget(m_initEdit, 0, 1);

	m_initBrowseBtn = new QPushButton("...", box);
	m_initBrowseBtn->setFixedWidth(30);
	grid->addWidget(m_initBrowseBtn, 0, 2);

	// --- mask image ---
	grid->addWidget(new QLabel("Mask:"), 1, 0);
	m_maskEdit = new QLineEdit(box);
	m_maskEdit->setPlaceholderText("(optional, img2img only)");
	m_maskEdit->setToolTip("Optional mask for img2img. White areas are regenerated.");
	grid->addWidget(m_maskEdit, 1, 1);

	m_maskBrowseBtn = new QPushButton("...", box);
	m_maskBrowseBtn->setFixedWidth(30);
	grid->addWidget(m_maskBrowseBtn, 1, 2);

	// после m_maskBrowseBtn:
	m_editMaskBtn = new QPushButton("Edit", box);
	m_editMaskBtn->setFixedWidth(45);
	grid->addWidget(m_editMaskBtn, 1, 3);

	// --- strength ---
	grid->addWidget(new QLabel("Strength:"), 2, 0);
	m_strengthSpin = new QDoubleSpinBox(box);
	m_strengthSpin->setRange(0.0, 1.0);
	m_strengthSpin->setSingleStep(0.05);
	m_strengthSpin->setDecimals(2);
	m_strengthSpin->setToolTip("denoising_strength for img2img");
	grid->addWidget(m_strengthSpin, 2, 1, 1, 2);

	grid->setColumnStretch(1, 1);

	// --- browse handlers ---
	connect(m_initBrowseBtn, &QPushButton::clicked, this, [this]() {
		const QString f = QFileDialog::getOpenFileName(
			this, "Select init image", QString(),
			"Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!f.isEmpty())
			m_initEdit->setText(f);
	});

	connect(m_maskBrowseBtn, &QPushButton::clicked, this, [this]() {
		const QString f = QFileDialog::getOpenFileName(
			this, "Select mask image", QString(),
			"Images (*.png *.jpg *.jpeg *.bmp *.webp)");
		if (!f.isEmpty())
			m_maskEdit->setText(f);
	});


	connect(m_editMaskBtn, &QPushButton::clicked,
		this, &MainWindow::openMaskEditor);

	// --- mask edit enabled only when init set ---
	auto updateMaskEnabled = [this]() {
		const bool hasInit = !m_initEdit->text().trimmed().isEmpty();
		m_maskEdit->setEnabled(hasInit);
		m_maskBrowseBtn->setEnabled(hasInit);
		m_strengthSpin->setEnabled(hasInit);
		m_editMaskBtn->setEnabled(hasInit);
	};
	connect(m_initEdit, &QLineEdit::textChanged, this, updateMaskEnabled);
	updateMaskEnabled();

	return box;
}

bool MainWindow::fileReadable(const QString &path) const
{
	QFileInfo fi(path);
	return fi.exists() && fi.isFile() && fi.isReadable();
}

bool MainWindow::validateParams(const GenerationParams &p, QString *errorOut) const
{
	if (p.width % 8 != 0 || p.height % 8 != 0) {
		*errorOut = "Width and height must be multiples of 8.";
		return false;
	}
	if (p.width < 64 || p.height < 64) {
		*errorOut = "Width and height must be at least 64.";
		return false;
	}
	if (p.baseSeed < -1) {
		*errorOut = "Base seed must be -1 (random) or >= 0.";
		return false;
	}
	if (p.repeatCount < 1) {
		*errorOut = "Repeat count must be >= 1.";
		return false;
	}

	const bool hasInit = !p.initImagePath.isEmpty();
	const bool hasMask = !p.maskImagePath.isEmpty();

	if (hasInit && !fileReadable(p.initImagePath)) {
		*errorOut = QString("Init image is not readable: %1").arg(p.initImagePath);
		return false;
	}
	if (hasMask && !hasInit) {
		*errorOut = "Mask requires an init image (img2img mode).";
		return false;
	}
	if (hasMask && !fileReadable(p.maskImagePath)) {
		*errorOut = QString("Mask image is not readable: %1").arg(p.maskImagePath);
		return false;
	}
	if (p.strength < 0.0 || p.strength > 1.0) {
		*errorOut = "Strength must be in [0, 1].";
		return false;
	}
	return true;
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

	QString err;
	if (!validateParams(task.params, &err)) {
		QMessageBox::warning(this, "Invalid parameters", err);
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

	m_initEdit->setText(m_settings.initImagePath);
	m_maskEdit->setText(m_settings.maskImagePath);
	m_strengthSpin->setValue(m_settings.strength);
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
	rightLayout->addWidget(buildImg2ImgGroup());

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

void MainWindow::openMaskEditor()
{
	const QString refPath = m_initEdit->text().trimmed();
	if (refPath.isEmpty()) {
		QMessageBox::warning(this, "No reference image",
			"Set init image before editing mask.");
		return;
	}

	QImage ref(refPath);
	if (ref.isNull()) {
		QMessageBox::warning(this, "Cannot load image",
			QString("Failed to load: %1").arg(refPath));
		return;
	}

	QImage existingMask;
	const QString maskPath = m_maskEdit->text().trimmed();
	if (!maskPath.isEmpty())
		existingMask = QImage(maskPath);

	if (!m_maskDialog) {
		m_maskDialog = new MaskEditorDialog(this);
		connect(m_maskDialog, &MaskEditorDialog::maskSaved,
			this, [this](const QImage &mask) {
			const QString path = QFileDialog::getSaveFileName(
				this, "Save mask", QString(),
				"PNG image (*.png)");
			if (path.isEmpty())
				return;
			if (!mask.save(path, "PNG")) {
				QMessageBox::warning(this, "Save failed",
					QString("Cannot save mask to: %1").arg(path));
				return;
			}
			m_maskEdit->setText(path);
		});
	}

	m_maskDialog->loadImages(ref, existingMask);
	m_maskDialog->show();
	m_maskDialog->raise();
	m_maskDialog->activateWindow();
}
