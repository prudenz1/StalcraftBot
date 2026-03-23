#pragma once

#include <QString>
#include <QDateTime>

struct PriceSnapshot {
    qint64 id = 0;
    // itemId: ссылка на `items.id`.
    QString itemId;
    // quality: quality-tier, по которому агрегировали (или -1 для "Все").
    int quality = -1;
    // timestamp: метка времени снапшота (важна для истории и графиков).
    QDateTime timestamp;
    // minPrice/avgPrice/medianPrice/maxPrice: агрегаты по buyoutPrice за интервал.
    qint64 minPrice = 0;
    qint64 avgPrice = 0;
    qint64 medianPrice = 0;
    qint64 maxPrice = 0;
    // stdDev: стандартное отклонение по ценам.
    double stdDev = 0.0;
    // lotCount: количество лотов, которые попали в выборку.
    int lotCount = 0;
    // filteredCount: количество лотов после фильтрации выбросов (outliers).
    int filteredCount = 0;
};
