#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <libpq-fe.h>

#include "models/Item.h"
#include "models/Lot.h"
#include "models/PriceSnapshot.h"
#include "models/Alert.h"

class Config;

class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(Config* config, QObject* parent = nullptr);
    ~Database();

    bool connect();
    void disconnect();
    bool isConnected() const;
    void migrate();

    // Items catalog
    bool upsertItem(const Item& item);
    bool upsertItems(const QVector<Item>& items);
    QVector<Item> allItems();
    QVector<Item> searchItems(const QString& query);

    // Tracking
    QVector<Item> trackedItems();
    bool addTracking(const QString& itemId, int quality);
    bool removeTracking(int trackingId);
    bool removeAllTracking(const QString& itemId);
    bool hasTracking(const QString& itemId);

    // Lot Snapshots
    bool insertLotSnapshots(const QString& itemId, const QVector<Lot>& lots);

    // Price Snapshots
    bool insertPriceSnapshot(const PriceSnapshot& snap);
    QVector<PriceSnapshot> priceHistory(const QString& itemId, int quality, int days);
    PriceSnapshot latestPriceSnapshot(const QString& itemId, int quality);

    // Hourly Stats
    bool upsertHourlyStat(const QString& itemId, int quality, int hour,
                          qint64 avgPrice, int sampleCount);
    QVector<std::pair<int, qint64>> hourlyStats(const QString& itemId, int quality);

    // Alerts
    bool insertAlert(const Alert& alert);
    QVector<Alert> recentAlerts(int limit = 100);

    // Helpers
    QString itemName(const QString& itemId);
    int daysSinceFirstSnapshot(const QString& itemId, int quality);
    double overallAvgPrice(const QString& itemId, int quality, int days);

signals:
    void alertInserted(const Alert& alert);

private:
    bool exec(const QString& query);
    PGresult* query(const QString& sql);
    void freeResult(PGresult* res);
    QString escape(const QString& str);
    void migrateFromV1();

    Config* m_config;
    PGconn* m_conn = nullptr;
};
