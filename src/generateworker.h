#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QImage>
#include <QDir>
#include <QDateTime>
#include <QQueue>
#include "generation.h"

class GenerateWorker : public QObject
{
    Q_OBJECT
public:
    explicit GenerateWorker(QObject *parent = nullptr);

public slots:
 //	void requestGeneration(const QString &prompt);
	void enqueueTask(const GenerationTask &task);
	void stopAfterCurrent();     // остановиться после завершения текущей
	void abortNow();             // немедленно прервать (отменить текущий QNetworkReply)


signals:
  //  void generationFinished(const QString &filePath, bool success, const QString &error);
	void taskStarted(const QString &taskId, int currentIndex, int total);
	void imageSaved(const QString &taskId, const QString &filePath, qint64 usedSeed);
	void taskFinished(const QString &taskId, bool success, const QString &error);
	void queueEmpty();
private slots:
	void processNext();
private:
	struct RunningState {
		GenerationTask task;
		int currentIndex = 0;
		qint64 currentSeed = -1;
		QString outputBase;      // базовое имя файла без .N.png
	};

	QQueue<GenerationTask> m_queue;
	RunningState m_running;
	bool m_hasRunning = false;
	bool m_stopRequested = false;
	QNetworkReply *m_currentReply = nullptr;
    QNetworkAccessManager *m_nam;
    QString m_outputDir;

	void startNextRequest();
	void handleReply(QNetworkReply *reply);
	QString makeBaseName() const;
    QString makeOutputPath(int index) const;
	qint64 computeSeedForIndex(int index) const;
};
