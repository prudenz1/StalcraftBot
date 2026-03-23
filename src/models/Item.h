#pragma once

#include <QString>
#include <QStringList>

struct Item {
    QString id;
    QString category;
    QString nameRu;
    // `quality`: -1 означает "Все качества" (агрегированное/без выбора tier),
    // иначе 0..5 соответствует конкретному quality-tier.
    int quality = -1;
    // `trackingId`: id записи в таблице `tracked_items` (0 если трекинга нет).
    int trackingId = 0;
    // `hasTracking`: вспомогательный флаг, заполняется, например, в searchItems().
    bool hasTracking = false;

    // Удобная проверка: item уже отслеживается в БД.
    bool isTracked() const { return trackingId > 0; }

    // true, если для категории имеет смысл выбирать quality-tier (есть наборы artefact/weapon modules).
    bool hasQualityTier() const {
        return category.startsWith(QStringLiteral("artefact/"))
            || category == QStringLiteral("weapon_modules/weapon_module")
            || category == QStringLiteral("weapon_modules/weapon_module_core");
    }

    // Преобразует числовой tier в человекочитаемое название (и "Все" для -1/неизвестных).
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

    // Отображаемое имя предмета для UI: добавляет tier в скобках, если `quality >= 0`.
    QString displayName() const {
        if (quality >= 0)
            return nameRu + QStringLiteral(" [") + qualityName(quality) + QStringLiteral("]");
        return nameRu;
    }

    // Ключ для UI-комбобоксов: "itemId|quality".
    // Для `quality < 0` используется "-1", чтобы было возможно восстановить выбор.
    QString trackingKey() const {
        if (quality >= 0)
            return id + QStringLiteral("|") + QString::number(quality);
        return id + QStringLiteral("|-1");
    }
};
