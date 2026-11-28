#pragma once

// generationparams.h
struct GenerationParams {
    int repeatCount = 1;        // количество повторов
    qint64 baseSeed = -1;       // начальный seed, -1 означает "случайный для каждого изображения"
    qint64 seedDelta = 0;       // 0 = фиксированный seed, 1 = +1 каждый раз, любое значение = пользовательский шаг
	int width = 512;
	int height = 512;
	int steps = 20;
	double cfgScale = 7.0;
	QString initImagePath;   // пусто = txt2img
	QString maskImagePath;   // пусто = без маски (только для img2img)
	double  strength = 0.75; // denoising_strength для img2img
};

// generationtask.h
struct GenerationTask {
    QString posPrompt;
	QString negPrompt;
    GenerationParams params;
    QString taskId;             // для идентификации в очереди
};

// Постоянные настройки воркера — на весь сеанс
struct WorkerConfig {
	QString serverBase = "http://127.0.0.1:1234";
	QString outPath = "./output";
};
