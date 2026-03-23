#pragma once

#include <QString>
#include <QDateTime>

struct Lot {
    // itemId: идентификатор предмета (ссылка на `items.id` в БД).
    QString itemId;
    // startPrice: стартовая цена лота (raw данные с аукциона).
    qint64 startPrice = 0;
    // buyoutPrice: цена выкупа (0 если выкупа нет/неизвестно).
    qint64 buyoutPrice = 0;
    // startTime: время выставления лота (важно для отображения и расчётов).
    QDateTime startTime;
    // snapshotTime: когда мы получили/зафиксировали лот (используется как метка снапшота).
    QDateTime snapshotTime;
    // quality: tier качества (обычно -1 если API не вернул tier).
    int quality = -1;
};
