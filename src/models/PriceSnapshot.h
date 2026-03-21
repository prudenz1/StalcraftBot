#pragma once

#include <QString>
#include <QDateTime>

struct PriceSnapshot {
    qint64 id = 0;
    QString itemId;
    QDateTime timestamp;
    qint64 minPrice = 0;
    qint64 avgPrice = 0;
    qint64 medianPrice = 0;
    qint64 maxPrice = 0;
    double stdDev = 0.0;
    int lotCount = 0;
    int filteredCount = 0;
};
