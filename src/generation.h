#pragma once

// generationparams.h
struct GenerationParams {
    int repeatCount = 1;        // количество повторов
    qint64 baseSeed = -1;       // начальный seed, -1 означает "случайный для каждого изображения"
    qint64 seedDelta = 0;       // 0 = фиксированный seed, 1 = +1 каждый раз, любое значение = пользовательский шаг
};

// generationtask.h
struct GenerationTask {
    QString prompt;
    GenerationParams params;
    QString taskId;             // для идентификации в очереди
};

