#include "generateworker.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QBuffer>
#include <QTimer>
#include <QTextStream>

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
	const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
	QString base = QDir(m_cfg.outPath).filePath(stamp);

	// Защита от затирания: если base уже занят sdp или .0.png,
	// добавляем суффикс _1, _2, ...
	int counter = 1;
	const QString original = base;
	while (QFile::exists(base + ".sdp") || QFile::exists(base + ".0.png")) {
		base = QString("%1_%2").arg(original).arg(counter++);
	}
	return base;
}

QString GenerateWorker::makeOutputPath(int index) const
{
	// m_running.outputBase — уже зафиксирован в processNext(),
	// здесь только добавляем номер в серии
	if (index < 0) {
		return m_running.outputBase + ".png";
	}
	return QString("%1.%2.png").arg(m_running.outputBase).arg(index);
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

QString GenerateWorker::fileToBase64(const QString &path) const
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return QString();
	return QString::fromLatin1(f.readAll().toBase64());
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

	const bool useImg2Img = !m_running.task.params.initImagePath.isEmpty();

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

	QString endpoint = m_cfg.serverBase;

	if (useImg2Img) {
		endpoint += "/sdapi/v1/img2img"; 

		const QString initB64 = fileToBase64(m_running.task.params.initImagePath);
		if (initB64.isEmpty()) {
			// ошибка: не удалось прочитать файл
			emit taskFinished(m_running.task.taskId, false,
				"Cannot read init image: " + m_running.task.params.initImagePath);
			m_hasRunning = false;
			processNext();
			return;
		}
		body["init_images"] = QJsonArray{ initB64 };
		body["denoising_strength"] = m_running.task.params.strength;

		if (!m_running.task.params.maskImagePath.isEmpty()) {
			const QString maskB64 = fileToBase64(m_running.task.params.maskImagePath);
			if (!maskB64.isEmpty())
				body["mask"] = maskB64;
			// если маска не читается — молча идём без неё, или логируем предупреждение
		}
	}
	else {
		endpoint += "/sdapi/v1/txt2img";
	}

	QNetworkRequest req{ QUrl(endpoint) };
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

	// после успешного f.write(...) и f.close():
	if (m_running.currentIndex == 0) {
		saveSdp(m_running.outputBase);
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

void GenerateWorker::saveSdp(const QString &basePath) const
{
	QFile f(basePath + ".sdp");
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
		return;
	const auto &p = m_running.task.params;
	QTextStream out(&f);
	out << "positive: " << m_running.task.posPrompt << "\n";
	out << "negative: " << m_running.task.negPrompt << "\n";
	out << "width: " << p.width << "\n";
	out << "height: " << p.height << "\n";
	out << "steps: " << p.steps << "\n";
	out << "cfgScale: " << p.cfgScale << "\n";
	out << "repeatCount: " << p.repeatCount << "\n";
	out << "baseSeed: " << p.baseSeed << "\n";
	out << "seedDelta: " << p.seedDelta << "\n";
	out << "mode: " << (p.initImagePath.isEmpty() ? "txt2img" : "img2img") << "\n";
	if (!p.initImagePath.isEmpty()) {
		out << "initImage: " << p.initImagePath << "\n";
		out << "maskImage: " << p.maskImagePath << "\n";
		out << "strength: " << p.strength << "\n";
	}
}



