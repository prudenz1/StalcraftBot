#pragma once

#include <QString>
#include <QStringList>

struct Item {
    QString id;
    QString category;
    QString nameRu;
    int quality = -1;
    int trackingId = 0;
    bool hasTracking = false;

    bool isTracked() const { return trackingId > 0; }
    bool hasQualityTier() const {
        return category.startsWith(QStringLiteral("artefact/"))
            || category == QStringLiteral("weapon_modules/weapon_module")
            || category == QStringLiteral("weapon_modules/weapon_module_core");
    }

    static QString qualityName(int q) {
        static const QStringList names = {
            QStringLiteral("Обычный"),
            QStringLiteral("Необычный"),
            QStringLiteral("Особый"),
            QStringLiteral("Редкий"),
            QStringLiteral("Исключительный"),
            QStringLiteral("Легендарный")
        };
        if (q >= 0 && q < names.size()) return names[q];
        return QStringLiteral("Все");
    }

    QString displayName() const {
        if (quality >= 0)
            return nameRu + QStringLiteral(" [") + qualityName(quality) + QStringLiteral("]");
        return nameRu;
    }

    QString trackingKey() const {
        if (quality >= 0)
            return id + QStringLiteral("|") + QString::number(quality);
        return id + QStringLiteral("|-1");
    }
};
