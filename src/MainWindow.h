#pragma once

#include <QMainWindow>
#include <QThread>
#include "generation.h"
#include "Settings.h"

class QTreeWidget;
class QPlainTextEdit;
class QListWidget;
class QPushButton;
class GenerateWorker;
class QTreeWidgetItem;
class QSpinBox;
class QDoubleSpinBox;
class QGroupBox;
class PromptPanel;
class MaskEditorDialog;
class QLineEdit;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

signals:
	// этот сигнал посылаем сами себе из обработчика кнопки
	void enqueueTask(const GenerationTask &task);

protected:
	void closeEvent(QCloseEvent *e) override;

private slots:

	void onGenerateClicked();
	void onQueueEmpty();
	void openMaskEditor();
private:
	bool fileReadable(const QString &path) const;
	bool validateParams(const GenerationParams &p, QString *errorOut) const;
	void setupCentralLayout();
	
	QGroupBox *buildParamsGroup();
	QGroupBox *buildImg2ImgGroup();
	
	void initWidgetsFromDefaults();
	void collectDefaultsFromWidgets();
	void readParamsInto(GenerationParams &p) const;  // снять значения с виджетов

	Settings m_settings;

	PromptPanel *m_posPanel = nullptr;
	PromptPanel *m_negPanel = nullptr;

	MaskEditorDialog *m_maskDialog = nullptr;

	QListWidget *m_queueView;
	QPushButton *m_generateBtn;
	QPushButton *m_stopBtn;
	QPushButton *m_abortBtn;

	QThread m_workerThread;
	GenerateWorker *m_worker;

	// main params
	QSpinBox *m_repeatSpin = nullptr;
	QSpinBox *m_baseSeedSpin = nullptr;
	QSpinBox *m_deltaSpin = nullptr;
	QSpinBox *m_widthSpin = nullptr;
	QSpinBox *m_heightSpin = nullptr;
	QSpinBox *m_stepsSpin = nullptr;
	QDoubleSpinBox *m_cfgSpin = nullptr;

	// img2img params
	QLineEdit      *m_initEdit = nullptr;
	QPushButton    *m_initBrowseBtn = nullptr;
	QLineEdit      *m_maskEdit = nullptr;
	QPushButton    *m_maskBrowseBtn = nullptr;
	QDoubleSpinBox *m_strengthSpin = nullptr;
	QPushButton    *m_editMaskBtn = nullptr;
};
