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
    QString itemId;
    QString itemName;
    AlertType type = AlertType::Watch;
    double rating = 0.0;
    QString message;
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
