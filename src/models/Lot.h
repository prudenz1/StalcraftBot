#pragma once

#include <QString>
#include <QDateTime>

struct Lot {
    QString itemId;
    qint64 startPrice = 0;
    qint64 buyoutPrice = 0;
    QDateTime startTime;
    QDateTime snapshotTime;
};
