#include "generateworker.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QBuffer>
#include <QTimer>


GenerateWorker::GenerateWorker(const WorkerConfig &cfg, QObject *parent)
    : QObject(parent)
	, m_cfg(cfg)
    , m_nam(new QNetworkAccessManager(this))
{
	if(m_cfg.outPath.isEmpty())
		m_cfg.outPath = QDir::current().filePath("output");
    QDir().mkpath(m_cfg.outPath);
}

GenerateWorker::~GenerateWorker()
{
	
}

QString GenerateWorker::makeBaseName() const
{
	// напр. "output/20260101_120000_123"
	const QString base = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
	QString path = QDir(m_cfg.outPath).filePath(base);
	return path;
}

QString GenerateWorker::makeOutputPath(int index) const
{
    // Простая защита от затирания: имя с временной меткой и счётчиком
	// outputBase + "." + QString::number(index + 1) + ".png"

	const QString base = makeBaseName(); 
	QString path;
    int counter = 1;
	if (index < 0) {
		path = base + ".png";
		while (QFile::exists(path)) {
			path = QString("%1_%2.png").arg(base).arg(counter++);
		}
	}
	else {
		path = base + QString(".%1.png").arg(index);
		while (QFile::exists(path)) {
			path = QString("%1_%2.%d.png").arg(base).arg(counter++).arg(index);
		}
	}
    return path;
}


void GenerateWorker::enqueueTask(const GenerationTask &task) 
{
	m_queue.enqueue(task);
	if (!m_hasRunning && !m_stopRequested) {
		processNext();
	}
}

void GenerateWorker::processNext() 
{
	if (m_queue.isEmpty()) {
		m_hasRunning = false;
		emit queueEmpty();
		return;
	}
	if (m_stopRequested) {
		m_hasRunning = false;
		return;
	}

	m_running.task = m_queue.dequeue();
	m_running.currentIndex = m_running.task.params.repeatCount ? 0 : -1;
	m_running.outputBase = makeBaseName();  // напр. "output/20260101_120000_123"
	m_hasRunning = true;

	emit taskStarted(m_running.task.taskId, 0, m_running.task.params.repeatCount);
	startNextRequest();
}

void GenerateWorker::startNextRequest() 
{
	if (m_running.currentIndex >= m_running.task.params.repeatCount) {
		// серия завершена
		emit taskFinished(m_running.task.taskId, true, QString());
		m_hasRunning = false;
		processNext();
		return;
	}

	// вычисляем сид
	m_running.currentSeed = computeSeedForIndex(m_running.currentIndex);

	QJsonObject body;
	body["prompt"] = m_running.task.posPrompt;
	body["negative_prompt"] = m_running.task.negPrompt;
	body["width"] = m_running.task.params.width;
	body["height"] = m_running.task.params.height;
	body["steps"] = m_running.task.params.steps;
	body["cfg_scale"] = m_running.task.params.cfgScale;
	body["seed"] = m_running.currentSeed;
	body["batch_size"] = 1;

	QNetworkRequest req{ QUrl(m_cfg.serverUrl) };
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	// отправляет на сервер запрос
	m_currentReply = m_nam->post(req, QJsonDocument(body).toJson());

	// по завершении - принимаем и обрабатываем запрос
	connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
		handleReply(m_currentReply);
		m_currentReply->deleteLater();
		m_currentReply = nullptr;
	});
}

qint64 GenerateWorker::computeSeedForIndex(int index) const 
{
	const auto &p = m_running.task.params;
	if (p.baseSeed == -1) 
		return -1;
	return p.baseSeed + static_cast<qint64>(index) * p.seedDelta;
}

// прием ответа с сервера, 
void GenerateWorker::handleReply(QNetworkReply *reply) 
{
	if (reply->error() != QNetworkReply::NoError) {
		emit taskFinished(m_running.task.taskId, false, reply->errorString());
		m_hasRunning = false;
		processNext();
		return;
	}

	// принимаем ответ с сервера
	const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
	const QJsonArray images = doc.object().value("images").toArray();
	if (images.isEmpty()) {
		emit taskFinished(m_running.task.taskId, false, "пустой ответ images");
		m_hasRunning = false;
		processNext();
		return;
	}

	// извлечь фактически использованный seed из info (формат JSON-строки)
	qint64 usedSeed = m_running.currentSeed;
	const QString infoStr = doc.object().value("info").toString();
	if (!infoStr.isEmpty()) {
		const QJsonDocument infoDoc = QJsonDocument::fromJson(infoStr.toUtf8());
		// структура info зависит от реализации сервера, общий путь: info.seed
		if (infoDoc.isObject() && infoDoc.object().contains("seed")) {
			usedSeed = static_cast<qint64>(infoDoc.object().value("seed").toDouble());
		}
	}

	// 
	const QString path = makeOutputPath(m_running.task.params.repeatCount > 1 ? m_running.currentIndex : -1);
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly) ||
		f.write(QByteArray::fromBase64(images.at(0).toString().toUtf8())) < 0) {
		emit taskFinished(m_running.task.taskId, false, "ошибка записи файла");
		m_hasRunning = false;
		processNext();
		return;
	}
	f.close();

	emit imageSaved(m_running.task.taskId, path, usedSeed);

	++m_running.currentIndex;
	emit taskStarted(m_running.task.taskId, m_running.currentIndex,
		m_running.task.params.repeatCount);

	// Ключевое: отложенный запуск следующего запроса, а не прямой вызов
	QTimer::singleShot(0, this, &GenerateWorker::startNextRequest);
	//startNextRequest();
}

void GenerateWorker::stopAfterCurrent() 
{ 
	m_stopRequested = true; 
}

void GenerateWorker::abortNow() 
{
	m_stopRequested = true;
	if (m_currentReply) {
		m_currentReply->abort();  // вызовет finished, handleReply обнаружит ошибку
	}
	m_queue.clear();
}

/*
void GenerateWorker::requestGeneration(const QString &prompt)
{
	static const int kTimeout = 600000; // 10 минут

    QJsonObject body;
    body["prompt"] = prompt;
    body["negative_prompt"] = "";
    body["width"] = kWidth;
    body["height"] = kHeight;
    body["steps"] = kSteps;
    body["cfg_scale"] = kCfgScale;
    body["seed"] = -1;
    body["batch_size"] = 1;

    QNetworkRequest req{QUrl(kServerUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());

    // Синхронное ожидание внутри потока-воркера (не блокирует GUI)
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Таймаут: генерация может быть долгой, ставим с запасом
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(kTimeout); 

    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        // сработал таймаут
        reply->abort();
        reply->deleteLater();
        emit generationFinished(QString(), false, "Таймаут ожидания ответа сервера");
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        reply->deleteLater();
        emit generationFinished(QString(), false, err);
        return;
    }

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit generationFinished(QString(), false,
            "Не удалось разобрать JSON ответа: " + parseErr.errorString());
        return;
    }

    const QJsonArray images = doc.object().value("images").toArray();
    if (images.isEmpty()) {
        emit generationFinished(QString(), false, "Ответ не содержит изображений");
        return;
    }

    const QByteArray png = QByteArray::fromBase64(
        images.at(0).toString().toUtf8());
    if (png.isEmpty()) {
        emit generationFinished(QString(), false, "Base64 изображения пуст");
        return;
    }

    const QString outPath = makeOutputPath();
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit generationFinished(QString(), false,
            "Не удалось открыть файл для записи: " + outPath);
        return;
    }
    f.write(png);
    f.close();

    emit generationFinished(outPath, true, QString());
}
*/


