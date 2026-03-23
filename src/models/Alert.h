#pragma once

#include <QString>
#include <QDateTime>

enum class AlertType {
    Watch,
    Buy,
    CheapLot
};

struct Alert {
    qint64 id = 0;
    // itemId: ссылка на предмет (items.id).
    QString itemId;
    // quality: выбранный quality-tier (или -1, если алерт агрегированный/без tier).
    int quality = -1;
    // itemName: человекочитаемое имя предмета (может быть пустым).
    QString itemName;
    // type: тип алерта (BUY/WATCH/CHEAP_LOT).
    AlertType type = AlertType::Watch;
    // rating: численный рейтинг/оценка (используется в UI).
    double rating = 0.0;
    // message: текст уведомления.
    QString message;
    // createdAt: время создания алерта на сервере.
    QDateTime createdAt;

    static QString typeToString(AlertType t) {
        switch (t) {
            case AlertType::Watch:    return "WATCH";
            case AlertType::Buy:      return "BUY";
            case AlertType::CheapLot: return "CHEAP_LOT";
        }
        return "UNKNOWN";
    }

    static AlertType stringToType(const QString& s) {
        if (s == "WATCH")     return AlertType::Watch;
        if (s == "BUY")       return AlertType::Buy;
        if (s == "CHEAP_LOT") return AlertType::CheapLot;
        return AlertType::Watch;
    }
};
